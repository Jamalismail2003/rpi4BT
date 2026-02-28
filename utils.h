#ifndef __UTILS_H
#define __UTILS_H


void display_bytes(const char *s, const unsigned char *p, int len);

void log_info(const char *format, ...);
void log_warning(const char *format, ...);
void log_error(const char *format, ...);
void log_indented(int indent_level, const char *format, ...);
unsigned systime_ms(void);

#endif
