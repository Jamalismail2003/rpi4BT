#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "transport.h"
#include "utils.h"

#ifndef CCTS_OFLOW
#define CCTS_OFLOW 0x00010000
#endif
#ifndef CRTS_IFLOW
#define CRTS_IFLOW 0x00020000
#endif
#ifndef CRTSCTS
#define CRTSCTS (CCTS_OFLOW | CRTS_IFLOW)
#endif

#define TX_QUEUE_DEPTH 16
#define TX_MAX_PKT     260

typedef struct {
    uint8_t data[TX_MAX_PKT];
    size_t len;
} tx_item_t;

typedef struct {
    int fd;
    int stop_rx;
    int stop_tx;
    int got_reset_ack;

    pthread_t rx_thread;
    pthread_t tx_thread;

    pthread_mutex_t lock;
    pthread_cond_t ack_cv;

    pthread_mutex_t tx_lock;
    pthread_cond_t tx_cv;
    tx_item_t txq[TX_QUEUE_DEPTH];
    unsigned tx_head;
    unsigned tx_tail;
    unsigned tx_count;

    transport_context_t *transport;
} app_ctx_t;

static app_ctx_t ctx;
#if 0
static void hexdump(const uint8_t *buf, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
}
#endif
static void dump_modem_bits(int fd)
{
    int mstat = 0;
    if (ioctl(fd, TIOCMGET, &mstat) == -1) {
        perror("TIOCMGET");
        return;
    }

    printf("TIOCMGET=0x%X  CTS=%s RTS=%s DTR=%s DSR=%s CAR=%s\n",
           mstat,
           (mstat & TIOCM_CTS) ? "HIGH" : "LOW",
           (mstat & TIOCM_RTS) ? "HIGH" : "LOW",
           (mstat & TIOCM_DTR) ? "HIGH" : "LOW",
           (mstat & TIOCM_DSR) ? "HIGH" : "LOW",
           (mstat & TIOCM_CAR) ? "HIGH" : "LOW");
}

static void *rx_thread_fn(void *arg)
{
    app_ctx_t *ctx = (app_ctx_t *)arg;
    uint8_t tmp[64];

    pthread_setname_np(pthread_self(), "BT Rx Handler");

    while (1) {
        int n;

        pthread_mutex_lock(&ctx->lock);
        if (ctx->stop_rx) {
            pthread_mutex_unlock(&ctx->lock);
            break;
        }
        pthread_mutex_unlock(&ctx->lock);

        n = read(ctx->fd, tmp, sizeof(tmp));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            perror("rx read");
            continue;
        }
        if (n == 0) {
            continue;
        }

        //printf("RX %d bytes: ", n);
        //hexdump(tmp, n);

        for (int i = 0; i < n; i++) {
            Receive(ctx->transport, tmp[i]);
        }


#if 0
        if (acc_len + (size_t)n > sizeof(acc)) {
            if ((size_t)n >= sizeof(acc)) {
                memcpy(acc, tmp + ((size_t)n - sizeof(acc)), sizeof(acc));
                acc_len = sizeof(acc);
            } else {
                size_t drop = acc_len + (size_t)n - sizeof(acc);
                memmove(acc, acc + drop, acc_len - drop);
                acc_len -= drop;
                memcpy(acc + acc_len, tmp, (size_t)n);
                acc_len += (size_t)n;
            }
        } else {
            memcpy(acc + acc_len, tmp, (size_t)n);
            acc_len += (size_t)n;
        }

        if (contains_reset_ack(acc, acc_len)) {
            pthread_mutex_lock(&ctx->lock);
            ctx->got_reset_ack = 1;
            pthread_cond_broadcast(&ctx->ack_cv);
            pthread_mutex_unlock(&ctx->lock);
        }
#endif
    }

    return NULL;
}

static void *tx_thread_fn(void *arg)
{
    app_ctx_t *ctx = (app_ctx_t *)arg;
    pthread_setname_np(pthread_self(), "BT Tx Handler");

    while (1) {
        tx_item_t item;
        int have_item = 0;

        pthread_mutex_lock(&ctx->tx_lock);
        while (!ctx->stop_tx && ctx->tx_count == 0) {
            pthread_cond_wait(&ctx->tx_cv, &ctx->tx_lock);
        }
        if (ctx->stop_tx) {
            printf("DEBUG: Tx thread stopping\n");
            pthread_mutex_unlock(&ctx->tx_lock);
            break;
        }
        if (ctx->tx_count > 0) {
            item = ctx->txq[ctx->tx_head];
            ctx->tx_head = (ctx->tx_head + 1U) % TX_QUEUE_DEPTH;
            ctx->tx_count--;
            have_item = 1;
        }
        pthread_mutex_unlock(&ctx->tx_lock);

        if (have_item) {
            ssize_t wr = write(ctx->fd, item.data, item.len);
            if (wr != (ssize_t)item.len) {
                perror("tx write");
            }
        }
    }
    return NULL;
}

