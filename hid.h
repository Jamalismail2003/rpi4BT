#ifndef _hid_layer_h_
#define _hid_layer_h_

#include "l2cap.h"

typedef void (*hid_callback)(const u8 *buffer, u16 length);

typedef struct
{
    lcapClient base;
    hid_callback pClient;
    uint16_t cid;

    lcapConnection *lcap_conn;
    l2cap_context_t *l2cap_ctx;
} hid_context_t;

enum HID_TransactionTypes_t
{
    HID_TRANS_HANDSHAKE            = 0x00,
    HID_TRANS_CONTROL              = 0x10,
    HID_TRANS_GET_REPORT           = 0x40,
    HID_TRANS_SET_REPORT           = 0x50,
    HID_TRANS_GET_PROTOCOL         = 0x60,
    HID_TRANS_SET_PROTOCOL         = 0x70,
    HID_TRANS_GET_IDLE             = 0x80,
    HID_TRANS_SET_IDLE             = 0x90,
    HID_TRANS_DATA                 = 0xA0,
    HID_TRANS_DATAC                = 0xB0,
};

int hid_connect(l2cap_context_t *l2cap_ctx, u8 *addr);
void hid_register_client(l2cap_context_t *l2cap_ctx, hid_callback pClient);
int hid_init(l2cap_context_t *l2cap_ctx);

#endif
