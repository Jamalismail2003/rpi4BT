#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define REPORT_SIZE (12 * 40)

static void display_bytes(const char *prefix, const unsigned char *data, int len)
{
    printf("%-12s  ", prefix);
    char ascii[17];
    memset(ascii, 0, sizeof(ascii));

    for (int i = 0; i < len; ++i) {
        if (i && i % 16 == 0) {
            printf("   %s\n%-12s  ", ascii, "");
            memset(ascii, 0, sizeof(ascii));
        }
        ascii[i % 16] = data[i] > ' ' ? (char)data[i] : '.';
        printf("%02x ", data[i]);
    }

    while (len % 16 != 0) {
        printf("   ");
        ++len;
    }
    printf("   %s\n", ascii);
}

int main(void)
{
    uint8_t buf_read[REPORT_SIZE];
    bool running = true;

    int btfd = open("/dev/rpiHID", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (btfd == -1) {
        perror("open /dev/rpiHID");
        return 1;
    }

    while (running) {
        fd_set rfd;
        struct timeval tv;

        FD_ZERO(&rfd);
        FD_SET(btfd, &rfd);
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int status = select(btfd + 1, &rfd, NULL, NULL, &tv);
        switch (status) {
        case -1:
            if (errno != EINTR) {
                perror("select");
                running = false;
            }
            break;
        case 0:
            break;
        default:
            if (FD_ISSET(btfd, &rfd)) {
                int read_bytes = (int)read(btfd, buf_read, sizeof(buf_read));
                if (read_bytes > 0) {
                    display_bytes("[HID]", buf_read, read_bytes);
                }
            }
            break;
        }
    }

    close(btfd);
    return 0;
}
