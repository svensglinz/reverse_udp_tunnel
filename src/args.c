#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include "args.h"

#define DEFAULT_N 10
#define DEFAULT_I 15
#define DEFAULT_T 60
#define DEFAULT_LOG_LEVEL 2

int arg_to_int(char *arg) {

    char *end;
    long out = strtol(arg, &end, 10);
    if (*end != '\0') {
        printf("Invalid argument %s\n", arg);
        exit(1);
    }
    return out;
}

    // error checking on input later
char* arg_to_ip(char *arg) {
    int len = strlen(arg);
    char tmp[len + 1];
    strncpy(tmp, arg, len + 1);

    char* tok = strtok(tmp, ":");
    char* ip = malloc((strlen(tok) + 1) * sizeof(char));
    strncpy(ip, tok, strlen(tok));
    return ip;
}

int arg_to_port(char *arg) {
    int len = strlen(arg);
    char tmp[len + 1];
    strncpy(tmp, arg, len + 1);

    char* tok = strtok(tmp, ":");
    if (tok != NULL) {
        tok = strtok(NULL, ":");
    }

    int port = arg_to_int(tok);
    return port;
}

char* arg_to_str(char *arg) {
    size_t len = strlen(arg);
    char* dest;

    if ((dest = malloc((len + 1) * sizeof(char))) == NULL) {
        printf("error allocating memory\n");
        return NULL;
    }
    // do this in ip parsing as well !
    strncpy(dest, arg, len + 1);
    dest[len] = '\0';

    return dest;
}

const struct option longopts[] = {
    {"keepaliveInterval", required_argument, 0, 1},
    {"connectionTimeout", required_argument, 0, 2},
    {"logLevel", required_argument, 0, 3},
    {0, 0, 0, 0}
};

int long_idx;

struct args *parse_args(int argc, char** argv) {
     struct args* a = calloc(sizeof(struct args), 1);
    int opt;

    // do argument parsing here (--> still have to do int conversion where needed)
    while ((opt = getopt_long(argc, argv, "s:o:l:n:k:t:h", longopts, &long_idx)) != -1) {
        switch (opt) {
            case 1: a->keepalive_timeout = arg_to_int(optarg);
                break;
            case 2: a->udp_timeout = arg_to_int(optarg);
                break;
            case 3: a->log_level = arg_to_int(optarg);
                break;
            case 's': a->inside_addr = arg_to_ip(optarg), a->inside_port = arg_to_port(optarg);
                break;
            case 'o': a->outside_addr = arg_to_ip(optarg), a->outside_port = arg_to_port(optarg);
                break;
            case 'l': a->outside_listen = arg_to_int(optarg);
                break;
            case 'n': a->max_connections = arg_to_int(optarg);
                break;
            case 'k': a->secret = arg_to_str(optarg);
                break;
            case 'h': print_usage("udp-reverse");
                exit(EXIT_SUCCESS);
            case '?': print_usage("udp-reverse");
                exit(EXIT_FAILURE);
        }
    }
    return a;
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("OPTIONS:\n");
    printf("  -s HOST:PORT      (inside) Host and port which the inside client connects to (required)\n");
    printf("  -o HOST:PORT      (inside) Host and port of the outside server (required)\n");
    printf("  -l PORT           (outside) Port on which the outside server listens for connections (required)\n");
    printf("  -n NUMBER         (inside, outside) Maximum number of open client connections to the outside server (optional, default: 10)\n");
    printf("  -k SECRET         (inside, outside) Secret for the keepalive connection (optional, default: none)\n");
    printf("  -h                Display this help message\n");

    printf("\nLong options:\n");
    printf("  --keepaliveInterval SECONDS   (inside, outside) Interval for keepalive packages (in seconds) (optional, default: 15)\n");
    printf("  --connectionTimeout SECONDS   (inside) Timeout for UDP sockets (in seconds) (optional, default: 60)\n");
    printf("  --logLevel LEVEL              (inside, outside) Log level (optional, default: 2)\n");
}
