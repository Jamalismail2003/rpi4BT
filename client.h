#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "hci.h"
#include "l2cap.h"

int bt_client_open(hci_context_t *hci_ctx, const char *mac_addr, int rf_channel);
int bt_client_close(void);
int bt_client_write(void *buf, size_t nbytes);
int bt_client_data_available(void);
int bt_client_read(void *buf, size_t nbytes);
int circular_buffer_read(uint8_t *data);
void client_init_queue(l2cap_context_t *l2cap_ctx);

#endif // CLIENT_H
