#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>

#include "inside.h"
#include "outside.h"
#include "args.h"
#include "misc.h"

#define DEFAULT_MAX_CONNECTIONS 20
#define DEFAULT_CONN_TIMEOUT 120
#define DEFAULT_KEEPALIVE_INTERVAL 30
#define DEFAULT_LOG_LEVEL 2
#define DEFAULT_SECRET "default"

int user_log_level;

int main(int argc, char** argv) {

    /* OPTIONS
     *
     * -s : HOST:PORT which the inside client connects to (required)
     * -o : HOST:PORT of the outside server (required)
     * -l : PORT on which the outside server listens for connections (optional, default: 7777)
     * -n : Maximum number of open client connections to the outside server (optional, default: 10)
     * -k : secret for the keepalive connection (optional, default: none)
     * --connectionTimeout : timeout for UDP sockets (in seconds) (optional, default: 60)
     * --keepaliveInterval : interval for keepalive packages (in seconds) (optional, default: 15)
     * --logLevel : Log level (optional, default: 2)
     */

   struct args *a = parse_args(argc, argv);
   // set log level
    if (a->log_level == 0) {
        user_log_level = DEFAULT_LOG_LEVEL;
    } else {
   user_log_level = a->log_level;
    }
   /*
    if (a->udp_timeout == 0) {
        a->udp_timeout = DEFAULT_T;
    }
    if (a->keepalive_timeout == 0) {
        a->keepalive_timeout = DEFAULT_I;
    }
    if (a->log_level == 0) {
        a->log_level = DEFAULT_LOG_LEVEL;
    }
    */

   if (a->keepalive_timeout == 0) {
       a->keepalive_timeout = DEFAULT_KEEPALIVE_INTERVAL;
    }

    if (a->max_connections == 0) {
            a->max_connections = DEFAULT_MAX_CONNECTIONS;
    }

    if (a->secret == NULL) {
        a->secret = DEFAULT_SECRET;
    }

   // start inside or outside tunnel
   // DEFINE STANDARF VALUES HERE TO CHECK IF SOME ARE NULL
    if (a->inside_addr && a->outside_addr && !a->outside_listen) {
        if (a->udp_timeout == 0) {
            a->udp_timeout = DEFAULT_CONN_TIMEOUT;
        }
        run_inside(a);
    } else if (a->outside_listen && !a->inside_addr && !a->outside_addr && !a->udp_timeout){
        run_outside(a);
    } else {
        print_usage("udp-reverse");
        exit(EXIT_FAILURE);
    }
}

