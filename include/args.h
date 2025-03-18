#ifndef UDP_REVERSE_ARGS_H
#define UDP_REVERSE_ARGS_H

struct args {
        char *inside_addr;
        int inside_port;
        char *outside_addr;
        int outside_port;
        int outside_listen;
        int max_connections;
        char *secret;
        int udp_timeout;
        char keepalive_timeout;
        int log_level;
};

int arg_to_int(char *arg);
char* arg_to_ip(char *arg);
int arg_to_port(char *arg);
void print_usage(const char *program_name);
struct args *parse_args(int argc, char** argv);

#endif
