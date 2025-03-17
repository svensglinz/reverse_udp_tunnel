#ifndef UDP_REVERSE_MISC_H
#define UDP_REVERSE_MISC_H

#include <time.h>
#include <stdio.h>

#define ERROR (0)
#define INFO_1 (1)
#define INFO_2 (2)
#define DEBUG (3)

// change 10 to variable
extern int user_log_level;

#define LOG(level, fmt, ...) do { \
    if (level <= user_log_level) { \
        time_t now = time(NULL); \
        struct tm *t = localtime(&now); \
        fprintf(stdout, "[%04d-%02d-%02d %02d:%02d:%02d] " fmt "\n", \
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, \
            t->tm_hour, t->tm_min, t->tm_sec, ##__VA_ARGS__); \
        fflush(stdout); \
    } \
} while (0)

time_t get_seconds();

#endif
