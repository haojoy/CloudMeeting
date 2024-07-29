#include <errno.h>
#include "unp.h"
#include <stdarg.h>

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

#ifdef USE_COLOR
    #define COLOR_ERROR RED
    #define COLOR_INFO GREEN
#else
    #define COLOR_ERROR ""
    #define COLOR_INFO ""
#endif

static void err_doit(const char *file, int line, const char *func, LOGLEVEL level, const char *fmt, va_list ap)
{
    char buf[MAXLINE];

    char timebuf[64];
    time_t t;
    struct tm *tm_info;
    // Get the current time
    time(&t);
    tm_info = localtime(&t);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    vsnprintf(buf, MAXLINE - 1, fmt, ap);
    if(level == CRITICAL){
        snprintf(buf + strlen(buf), MAXLINE - 1 - strlen(buf), ": %s", strerror(errno));
    }
    strcat(buf, "\n");

    if (level == CRITICAL || level == ERROR) {
        fprintf(stdout, "%s[%s]%s%5s%s: in [file:]%12s, [line:]%4d, [function:]%14s: %s", 
                COLOR_ERROR, timebuf, COLOR_ERROR, "Error", RESET, file, line, func, buf);
        fflush(stdout);
    } else {
        fprintf(stdout, "%s[%s]%s%5s%s: in [file:]%12s, [line:]%4d, [function:]%14s: %s", 
                COLOR_INFO, timebuf, COLOR_INFO, "Info", RESET, file, line, func, buf);
        fflush(stdout);
    }
}


void err_quit(const char *file, int line, const char *func, LOGLEVEL level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    err_doit(file, line, func, level, fmt, ap);
    va_end(ap);
    exit(1);
}

void err_msg(const char *file, int line, const char *func, LOGLEVEL level,const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    err_doit(file, line, func, level, fmt, ap);
    va_end(ap);
}

void convert_to_hex_string(char *sendbuf, size_t len, char *output, size_t output_size) 
{
    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        pos += snprintf(output + pos, output_size - pos, "%02X ", (unsigned char)sendbuf[i]);
    }
}

void ipConvertToString(uint32_t ip, char *ip_str, size_t size) {
    struct in_addr ip_addr;
    ip_addr.s_addr = ip;
    // Convert IP address to string
    snprintf(ip_str, size, "%s", inet_ntoa(ip_addr));
}