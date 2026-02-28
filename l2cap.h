#ifndef L2CAP_H
#define L2CAP_H

#include <stdint.h>
#include <stdio.h>

#include "l2cap_defs.h"
#include "hci.h"



// Define necessary types and structures
typedef uint16_t u16;
typedef uint8_t u8;



#define MAX_LCAP_CLIENTS            4
#define MAX_LCAP_CONNECTIONS        100

#define LCAP_EVENT_CONNECTING      0x0001
#define LCAP_EVENT_CONNECTED       0x0002
#define LCAP_EVENT_DISCONNECTED    0x0004
#define LCAP_EVENT_DATA            0x0010
#define LCAP_EVENT_ERROR           0x8000

extern const char *lcapEventName(u16 event);

typedef struct lcapConnection lcapConnection;
typedef struct lcapClient lcapClient;
typedef struct hciLayer hciLayer; // Assuming hciLayer is defined elsewhere.
typedef struct hciRemoteDevice hciRemoteDevice; // Assuming hciRemoteDevice is defined elsewhere.

struct lcapConnection {
    u16 psm;
    u16 local_cid;
    hciRemoteDevice *device;
    u8  lcap_state;
    u16 remote_cid;
    lcapClient *pClient;
};

struct lcapClientVTable {
    void (*foo)(void);
    u16 (*getPSM)(lcapClient *);
    const char *(*serviceName)(lcapClient *);
    void (*lcapEvent)(void *user_data, u16 event_type, lcapConnection *lcn, const u8* buffer, u16 length);
};

struct lcapClient {
#if 0
    struct lcapClientVTable *vtable;
#else
    void (*foo)(void);
    //u16 (*getPSM)(lcapClient *);
    u16 (*getPSM)(void);
    const char *(*serviceName)(void);
    void (*lcapEvent)(void *user_data, u16 event_type, lcapConnection *lcn, const u8* buffer, u16 length);
#endif
};
#if 0
typedef struct lcapLayer lcapLayer;

struct lcapLayer {
//    hciLayer *m_pHCI;
    u16 m_num_clients;
    lcapClient *m_pClients[MAX_LCAP_CLIENTS];
    lcapConnection m_connections[MAX_LCAP_CONNECTIONS];
    // Function pointers for virtual functions
    void (*receiveData)(lcapLayer *, const void *buffer, unsigned length);
    void (*receiveEvent)(lcapLayer *, u16 hci_client_event, hciRemoteDevice *device);
};
typedef struct lcapLayer lcapLayer;
#endif
typedef struct l2cap_context_t l2cap_context_t;
struct l2cap_context_t {
    u16 m_num_clients;
    lcapClient *m_pClients[MAX_LCAP_CLIENTS];
    lcapConnection m_connections[MAX_LCAP_CONNECTIONS];
    // Function pointers for virtual functions
    void (*receiveData)(l2cap_context_t *, const void *buffer, unsigned length);
    void (*receiveEvent)(l2cap_context_t *, u16 hci_client_event, hciRemoteDevice *device);

    //int mtu;
    hci_context_t *hci_context;
};

l2cap_context_t * l2cap_init(hci_context_t *hci_context);
void l2cap_handle_hci_data(hci_context_t *hci_context, uint8_t *data, size_t length);


// Function declarations
l2cap_context_t* lcapLayer_create(void);
void lcapLayer_destroy(l2cap_context_t *layer);


int lcapLayer_sendData(void *cmd, uint32_t cmd_size);

void lcapLayer_registerClient(l2cap_context_t *layer, lcapClient *pClient);
//lcapConnection* lcapLayer_startConnection(lcapLayer *layer, u8 *addr, u16 psm);
//lcapConnection* lcapLayer_startConnection(u8 *addr, u16 psm);
lcapConnection* lcapLayer_startConnection(l2cap_context_t *l2cap_ctx, const u8 *addr, u16 psm);
void lcapLayer_closeConnection(lcapConnection *lcn);

// Additional function declarations for lcapLayer
void lcapLayer_closeHCIConnection(l2cap_context_t *layer, hciRemoteDevice *device);
lcapConnection *lcapLayer_findRemoteCid(l2cap_context_t *l2cap_ctx, hciRemoteDevice *device, u16 cid);
//lcapConnection* lcapLayer_findLocalCid(lcapLayer *layer, u16 hci_handle, u16 cid);
lcapConnection *lcapLayer_findLocalCid(hci_context_t *context, u16 hci_handle, u16 cid);
lcapConnection *lcapLayer_addConnection(l2cap_context_t *l2cap_ctx, hciRemoteDevice *device, u16 psm, u16 rcid) ;
void lcapLayer_deleteConnection(l2cap_context_t *l2cap_ctx, lcapConnection *lcn, bool ifLastCloseHCI);
lcapClient *lcapLayer_findClientByPSM(l2cap_context_t *l2cap_ctx, u16 psm);
void lcapLayer_dispatchEvents(l2cap_context_t *l2cap_ctx, hciRemoteDevice *device, u16 event_type);
void lcapLayer_checkLcapConnectionState(l2cap_context_t *l2cap_ctx, void *req, lcapConnection *lcn);

// Function prototypes for virtual methods from inherited hciClient
void lcapLayer_receiveData(const void *buffer, unsigned length);
void lcapLayer_receiveEvent(l2cap_context_t *layer, u16 hci_client_event, hciRemoteDevice *device);

//void receiveEvent(u16 event, hciRemoteDevice *device);
void receiveEvent(hci_context_t *hci_ctx, u16 event, hciRemoteDevice *device);

#endif // L2CAP_H
