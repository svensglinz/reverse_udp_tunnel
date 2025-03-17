#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "args.h"
#include "conn_table.h"
#include "mac.h"
#include "hashmap.h"
#include "inside.h"
#include "misc.h"

#define BUFF_SIZE 0xffff

// global variables
static struct sockaddr_in outside_addr;
static const struct args *prog_args;
static struct conn_table_inside *conn_tbl;
static int epollfd;
static struct sockaddr_in service_addr;
pthread_mutex_t lock;

// creates a new non-blocking socket and adds it to
// the epoll instance referred to by `epollfd`
int create_and_register_client_socket(int epollfd) {

  // new non-blocking socket
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  int flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_in addr;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0; // allocate by OS
  addr.sin_family = AF_INET;

  if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG(ERROR, "creating socket for new outbound connection failed");
    exit(EXIT_FAILURE);
  }

  // register
  struct epoll_event e;
  e.events = EPOLLIN;
  e.data.fd = fd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &e) < 0) {
      LOG(ERROR, "registering newly created socket with epoll failed");
      exit(EXIT_FAILURE);
  }
  return fd;
}

int run_inside(struct args *args) {

  LOG(INFO_1, "Listening to tunnel connections from %s:%d", args->outside_addr, args->outside_port);
  LOG(INFO_1, "Forwarding traffic to %s:%d", args->inside_addr, args->inside_port);

  // initialize global variables
  pthread_mutex_init(&lock, NULL);
  epollfd = epoll_create1(0);
  prog_args = args;
  conn_tbl = conn_table_inside_init();

  // address of outside server
  if ((inet_pton(AF_INET, args->outside_addr, &outside_addr.sin_addr) <= 0)) {
    struct hostent *h = gethostbyname(args->outside_addr);
    if (h == NULL) {
      LOG(ERROR, "hostname %s could not be resolved", args->outside_addr);
      exit(EXIT_FAILURE);
    }
    memcpy(&outside_addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
  }
  outside_addr.sin_family = AF_INET;
  outside_addr.sin_port = htons(args->outside_port);

  // adddress of inside service
  service_addr.sin_family = AF_INET;
  service_addr.sin_port = htons(args->inside_port);
  if ((inet_pton(AF_INET, args->inside_addr, &service_addr.sin_addr.s_addr) <= 0)) {
    struct hostent *h = gethostbyname(args->inside_addr);
    if (h == NULL) {
      LOG(ERROR, "hostname %s could not be resolved", args->inside_addr);
      exit(EXIT_FAILURE);
    }
    memcpy(&service_addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
  }

  // create first spare tunnel that sends signals to the outside
  conn_tbl->free_tunnel = create_and_register_client_socket(epollfd);
  struct mac_t m;

  const int len_secret = args->secret == 0 ? 0 : strlen(args->secret);

  // setup epoll structure to monitor sockets
  struct epoll_event events[prog_args->max_connections];

  // starting keepalive thread
  pthread_t t_keepalive;
  pthread_create(&t_keepalive, 0, send_keepalive, 0);

  // starting cleanup thread
  pthread_t t_cleanup;
  pthread_create(&t_cleanup, 0, cleanup, 0);


  // listen to all sockets for incoming messages
  // messges are coming in either as
  // 1.) inbound trafffic from established connection from the outside
  // 2.) inbound traffic new connection from the outside via spare tunnel
  // 3.) outbound traffic from the service application
  char buffer[BUFF_SIZE];
  struct sockaddr_in client;
  socklen_t client_size;
  struct mac_t mac;
  int bytes_recv;

  while (1) {
    int nfds = epoll_wait(epollfd, events, prog_args->max_connections, -1);

    // lock as we need to modify the conn_table
    pthread_mutex_lock(&lock);
    for (int i = 0; i < nfds; i++) {
      if (events[i].events & EPOLLIN) {
        int sock = events[i].data.fd;

        ssize_t bytes_recv = recvfrom(sock, buffer, BUFF_SIZE, 0, (struct sockaddr *)&client, &client_size);
        printf("reveived %d bytes\n", bytes_recv);
        // traffic is from service application
        if (client.sin_addr.s_addr == service_addr.sin_addr.s_addr && client.sin_port == service_addr.sin_port) {
          //printf("sendint to outside\n");
            sendto(sock, buffer, bytes_recv, 0, (const struct sockaddr *)&outside_addr, sizeof(outside_addr));
            continue;
        }

        // traffic is from free tunnel -> must be from a new outside connection
        if (sock == conn_tbl->free_tunnel) {

            // add tunnel to the connection table
            conn_table_inside_add_fd(conn_tbl, sock);

            // create new free tunnel and send out ping message
            conn_tbl->free_tunnel = create_and_register_client_socket(epollfd);
            gen_mac(&mac, prog_args->secret);
            sendto(conn_tbl->free_tunnel, &mac, sizeof(mac), 0, (struct sockaddr *)&outside_addr, sizeof(outside_addr));
            LOG(DEBUG, "creating a new spare tunnel");
        }

        // send traffic to service application and update last connection
        conn_table_inside_update_last_ping(conn_tbl, sock);
        //printf("sending to service application\n");
        sendto(sock, buffer, bytes_recv, 0, (const struct sockaddr *)&service_addr, sizeof(service_addr));
    }
  }
      pthread_mutex_unlock(&lock);
  }
  close(epollfd);
  return 0;
}

// send periodic keepalives to all active tunnels + spare one
void *send_keepalive(void *args) {
  struct mac_t mac;
  int r;
  while (1) {
    struct hashmap *map = conn_tbl->fd_to_time;

    // first ping current free element
    gen_mac(&mac, prog_args->secret);
    sendto(conn_tbl->free_tunnel, &mac, sizeof(mac), 0, (const struct sockaddr *)&outside_addr, sizeof(outside_addr));

    // put in external function
    // send packages to all active connections
    for (int i = 0; i < map->len; i++) {
      struct hash_node *cur = map->elems[i];
      while (cur != NULL) {
        struct map_fd_time* t = (struct map_fd_time *)cur->elem;
        int fd = t->key;

        gen_mac(&mac, prog_args->secret);
        sendto(fd, &mac, sizeof(mac), 0, (const struct sockaddr *)&outside_addr, sizeof(outside_addr));
        //printf("sending keepalive\n");
        // wait 50ms until sending out next ping to reduce probability of packages
        // arriving out of order
        cur = cur->next;
        usleep(100 * 1000);
      }
    }
    // effectively ~ keepalive_timeout + 50ms * #connections (assumed negligible here for reasonable N. connections)
    sleep(prog_args->keepalive_timeout);
  }
}

// periodically close unused tunnels
void *cleanup(void *args) {
  while (1) {
    pthread_mutex_lock(&lock);
    conn_table_inside_clean(conn_tbl, epollfd, prog_args->udp_timeout);
    pthread_mutex_unlock(&lock);
    sleep(prog_args->udp_timeout / 5); // max 20% error tolerance
  }
}