int serial_send(transport_context_t *transport, const uint8_t *data, size_t length)
{
    app_ctx_t *ctx = (app_ctx_t *)transport->serial_ctx;

    if (length > TX_MAX_PKT) {
        log_error("Data length exceeds maximum packet size");
        return -1;
    }

    pthread_mutex_lock(&ctx->tx_lock);
    if (ctx->tx_count == TX_QUEUE_DEPTH) {
        pthread_mutex_unlock(&ctx->tx_lock);
        log_error("TX queue is full");
        return -1;
    }

    tx_item_t *item = &ctx->txq[ctx->tx_tail];
    memcpy(item->data, data, length);
    item->len = length;
    ctx->tx_tail = (ctx->tx_tail + 1U) % TX_QUEUE_DEPTH;
    ctx->tx_count++;
    //printf("DEBUG:  Enqueued %zu bytes for transmission\n", length);
    pthread_cond_signal(&ctx->tx_cv);
    pthread_mutex_unlock(&ctx->tx_lock);

    //printf("DEBUG: ctx->tx_count = %u\n", ctx->tx_count);
    return length;
}

int serial_init(transport_context_t *transport)
{
    printf("DEBUG: Initializing serial transport\n");

    const char *devnode = "/dev/ser11";
    int fd;
    int bits;
    int rc;
    struct termios tio;

    memset(&ctx, 0, sizeof(ctx));
    ctx.transport = transport;
    transport->serial_ctx = &ctx;

    fd = open(devnode, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (tcgetattr(fd, &tio) < 0) {
        perror("tcgetattr");
        close(fd);
        return 1;
    }

    cfmakeraw(&tio);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1; /* 100 ms */
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8 | CRTSCTS);
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);

    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        perror("tcsetattr");
        close(fd);
        return 1;
    }

    tcflush(fd, TCIOFLUSH);

    bits = TIOCM_RTS;
    (void)ioctl(fd, TIOCMBIS, &bits);
    bits = TIOCM_DTR;
    (void)ioctl(fd, TIOCMBIS, &bits);

    ctx.fd = fd;
    pthread_mutex_init(&ctx.lock, NULL);
    pthread_cond_init(&ctx.ack_cv, NULL);
    pthread_mutex_init(&ctx.tx_lock, NULL);
    pthread_cond_init(&ctx.tx_cv, NULL);

    rc = pthread_create(&ctx.rx_thread, NULL, rx_thread_fn, &ctx);
    if (rc != 0) {
        errno = rc;
        perror("pthread_create(rx)");
        pthread_cond_destroy(&ctx.tx_cv);
        pthread_mutex_destroy(&ctx.tx_lock);
        pthread_cond_destroy(&ctx.ack_cv);
        pthread_mutex_destroy(&ctx.lock);
        close(fd);
        return 1;
    }
    rc = pthread_create(&ctx.tx_thread, NULL, tx_thread_fn, &ctx);
    if (rc != 0) {
        errno = rc;
        perror("pthread_create(tx)");
        pthread_mutex_lock(&ctx.lock);
        ctx.stop_rx = 1;
        pthread_mutex_unlock(&ctx.lock);
        pthread_mutex_lock(&ctx.tx_lock);
        ctx.stop_tx = 1;
        pthread_cond_broadcast(&ctx.tx_cv);
        pthread_mutex_unlock(&ctx.tx_lock);
        pthread_join(ctx.rx_thread, NULL);
        pthread_cond_destroy(&ctx.tx_cv);
        pthread_mutex_destroy(&ctx.tx_lock);
        pthread_cond_destroy(&ctx.ack_cv);
        pthread_mutex_destroy(&ctx.lock);
        close(fd);
        return 1;
    }

    printf("Using %s at 115200 8N1 with RTS/CTS enabled\n", devnode);
    dump_modem_bits(fd);

#if 1
    // TODO: create a cleaup function
//    pthread_join(ctx.rx_thread, NULL);
//    pthread_join(ctx.tx_thread, NULL);

    return 0;
#else

    /* Main thread sends reset directly. */
    printf("Sending HCI Reset...\n");
    hexdump(hci_reset, (int)sizeof(hci_reset));
sleep(2);
    if (write(fd, hci_reset, sizeof(hci_reset)) != (ssize_t)sizeof(hci_reset)) {
        perror("write");
        exit_code = 1;
        goto done;
    }

    tcdrain(fd);
    printf("Waiting up to 2 seconds for HCI event...\n");

    pthread_mutex_lock(&ctx.lock);
    if (!ctx.got_reset_ack) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        while (!ctx.got_reset_ack) {
            int prc = pthread_cond_timedwait(&ctx.ack_cv, &ctx.lock, &ts);
            if (prc == ETIMEDOUT) {
                break;
            }
        }
    }
    if (!ctx.got_reset_ack) {
        pthread_mutex_unlock(&ctx.lock);
        printf("No response (timeout)\n");
        exit_code = 1;
        goto done;
    }
    pthread_mutex_unlock(&ctx.lock);

    printf("HCI Reset ACK OKK\n");

done:
    pthread_mutex_lock(&ctx.lock);
    ctx.stop_rx = 1;
    pthread_cond_broadcast(&ctx.ack_cv);
    pthread_mutex_unlock(&ctx.lock);

    pthread_mutex_lock(&ctx.tx_lock);
    ctx.stop_tx = 1;
    pthread_cond_broadcast(&ctx.tx_cv);
    pthread_mutex_unlock(&ctx.tx_lock);

    pthread_join(ctx.rx_thread, NULL);
    pthread_join(ctx.tx_thread, NULL);
    pthread_cond_destroy(&ctx.ack_cv);
    pthread_mutex_destroy(&ctx.lock);
    pthread_cond_destroy(&ctx.tx_cv);
    pthread_mutex_destroy(&ctx.tx_lock);
    close(fd);
    return exit_code;
#endif
}
