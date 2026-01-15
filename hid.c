#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>

#include "utils.h"
#include "hci_defs.h"
#include "l2cap.h"
#include "rfcomm.h"
#include "hci.h"
#include "hid.h"

#define LCAP_CID    0x0001

#if 0
typedef struct {
    u16 handle;        // HCI connection handle + flags
    u16 length;        // L2CAP payload length
    u16 cid;           // L2CAP channel ID (destination CID)
    u8  request_type;  // Request type (Host-to-Device, Class, Interface)
    u8  request;       // HID-specific request (Set Idle: 0x0A)
    u16 value;         // Idle rate (high byte) + Report ID (low byte)
    u16 index;         // Interface index (usually 0x0000)
    u16 length_field;  // No additional data (0x0000)
} __attribute__((packed)) set_idle_request;

void send_set_idle_request(/*hciRemoteDevice *device, */uint8_t idle_rate) {
    set_idle_request cmd;

    // Set HCI handle and L2CAP flags
    u16 hci_handle = 0x200b;//device->handle & 0x0fff;
    cmd.handle = hci_handle | 0x2000; // Set HCI handle with ACL data flag

    // Set L2CAP-specific fields
    cmd.length = sizeof(cmd) - sizeof(cmd.handle) - sizeof(cmd.length); // L2CAP payload length
    cmd.cid = 0x0011; // L2CAP channel ID for HID Control (PSM 0x11)

    // Set HID-specific fields
    cmd.request_type = 0x21;        // Host-to-Device, Class, Interface
    cmd.request = 0x0A;             // Set Idle request
    cmd.value = (idle_rate << 8);   // Idle rate in 4 ms intervals (high byte) + Report ID (low byte)
    cmd.index = 0x0000;             // Interface index (default is 0x00)
    cmd.length_field = 0x0000;      // No additional data

display_bytes("set_idle_request:", &cmd, sizeof cmd);

    // Send the L2CAP packet
    lcapLayer_sendData(&cmd, sizeof(cmd));
}
#endif

static u16 hid_intr_get_PSM()
{
    return HID_INTR_PSM;
}

static const char *hid_intr_get_service_name()
{
    return "HID - INTR";
}

static u16 hid_ctrl_get_PSM()
{
    return HID_CTRL_PSM;
}

static const char *hid_ctrl_get_service_name()
{
    return "HID - Ctrl";
}


static void hid_intr_event(void *user_data, u16 event_type, lcapConnection *lcn,  const u8* buffer,  u16 length)
{
	hid_context_t *hid_intr_ctx = user_data;

    if (buffer!=NULL && length>0)
        (*hid_intr_ctx->pClient)(buffer, length);

#if 0
    log_info("> HID Intr Event -------- len:%d PSM:%s\n", length, hid_intr_ctx->base.serviceName());

    if(length > 0)
    	display_bytes("HID Intr Event", buffer, length);
#endif
}


static void hid_ctrl_event(void *user_data, u16 event_type, lcapConnection *lcn,  const u8* buffer,  u16 length)
{
	hid_context_t *hid_ctrl_ctx = user_data;

    log_info("> HID Ctrl Event ******** len:%d PSM:%s\n", length, hid_ctrl_ctx->base.serviceName());

#if 0
	// FIXME
	// This is suppose to set the rate and is not working.
	// Need to find a way to send set/get report to the device
	send_set_idle_request(10);
#endif

    if(length > 0)
	    display_bytes("HID Ctrl Event", buffer, length);
}

int hid_connect(l2cap_context_t *l2cap_ctx, u8 *addr)
{
	lcapLayer_startConnection(l2cap_ctx, addr, HID_CTRL_PSM);

	lcapLayer_startConnection(l2cap_ctx, addr, HID_INTR_PSM);

	return 0;
}

void hid_register_client(l2cap_context_t *l2cap_ctx, hid_callback pClient)
{   
    log_info("HID: Registering client\n");
    
    hid_context_t *hid_ctrl_ctx = (hid_context_t *)lcapLayer_findClientByPSM(l2cap_ctx, HID_INTR_PSM);

    hid_ctrl_ctx->pClient = pClient;
}

int hid_init(l2cap_context_t *l2cap_ctx)
{
    hid_context_t *hid_ctrl_ctx = (hid_context_t *)malloc(sizeof(hid_context_t));
    if (!hid_ctrl_ctx) {
        log_error("Failed to allocate hid_context_t");
        return 0;
    }

    hid_context_t *hid_intr_ctx = (hid_context_t *)malloc(sizeof(hid_context_t));
    if (!hid_intr_ctx) {
        log_error("Failed to allocate hid_context_t");
        free(hid_ctrl_ctx);
        return 0;
    }

	hid_ctrl_ctx->l2cap_ctx = l2cap_ctx;
    hid_ctrl_ctx->base.getPSM = hid_ctrl_get_PSM;
    hid_ctrl_ctx->base.serviceName = hid_ctrl_get_service_name;
    hid_ctrl_ctx->base.lcapEvent = hid_ctrl_event;
	lcapLayer_registerClient(l2cap_ctx, (lcapClient*)hid_ctrl_ctx);


	hid_intr_ctx->l2cap_ctx = l2cap_ctx;
    hid_intr_ctx->base.getPSM = hid_intr_get_PSM;
    hid_intr_ctx->base.serviceName = hid_intr_get_service_name;
    hid_intr_ctx->base.lcapEvent = hid_intr_event;
	lcapLayer_registerClient(l2cap_ctx, (lcapClient*)hid_intr_ctx);
	
	return 0;
}
