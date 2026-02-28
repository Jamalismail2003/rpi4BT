#ifndef SDP_H
#define SDP_H

#include "l2cap.h"



//--------------------


#define MAX_SDP_REQUESTS 20
#define MAX_SDP_RESULT_FRAMES  15


typedef struct
{
    u16 txn_id;
    u8 *data;
    u16 len;
    u8 cont_len;
    u8 *cont;
} sdpResultFrame;



typedef struct
{
    u16 txn_id;
    u16 svc_id;
    u16 begin_attr;
    u16 end_attr;
    lcapConnection *lcn;
    u8  num_frames;
    sdpResultFrame frame[MAX_SDP_RESULT_FRAMES];

    // parser variables
    
    u16 bytes_parsed;
    u8  parse_frame_num;
    int parse_frame_len;
    u8 *parse_ptr;
    u8 rfcomm_channel;

    bool parse_complete;
    char *output_buf;//[4096];
    int output_size;
    int output_len;
} sdpRequest;



#define MAX_SERVICES        2

typedef struct
{
    u32 service_id;
    u16 uuid_service;      // must be SDP or SP for now
    u16 uuid_protocol;      // L2CAP protocol automatically added first
    const char *service_name;
    const char *service_desc;
    u8 rfcomm_channel;      
} localServiceRecord;


int sdpLayer_init(l2cap_context_t *pLCAP);

typedef struct
{
    lcapClient base;

    u16 m_next_txn_id;
    //lcapLayer *m_pLCAP;
    sdpRequest m_requests[MAX_SDP_REQUESTS];

    // rudimentary SDP server
    
    u8 m_num_services;
    u32 m_next_service_id;
    localServiceRecord m_services[MAX_SERVICES];

    l2cap_context_t *l2cap_ctx;
} sdp_context_t;

int sdp_init(l2cap_context_t *l2cap_ctx);


u32 addService(
    sdp_context_t *sdp_ctx,
    u16 uuid_service,           // must be SDP or SP for now
    u16 uuid_protocol,          // L2CAP protocol automatically added first
    const char *service_name,
    const char *service_desc,
    u8 rfcomm_channel);         // set to zero if unused

//sdpRequest *sdpLayer_doSdpRequest(u8 *addr, u16 svc_id,  u16 begin_attr,  u16 end_attr );
sdpRequest *sdpLayer_doSdpRequest(l2cap_context_t *l2cap_ctx, const u8 *addr, u16 svc_id,  u16 begin_attr,  u16 end_attr );


#endif // SDP_H
