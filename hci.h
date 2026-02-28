#ifndef HCI_H
#define HCI_H

#include <stdint.h>
#include <stddef.h> 
#include <stdbool.h>
#include "hci_defs.h"
#include "btQueue.h"
#include "transport.h"

// these bits are kept in the high byte of the nci_handle
// a non-zero handle with neither set is valid

#define HCI_HANDLE_CONNECTING        0x1000
#define HCI_HANDLE_ERROR             0x8000

// hci client events

#define HCC_EVENT_CONNECTED          0x0001
#define HCC_EVENT_DISCONNECTED       0x0002
#define HCC_EVENT_CONNECTION_ERROR   0x8000
#define HCC_INQUIRY_COMPLETE         0x0100
#define HCC_INQUIRY_DEVICE_FOUND     0x0200
#define HCC_INQUIRY_NAME_FOUND       0x0400

#define INQUIRY_SECONDS              12 // Scan timeout
#define HCI_DEVICE_INCLUDE_UNUSED_FIELDS   0

typedef struct hciRemoteDevice hciRemoteDevice;
struct hciRemoteDevice
{
    hciRemoteDevice *prev;
    hciRemoteDevice *next;
    
    u8      addr[BT_ADDR_SIZE];
    u8      device_class[BT_CLASS_SIZE];
    char    name[BT_NAME_SIZE];
    u16     handle;
    u8      link_key_type;
    u8      link_key[BT_LINK_KEY_SIZE];
    
    // support for lcap layer
    
    u16     next_lcap_id;
    u16     next_lcap_cid;
    
    #if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
        u16     packet_type;
        u8      page_rep_mode;
        u8      page_mode;
        u16     clock_offset;
        u8      max_slots;
        u16     timeout;
        u8      link_type;
        u8      encrypt;
        u8      bonding_state;
    #endif
   
};


typedef struct hci_context_t hci_context_t;

typedef void (*hci_data_callback_t)(hci_context_t *context, uint8_t *data, size_t length);

struct hci_context_t {
    int device_handle;
    hci_data_callback_t data_callback;
    void *user_data;


    u16 m_num_devices;  
    hciRemoteDevice *m_first_device;
    hciRemoteDevice *m_last_device;

    btQueue m_command_queue;
    btQueue m_event_queue;
    btQueue m_send_data_queue;
    btQueue m_recv_data_queue;

    u16 m_can_send_data;
    u16 m_can_send_command;

    u16 m_next_hci_handle;
    u16 m_num_name_requests;
    
// Base layer class
    bool base_m_is_setup;
    bool inquiry_complete;
    u32  m_device_class;
    u8   m_local_name[BT_NAME_SIZE];
    u8   m_local_addr[BT_ADDR_SIZE];

// HCI Vendor class
    bool vendor_m_is_setup;
    unsigned m_nFirmwareOffset;
    transport_context_t *transport;
};

//hci_context_t *myhciLayer;

//----------------------------
/*
typedef struct hci_context_t hci_context_t;

typedef void (*hci_data_callback_t)(hci_context_t *context, uint8_t *data, size_t length);

struct hci_context_t {
    int device_handle;
    hci_data_callback_t data_callback;
    void *user_data;
};
*/
hci_context_t *hci_init(transport_context_t *transport);
int hci_register_data_callback(hci_context_t *context, hci_data_callback_t callback, void *user_data);
void hci_receive_data(hci_context_t *context, uint8_t *data, size_t length);
//----------------------------

int hci_send_command(hci_context_t *context, void *cmd, uint32_t cmd_size);
void hci_send_data(hci_context_t *context, void *data, unsigned length);
int hci_send_hci_command(hci_context_t *context, void *cmd, uint32_t cmd_size);
int hci_send_hci_data(hci_context_t *context, void *data, uint32_t data_size);

int hciLayer_init(hci_context_t *hci);
int SendHCICommand(void *cmd, uint32_t cmd_size);
int sendCommand(void *cmd, uint32_t cmd_size);
void sendData(void *data, unsigned length);
int SendHCIData(void *cmd, uint32_t cmd_size);
const char *addrToString(const unsigned char *addr);
const char *deviceClassToString(const unsigned char *cls);


hciRemoteDevice *addDevice(hci_context_t *context, const u8 *bdAddr, const char *name);
hciRemoteDevice *findDeviceByAddr(hci_context_t *context, const u8 *addr);
hciRemoteDevice *findDeviceByName(const char *name);
hciRemoteDevice *findDeviceByHandle(hci_context_t *context, u16 handle);

const u8 *strToBtAddr(const char *str);

void hci_base_setup(hci_context_t *context);
void hci_base_reset(hci_context_t *context);
void hci_vendor_setup(hci_context_t *context);

void saveDevices(hci_context_t *context);
void loadDevices(hci_context_t *context);
void unpair(hci_context_t *context, hciRemoteDevice *device);

void  hciLayer_closeConnection(hci_context_t *context, hciRemoteDevice *device);
hciRemoteDevice *hciLayer_startConnection(hci_context_t *context, const uint8_t *addr);

hci_context_t *hci_get_context(void);

#endif // HCI_H
