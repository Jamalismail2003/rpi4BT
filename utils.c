#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdarg.h>
#include "utils.h"


#define ANSI_RESET "\x1b[0m"
#define ANSI_BLACK "\x1b[30m"
#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_BLUE "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN "\x1b[36m"
#define ANSI_WHITE "\x1b[37m"

#define  INDENT_LEVEL 4

void display_bytes(const char *s, const unsigned char *p, int len)
{
#if 0
	if(len > 32)
		return;
#endif

printf("%s", ANSI_YELLOW);

    printf("%-12s  ",s);
    char buf[17];
    memset(buf,0,17);
    for (int i=0; i<len; i++)
    {
        if (i && i%16==0)
        {
            printf("   %s\n%-12s  ",buf,"");
            memset(buf,0,17);
        }
        buf[i % 16] = p[i]>' ' ? p[i] : '.';
        printf("%02x ",p[i]);
    }
    while (len % 16 != 0)
    {
        printf("   ");
        len++;
    }
    printf("   %s\n",buf);
printf("%s", ANSI_RESET);
}




void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("%s", ANSI_GREEN);
    vprintf(format, args);
    printf("%s\n", ANSI_RESET);
    va_end(args);
}

void log_warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("%s", ANSI_YELLOW);
    vprintf(format, args);
    printf("%s\n", ANSI_RESET);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("%s", ANSI_RED);
    vprintf(format, args);
    printf("%s\n", ANSI_RESET);
    va_end(args);
}

void log_indented(int indent_level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    for (int i = 0; i < indent_level; i++) {
        printf(" ");
    }
    printf("%s", ANSI_RESET);
    vprintf(format, args);
    printf("%s\n", ANSI_RESET);
    va_end(args);
}
