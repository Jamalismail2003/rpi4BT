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
#include "hci.h"
#include "l2cap.h"


#define LCAP_CID    0x0001
    // probably defined else where, this is the
    // main CAP signal channel id


#define SHOW_INCOMING_DATA      1
#define SHOW_OUTGOING_DATA      1

#define CLOSE_UNUSED_HCI_HANDLES    1
    // if we close all the lcap channels to an hci_handle
    // we will also close the HCI handle 


// It appears to be required that you (a) respond to any config and
// info requests, and that (b) you MUST send a config request for each
// new lcap channel.  It took me 2 hard weeks to get through to a remote
// SDP the first time.  The only option I left was maybe to do a pending
// connection, in case I find some other cranky remote needs it.

#define DO_CONFIG_REQUESTS          1       // REQUIRED
#define SEND_PENDING_CONNECTION     0
#define WAIT_FOR_CONFIG_RESPONSES   1
    // I found it works best if we wait for a config request before
    // we send our own config request, and that we wait, even after
    // EVENT_CONNECTED completed, to call the HCI connection "open"
    // and let clients start using the port.

// lcap connection state bits

#define LCAP_STATE_CONNECTING                   0x0001
#define LCAP_STATE_CONNECTED                    0x0002
#define LCAP_STATE_ERROR                        0x0008

#define LCAP_STATE_CONFIG_REQUEST_RECEIVED      0x0010
#define LCAP_STATE_CONFIG_RESPONSE_SENT         0x0020
#define LCAP_STATE_CONFIG_REQUEST_SENT          0x0040
#define LCAP_STATE_CONFIG_RESPONSE_RECEIVED     0x0080

//----------------------------------------------------------------------------------------

const char *lcapEventName(u16 event)
{
    if (event == LCAP_EVENT_CONNECTING    ) return "LCAP_EVENT_CONNECTING";
    if (event == LCAP_EVENT_CONNECTED     ) return "LCAP_EVENT_CONNECTED";
    if (event == LCAP_EVENT_DISCONNECTED  ) return "LCAP_EVENT_DISCONNECTED";
    if (event == LCAP_EVENT_DATA          ) return "LCAP_EVENT_DATA";
    if (event == LCAP_EVENT_ERROR         ) return "LCAP_EVENT_ERROR";
    return "unknown lcapEvent";
}


const char *hccEventName(u16 event)
{
    if (event == HCC_EVENT_CONNECTED        ) return "HCC_EVENT_CONNECTED";
    if (event == HCC_EVENT_DISCONNECTED     ) return "HCC_EVENT_DISCONNECTED";
    if (event == HCC_EVENT_CONNECTION_ERROR ) return "HCC_EVENT_CONNECTION_ERROR";
    if (event == HCC_INQUIRY_COMPLETE       ) return "HCC_INQUIRY_COMPLETE";
    if (event == HCC_INQUIRY_DEVICE_FOUND   ) return "HCC_INQUIRY_DEVICE_FOUND";
    if (event == HCC_INQUIRY_NAME_FOUND     ) return "HCC_INQUIRY_NAME_FOUND";
    return "unknown hccEvent";
}


const char *getLcapCommandString(u8 b)
{
    if (b == 0x01)  return "LCAP_COMMAND_REJECT";
    if (b == 0x02)  return "LCAP_CONNECTION_REQUEST";
    if (b == 0x03)  return "LCAP_CONNECTION_RESPONSE";
    if (b == 0x04)  return "LCAP_CONFIGURE_REQUEST";
    if (b == 0x05)  return "LCAP_CONFIGURE_RESPONSE";
    if (b == 0x06)  return "LCAP_DISCONNECTION_REQUEST";
    if (b == 0x07)  return "LCAP_DISCONNECTION_RESPONSE";
    if (b == 0x08)  return "LCAP_ECHO_REQUEST";
    if (b == 0x09)  return "LCAP_ECHO_RESPONSE";
    if (b == 0x0a)  return "LCAP_INFORMATION_REQUEST";
    if (b == 0x0b)  return "LCAP_INFORMATION_RESPONSE";
    if (b == 0x12)  return "LCAP_PARAMETER_UPDATE_REQUEST";
    if (b == 0x13)  return "LCAP_PARAMETER_UPDATE_RESPONSE";
    if (b == 0x14)  return "LCAP_LE_CONNECTION_REQUEST";
    if (b == 0x15)  return "LCAP_LE_CONNECTION_RESPONSE";
    if (b == 0x16)  return "LCAP_LE_FLOW_CONTROL_CREDIT";
    return "UNKNOWN LCAP COMMAND";
}




int lcapLayer_sendData(void *buffer, uint32_t length)
{

    lcap_data_packet_header *hdr = (lcap_data_packet_header *) buffer;
    hdr->lcap_d.len = length - sizeof(lcap_data_packet_header);

    hci_context_t *hci_ctx = hci_get_context();
    //hci_send_data(mylcapLayer->hci_context, buffer, length);
    hci_send_data(hci_ctx, buffer, length);
    return 0;
}


// receive event from HCI layer
void receiveEvent(hci_context_t *hci_ctx, u16 event, hciRemoteDevice *device)
{
    if (event != HCC_INQUIRY_DEVICE_FOUND)
        log_indented(8, "%s(0x%04x) handle(0x%04x) %s %s",
            hccEventName(event),
            event,
            device ? device->handle : 0,
            device ? (char *) addrToString(device->addr) : "",
            device ? device->name : "");

    l2cap_context_t *l2cap_ctx = (l2cap_context_t *)hci_ctx->user_data;

    switch (event)
    {
        case HCC_EVENT_CONNECTED: // start any LCAP connections that were pending
        {
            assert(device);
            for (int i=0; i<MAX_LCAP_CONNECTIONS; i++)
            {
                //l2cap_context_t *l2cap_ctx = (l2cap_context_t *)hci_ctx->user_data;
                lcapConnection *lcn = &l2cap_ctx->m_connections[i];

                if (lcn->device == device &&
                    lcn->lcap_state & LCAP_EVENT_CONNECTING)
                {
                    log_indented(8, "Starting pending connection to %s(%s):0x%04x for lcid(0x%04x) psm(0x%04x)",
                        addrToString(device->addr), device->name, device->handle, lcn->local_cid, lcn->psm);

                    lcap_connect_request cmd;
                    u16 hci_handle = device->handle & 0x0fff;
                    cmd.hdr.hci.handle = hci_handle | 0x2000;
                    cmd.hdr.lcap_d.cid = LCAP_CID;
                    cmd.hdr.lcap_c.cmd = LCAP_CONNECTION_REQUEST;
                    cmd.hdr.lcap_c.id = device->next_lcap_id++;
                    cmd.hdr.lcap_c.cmd_len = 4;
                    cmd.psm = lcn->psm;
                    cmd.src_cid = lcn->local_cid;

                    log_info("<-- LCAP - LCAP_CONNECTION_REQUEST[%d] lcid=0x%04x", cmd.hdr.lcap_c.id, lcn->local_cid);
                    lcapLayer_sendData(&cmd, sizeof(cmd));
                }
            }
            break;
        }
        
        case HCC_EVENT_CONNECTION_ERROR:
        {
            log_error("HCC_EVENT_CONNECTION_ERROR");
            assert(device);
            lcapLayer_dispatchEvents(l2cap_ctx, device, LCAP_EVENT_ERROR);
            break;
        }
        case HCC_EVENT_DISCONNECTED:
        {
            log_error("HCC_EVENT_DISCONNECTED");
            assert(device);
            lcapLayer_dispatchEvents(l2cap_ctx, device, LCAP_EVENT_DISCONNECTED);
            break;
        }
    }
}

void lcapLayer_closeHCIConnection(l2cap_context_t *layer, hciRemoteDevice *device) {
    // Empty body
}

lcapConnection *lcapLayer_findRemoteCid(l2cap_context_t *l2cap_ctx, hciRemoteDevice *device, u16 cid)
    // used to make sure it does NOT already exist in a new
    // connection request
{
    assert(device);
    for (u16 i=0; i<MAX_LCAP_CONNECTIONS; i++)
    {
        lcapConnection *lcn = &l2cap_ctx->m_connections[i];
        if (lcn->device == device &&
            lcn->remote_cid == cid)
            return lcn;
    }
    return NULL;
}

lcapConnection *lcapLayer_findLocalCid(hci_context_t *hci_ctx, u16 hci_handle, u16 cid) 
{
    hci_handle &= 0x0fff;
    hciRemoteDevice *device = findDeviceByHandle(hci_ctx, hci_handle);
    if (!device)
    {
        log_error("Could not find device for hci_handle(0x%04x)", hci_handle);
        return 0;
    }

    l2cap_context_t *l2cap_ctx = (l2cap_context_t *)hci_ctx->user_data;

    for (u16 i=0; i<MAX_LCAP_CONNECTIONS; i++)
    {
        lcapConnection *lcn = &l2cap_ctx->m_connections[i];
        if (lcn->device == device &&
            lcn->local_cid == cid)
            return lcn;
    }
    log_error("Could not find lcn for hci_handle(0x%04x) and lcid(0x%04x)", hci_handle, cid);
    return 0;
}

lcapConnection *lcapLayer_addConnection(l2cap_context_t *l2cap_ctx, hciRemoteDevice *device, u16 psm, u16 rcid) 
{
    assert(device);
    log_indented(8, "addConnection(%s:0x%04x) psm=0x%04x lcid=0x%04x rcid=0x%04x",
        addrToString(device->addr), device->handle, psm, device->next_lcap_cid, rcid);
    
    for (u16 i=0; i<MAX_LCAP_CONNECTIONS; i++)
    {
        lcapConnection *lcn = &l2cap_ctx->m_connections[i];// &m_connections[i];
        if (!lcn->local_cid)
        {
            lcn->psm = psm;
            lcn->local_cid = device->next_lcap_cid++;
            lcn->device = device;
            lcn->lcap_state = 0;
            lcn->remote_cid = rcid;
            lcn->pClient = lcapLayer_findClientByPSM(l2cap_ctx, lcn->psm);
                // hmm .. incoming or outgoing?
                // we associate the psm with any matching service 
                
            log_indented(8, "LCAP Connection Index=%d  addConnection(%s:0x%04x,0x%04x) added lcid(0x%04x) client=0x%p",
                i, addrToString(device->addr), device->handle, rcid, lcn->local_cid, lcn->pClient);
            return lcn;
        }
    }

    log_warning("        Could not addConnection(%s:0x%04x) psm=0x%04x lcid=0x%04x rcid=0x%04x - no room!!",
        addrToString(device->addr), device->handle, psm, device->next_lcap_cid, rcid);

    return NULL;
}

void lcapLayer_deleteConnection(l2cap_context_t *l2cap_ctx, lcapConnection *lcn, bool ifLastCloseHCI) 
{
    assert(lcn);
    log_indented(8, "deleteConnection(%s:0x%04x) lcid=0x%04x rcid=0x%04x",
        lcn->device ? addrToString(lcn->device->addr) : "unknown",
        lcn->device ? lcn->device->handle : 0xFFFF,
        lcn->local_cid,
        lcn->remote_cid);
    
    #if CLOSE_UNUSED_HCI_HANDLES
        if (ifLastCloseHCI &&
            lcn->device &&
            lcn->device->handle)
        {
            bool any = false;
            for (u16 i=0; i<MAX_LCAP_CONNECTIONS; i++)
            {
                lcapConnection *l = &l2cap_ctx->m_connections[i];
                if (l != lcn &&
                    l->device == lcn->device)
                {
                    any = true;
                    break;
                }
            }
            
            if (!any)
            {
                log_indented(8, "last lcap connection to (%s:0x%04x).  Closing hci connection.",
                    addrToString(lcn->device->addr), lcn->device->handle); 
#if 0
                // FIXME: i need hci context
                hciLayer_closeConnection(lcn->device);
#endif
            }
        }
    #endif

    memset(lcn,0,sizeof(lcapConnection));
}

lcapClient *lcapLayer_findClientByPSM(l2cap_context_t *l2cap_ctx, u16 psm) 
{

    for (u16 i=0; i < l2cap_ctx->m_num_clients; i++)
    {
        lcapClient *pClient = l2cap_ctx->m_pClients[i];

        if (pClient->getPSM() == psm)
        {
            log_indented(8, "find Client By PSM = %d", psm);
            return pClient;
        }
    }

    return 0;   
}

void lcapLayer_dispatchEvents(l2cap_context_t *l2cap_ctx, hciRemoteDevice *device, u16 event_type) 
//void lcapLayer::dispatchEvents(hciRemoteDevice *device, u16 event_type)
    // send events to client layers
    // we call deleteConnection(false) because we know we
    // are already in an hciCloseConnection cycle 
{
    assert(device);
    for (u16 i=0; i<MAX_LCAP_CONNECTIONS; i++)
    {
        lcapConnection *lcn = &l2cap_ctx->m_connections[i];
        if (lcn->device == device && lcn->pClient)
        {
            if (event_type == LCAP_EVENT_ERROR)
                lcn->lcap_state |= LCAP_STATE_ERROR;

            lcn->pClient->lcapEvent(lcn->pClient, event_type, lcn, 0, 0);
            if (event_type == LCAP_EVENT_DISCONNECTED)
                lcapLayer_deleteConnection(l2cap_ctx, lcn,false);
        }
    }
}

void lcapLayer_checkLcapConnectionState(l2cap_context_t *l2cap_ctx, void *req, lcapConnection *lcn) 
{
    log_indented(8, "lcapLayer_checkLcapConnectionState");

    #if DO_CONFIG_REQUESTS
        assert(lcn);
        if (lcn->remote_cid &&
            lcn->lcap_state & LCAP_STATE_CONFIG_REQUEST_RECEIVED &&
            !(lcn->lcap_state & LCAP_STATE_CONFIG_REQUEST_SENT))
        {
            lcn->lcap_state |= LCAP_STATE_CONFIG_REQUEST_SENT;
            
            assert(lcn->device);
            
            lcap_config_request cmd;
            memset(&cmd,0,sizeof(cmd));
            cmd.hdr.hci.handle = lcn->device->handle | 0x2000;
            cmd.hdr.lcap_d.cid = LCAP_CID;
            cmd.hdr.lcap_c.cmd = LCAP_CONFIGURE_REQUEST;
            cmd.hdr.lcap_c.id = lcn->device->next_lcap_id++;
            cmd.hdr.lcap_c.cmd_len = 8;
            cmd.dest_cid = lcn->remote_cid;
            cmd.flags = 0;
            cmd.options[0] = 1;         // type MTU, not optional
            cmd.options[1] = 2;         // len

            // RF & SDP: 0x0200
            cmd.options[2] = 0x00;      // our MTU LSB
            cmd.options[3] = 0x02;      // our MTU MSB
            log_info("<-- LCAP - LCAP_CONFIG_REQUEST[%d]",cmd.hdr.lcap_c.id);
            lcapLayer_sendData(&cmd,sizeof(cmd));
        }
    #endif

    #if SEND_PENDING_CONNECTION || WAIT_FOR_CONFIG_RESPONSES
        assert(lcn);
        if ((lcn->lcap_state & LCAP_STATE_CONFIG_REQUEST_RECEIVED) &&
            (lcn->lcap_state & LCAP_STATE_CONFIG_RESPONSE_SENT) &&
            (lcn->lcap_state & LCAP_STATE_CONFIG_REQUEST_SENT) &&
            (lcn->lcap_state & LCAP_STATE_CONFIG_RESPONSE_RECEIVED) &&
            lcn->local_cid )
        {
            #if SEND_PENDING_CONNECTION
                lcap_connect_response cmd;
                memset(&cmd,0,sizeof(cmd));
                
                u16 hci_handle = req->hdr.hci.handle & 0x0fff;
                u16 rcid = ;
                
                cmd.hdr.hci.handle      = hci_handle | 0x2000;      // word - handle or'd with flags
                cmd.hdr.hci.len         = 0;                        // word - hci length (filled in by hci::sendData())
                cmd.hdr.lcap_d.len      = 0;                        // word - lcap length (filled in by lcap::sendData())
                cmd.hdr.lcap_d.cid      = 0x0001;                   // word - main channel
                cmd.hdr.lcap_c.cmd      = LCAP_CONNECTION_RESPONSE; // byte - lcap command
                cmd.hdr.lcap_c.id       = req->hdr.lcap_c.id;       // byte - return the unique id from requestor
                cmd.hdr.lcap_c.cmd_len  = 8;                        // word - length of following
                cmd.dest_cid            = lcn->local_cid;           // word - the local cid we are granting
                cmd.src_cid             = lcn->remote_cid;          // word - the remote cid being associated with the local cid
                cmd.result              = 0;                        // word - 0=complete, 1=pending
                cmd.status              = 0;                        // word - 0=nothing to say

                log_info("<-- LCAP - LCAP_CONNECTION_RESPONSE[%d] 0=completed",cmd.hdr.lcap_c.id);
                lcapLayer_sendData(&cmd,sizeof(cmd));
            #endif
            
            #if WAIT_FOR_CONFIG_RESPONSES
                lcapClient *pClient = lcapLayer_findClientByPSM(l2cap_ctx, lcn->psm);
                if (pClient)
                {
                    pClient->lcapEvent(pClient, LCAP_EVENT_CONNECTED, lcn, 0, 0);
                }
            #endif
            
        }
    #endif
}

void l2cap_handle_hci_data(hci_context_t *hci_context, uint8_t *buffer, size_t length)
{
    l2cap_context_t *l2cap_ctx = (l2cap_context_t *)hci_context->user_data;

    #if SHOW_INCOMING_DATA
        //display_bytes("lcap>",(u8 *)buffer,length);
    #endif

    lcap_command_packet_header *hdr = (lcap_command_packet_header *) buffer;
    
    //-----------------------
    // client data received
    //-----------------------
    
    if (hdr->lcap_d.cid != LCAP_CID)
    {
        u16 hci_handle =  hdr->hci.handle & 0x0fff;
        u16 lcid =  hdr->lcap_d.cid;
        
//        log_info("-->  LCAP CLIENT PACKET: hci_handle(0x%04x) cid(0x%04x)", hci_handle, lcid);
        lcapConnection *lcn = lcapLayer_findLocalCid(hci_context, hci_handle, lcid);
        if (!lcn) return;

        // we should only receive data for a client
        // specifically after a channel has been opened for that client,
        // and thus we have set the client pointer ..
        
        assert(lcn->pClient);
        if (lcn->pClient)
        {
            // send the data after the hci and lcap headers
            
//            log_indented(8, "Forwarding %d data bytes to client %s for psm(0x%04x) and lcid(0x%04x)",
//                hdr->lcap_d.len, lcn->pClient->serviceName(), lcn->psm, lcid);

            u8 *data_ptr = &((u8 *)buffer)[sizeof(lcap_data_packet_header)];
            
            lcn->pClient->lcapEvent(lcn->pClient, LCAP_EVENT_DATA, lcn, data_ptr, hdr->lcap_d.len);
            return;
        }

        log_error("        LCAP packet on hci_handle(0x%04x) to unknown psm(0x%04x)", hci_handle,lcn->psm);
        return;
    }

    //--------------------------
    // lcap event handling
    //--------------------------

    log_info("-->  %s: 0x%x", getLcapCommandString(hdr->lcap_c.cmd), hdr->lcap_c.cmd);

    switch (hdr->lcap_c.cmd)
    {
        case LCAP_CONNECTION_REQUEST:   // 0x02
        {
            lcap_connect_request *req = (lcap_connect_request *) buffer;
            u16 hci_handle =  req->hdr.hci.handle & 0x0fff;
            u16 rcid = req->src_cid;
            log_indented(8, "hci_handle=[%d] hci_handle(0x%04x) PSM(0x%04x) rcid=0x%04x",
                hci_handle,
                req->hdr.lcap_c.id,
                req->psm,
                rcid);


            // prh we should reject the request if it's an unknown PSM
            
            hciRemoteDevice *device = findDeviceByHandle(hci_context, hci_handle);
            if (!device)
            {
                log_error("        LCAP connection request from unknown hci_handle(0x%04x)", hci_handle);
                return;
            }

            // we should not receive multiple connection requests
            // or requests to connect to a channel that is already open
        
            lcapConnection *lcn = lcapLayer_findRemoteCid(l2cap_ctx, device, rcid);
            if (lcn)
            {
                log_error("        LCAP connection request to existing hci_handle(0x%04x) rcid(0x%04x)",
                    hci_handle,
                    rcid);
                return;
            }

            // create a new connection
            lcn = lcapLayer_addConnection(l2cap_ctx, device, req->psm, rcid);
            if (!lcn) return;

            // send the connection response
            
            lcap_connect_response cmd;
            memset(&cmd,0,sizeof(cmd));
            
            cmd.hdr.hci.handle      = hci_handle | 0x2000;      // word - handle or'd with flags
            cmd.hdr.lcap_d.cid      = 0x0001;                   // word - main channel
            cmd.hdr.lcap_c.cmd      = LCAP_CONNECTION_RESPONSE; // byte - lcap command
            cmd.hdr.lcap_c.id       = req->hdr.lcap_c.id;       // byte - return the unique id from requestor
            cmd.hdr.lcap_c.cmd_len  = 8;                        // word - length of following
            cmd.dest_cid            = lcn->local_cid;           // word - the local cid we are granting
            cmd.src_cid             = lcn->remote_cid;          // word - the remote cid being associated with the local cid

            #if SEND_PENDING_CONNECTION
                cmd.result          = 1;                        // word - 0=complete, 1=pending
            #else
                cmd.result          = 0;                        // word - 0=complete, 1=pending
            #endif              
            cmd.status              = 0;                        // word - 0=nothing to say

            log_info("<--  LCAP - CONNECTION_RESPONSE[%d] %d=%s",
                   cmd.hdr.lcap_c.id,
                   cmd.result,
                   cmd.result ? "pending" : "completed");
            lcapLayer_sendData(&cmd,sizeof(cmd));
                
            lcapLayer_checkLcapConnectionState(l2cap_ctx, (lcap_command_packet_header *) req, lcn);

            break;
        }
    
        case LCAP_CONNECTION_RESPONSE:  // 0x03
        {
            lcap_connect_response *rsp = (lcap_connect_response *) buffer;
            
            log_indented(8, "lcap_c.id=[%d] dest_cid=0x%04x  src_cid=0x%04x rslt(%d) stat(%d)",
                rsp->hdr.lcap_c.id,
                rsp->dest_cid,
                rsp->src_cid,
                rsp->result,
                rsp->status);

            switch (rsp->result)
            {
                case 0:
                    log_indented(8, "Connection succesful");
                    break;
                case 1:
                    log_indented(8, "Connection pending ...");
                    switch (rsp->status)
                    {
                        case 0:
                            log_indented(8, "0 No further information available");
                            break;
                        case 1:
                            log_indented(8, "1 Authentication pending");
                            break;
                        case 2:
                            log_indented(8, "22 Authorization pending");
                            break;
                        default:
                            log_error("        UNKNOWN STATUS(%d)",rsp->status);
                            break;
                    }
                    break;
                case 2:
                    log_error("        Connection refused – PSM not supported");
                    return;
                case 3:
                    log_error("        Connection refused – security block");
                    return;
                case 4:
                    log_error("        Connection refused – no resources available");
                    return;
                default:
                    log_error("        UNKNOWN CONNECTION RESULT!!!");
                    return;
            }

            u16 hci_handle = rsp->hdr.hci.handle;
            u16 rcid = rsp->dest_cid;
            u16 lcid = rsp->src_cid;

            lcapConnection *lcn = lcapLayer_findLocalCid(hci_context, hci_handle, lcid );
            if (!lcn) return;
            assert(lcn->remote_cid == 0 || lcn->remote_cid == rcid);

            lcapClient *pClient = lcn->pClient; 

            if (rsp->result == 0 ||
                rsp->result == 1 )
            {
                // add the remote cid
                
                lcn->remote_cid = rcid;
                if (rsp->result == 0)
                {
                    lcn->lcap_state |= LCAP_STATE_CONNECTED;
                    
                    #if !WAIT_FOR_CONFIG_RESPONSES
                        if (pClient)
                            pClient->lcapEvent(pClient, LCAP_EVENT_CONNECTED,lcn, 0, 0);
                    #endif
                }
                
                lcapLayer_checkLcapConnectionState(l2cap_ctx, (lcap_command_packet_header *)rsp,lcn);
            }
            else    // connection refused/rejected
            {
                if (pClient)
                    pClient->lcapEvent(pClient, LCAP_EVENT_ERROR, lcn, 0, 0);
                lcapLayer_deleteConnection(l2cap_ctx, lcn,true);
            }

            break;
        }

        case LCAP_CONFIGURE_REQUEST: // 0x04
        {
            lcap_config_request *req = (lcap_config_request *) buffer;
            u16 hci_handle =  req->hdr.hci.handle;
            u16 lcid       =  req->dest_cid;
            u8 opt_bytes   = (req->hdr.lcap_c.cmd_len - 4);


//lcap>         0c 20 10 00 0c 00 01 00 04 26 08 00 70 00 00       .........&..p..
//--> LCAP_CONFIG_REQUEST[38] hci_handle(0x200c) lcid=0x0070 opt_bytes(4) flag(0x0000)
//    MANDATORY UNKNOWN OPTION

            log_indented(8, "lcap_c.id=[%d] hci_handle(0x%04x) lcid=0x%04x opt_bytes(%d) flag(0x%04x)",
                req->hdr.lcap_c.id,
                hci_handle,
                lcid,
                opt_bytes,
                req->flags);
            assert(req->flags == 0);
            
            // the options are themselves a stream
            
            int i = 0;
            u8 *o = (u8 *) &req->options;
            while (i < opt_bytes)
            {
                u8 type = *o++;  i++;       // a byte for the type
                // u8 optional = type & 0x80;   // high order bit of the type says if it's an optional parameter
                type &= 0x7f;               // we only get, and respond to, MANDATORY ones
                
                u8 len  = *o++;  i++;       // a byte for the len that *could* have a high order continuation bit itself
                
                log_indented(8, type & 0x80 ? "    optional " : "    MANDATORY ");
                switch (type)
                {
                    case 1 : log_indented(8, "MTU 0x%04x",*(u16 *) o); break;
                    case 2 : log_indented(8, "FLUSH TIMEOUT 0x%04x",*(u16 *) o);  break;
                    case 3 : display_bytes("QOS",o,len); break;
                    default : log_error("      UNKNOWN OPTION");
                }
                i += len;
                o += len;
            }

            lcapConnection *lcn = lcapLayer_findLocalCid(hci_context, hci_handle, lcid );
            if (!lcn) return;
            lcn->lcap_state |= LCAP_STATE_CONFIG_REQUEST_RECEIVED;

            // create a config response and send it

            lcap_config_response cmd;
            memset(&cmd,0,sizeof(cmd));
            cmd.hdr.hci.handle      = hci_handle | 0x2000;      
            cmd.hdr.lcap_d.cid      = 0x0001;                   
            cmd.hdr.lcap_c.cmd      = LCAP_CONFIGURE_RESPONSE;  
            cmd.hdr.lcap_c.id       = req->hdr.lcap_c.id;       
            cmd.hdr.lcap_c.cmd_len  = 6;                        
            cmd.dest_cid            = lcn->remote_cid;  // word - the remote cid being associated with the local cid
            cmd.result              = 0;
            cmd.flags               = 0;

            log_info("<-- LCAP - LCAP_CONFIG_RESPONSE[%d]",cmd.hdr.lcap_c.id);
            lcapLayer_sendData(&cmd,sizeof(cmd));

            lcn->lcap_state |= LCAP_STATE_CONFIG_RESPONSE_SENT;
            lcapLayer_checkLcapConnectionState(l2cap_ctx, (lcap_command_packet_header *)req,lcn);

            break; 
        }

        case LCAP_CONFIGURE_RESPONSE: // 0x05
        {
            lcap_config_response *rsp = (lcap_config_response *) buffer;
            u16 hci_handle = rsp->hdr.hci.handle;
            u16 lcid       = rsp->dest_cid;      // local cid
            
            assert(!rsp->result);
            assert(!rsp->flags);
            
            log_indented(8,"lcap_c.id=[%d] hci_handle(0x%04x) lcid=0x%04x",
                rsp->hdr.lcap_c.id,
                hci_handle,
                lcid);

            lcapConnection *lcn = lcapLayer_findLocalCid(hci_context, hci_handle,lcid);
            if (!lcn) return;
            lcn->lcap_state |= LCAP_STATE_CONFIG_RESPONSE_RECEIVED;

            lcapLayer_checkLcapConnectionState(l2cap_ctx, (lcap_command_packet_header *)rsp, lcn);

            break;
        }
        
        
        case LCAP_INFORMATION_REQUEST : // 0x0a
        {
            lcap_info_request *req = (lcap_info_request *) buffer;
            u16 type = req->info_type;
            
            log_indented(8, "lcap_c.id=[%d] hci_handle(0x%04x) info_type(0x%04x)=%s",
                req->hdr.lcap_c.id,
                req->hdr.hci.handle,
                type,
                type == 1 ? "connectionless MTU" :
                type == 2 ? "extended features supported" :
                type == 3 ? "fixed channels supported" :
                "unknown");
            u16 hci_handle = req->hdr.hci.handle & 0x0fff;

            if (type == 2)
            {
                lcap_info_response_features cmd;
                memset(&cmd,0,sizeof(cmd));

                cmd.hdr.hci.handle      = hci_handle | 0x2000;      
                cmd.hdr.lcap_d.cid      = 0x0001;                   
                cmd.hdr.lcap_c.cmd      = LCAP_INFORMATION_RESPONSE;
                cmd.hdr.lcap_c.id       = req->hdr.lcap_c.id;       
                cmd.hdr.lcap_c.cmd_len  = 8;                        

                cmd.info_type   = type;
                cmd.result      = 0;
                cmd.data[0]     = LCAP_FEATURE_FIXED_CHANNELS;
                cmd.data[1]     = 0;
                cmd.data[2]     = 0;
                cmd.data[3]     = 0;
    
                log_info("<--  LCAP - LCAP_INFO_RESPONSE[%d]", cmd.hdr.lcap_c.id);
                lcapLayer_sendData(&cmd,sizeof(cmd));
            }
            else if (type == 3)
            {
                lcap_info_response_fixed_channels cmd;
                memset(&cmd,0,sizeof(cmd));
                
                cmd.hdr.hci.handle      = hci_handle | 0x2000;      
                cmd.hdr.lcap_d.cid      = 0x0001;                   
                cmd.hdr.lcap_c.cmd      = LCAP_INFORMATION_RESPONSE;
                cmd.hdr.lcap_c.id       = req->hdr.lcap_c.id;       
                cmd.hdr.lcap_c.cmd_len  = 12;                       

                cmd.info_type   = type;
                cmd.result      = 0;
                cmd.data[0]     = 0x02;     // // bit 2 is the l2cap fixed channel
                cmd.data[1]     = 0;
                cmd.data[2]     = 0;
                cmd.data[3]     = 0;
                cmd.data[4]     = 0;
                cmd.data[5]     = 0;
                cmd.data[6]     = 0;
                cmd.data[7]     = 0;
    
                log_info("<--  LCAP - LCAP_INFO_RESPONSE[%d]",cmd.hdr.lcap_c.id);
                lcapLayer_sendData(&cmd,sizeof(cmd));
            }
            else
            {
                log_error("        UNHANDLED INFORMATION REQUEST TYPE(%d)",type);
            }

            break;
        }
        
        case LCAP_COMMAND_REJECT :
        {
            lcap_command_reject *reject = (lcap_command_reject *) buffer;
            u16 reason = reject->reason;
            log_indented(8, "lcap_c.id=[%d] hci_handle(0x%04x) reason=0x%04x %s",
                reject->hdr.lcap_c.id,
                reject->hdr.hci.handle,
                reason,
                reason == 0 ? "command not understood" :
                reason == 1 ? "mtu exceeded" :
                reason == 2 ? "invalid cid in request" :
                "other" );
            if (reason == 1)
                log_indented(8, "max_mtu=%d\n",reject->max_mtu);
            else if (reason == 2)
                log_indented(8, "lcid=0x%04x    rcid=0x%04x\n",reject->lcid,reject->rcid);
            break;
        }
        
        case LCAP_DISCONNECTION_REQUEST :
        {
            lcap_disconnect_request *req = (lcap_disconnect_request *) buffer;  
            log_indented(8,"lcap_c.id=[%d]  hci_handle(0x%04x) dest_cid=0x%04x src_cid=0x%04x",
                req->hdr.lcap_c.id,
                req->hdr.hci.handle,
                req->src_cid,
                req->dest_cid);
            lcapConnection *lcn = lcapLayer_findLocalCid(hci_context, req->hdr.hci.handle,req->dest_cid);
            if (!lcn) return;
            if (lcn->pClient)
                lcn->pClient->lcapEvent(lcn->pClient, LCAP_EVENT_DISCONNECTED,lcn,0,0);
            
            // the disconnection response uses the same data
            // structure as the disconnection request
            
            lcap_disconnect_request cmd;
            cmd.hdr.hci.handle = req->hdr.hci.handle | 0x2000;  // word - handle or'd with flags
            cmd.hdr.lcap_d.cid = LCAP_CID;
            cmd.hdr.lcap_c.cmd = LCAP_DISCONNECTION_RESPONSE;
            cmd.hdr.lcap_c.id = req->hdr.lcap_c.id;     // byte - packet number = unique id
            cmd.hdr.lcap_c.cmd_len = 4;
            cmd.dest_cid = req->dest_cid;
            cmd.src_cid  = req->src_cid;
            log_info("<--  LCAP - LCAP_DISCONNECT_RESPONSE hci_handle(0x%04x) lcid=0x%04x local_cid=0x%04x",
                req->hdr.hci.handle,
                cmd.hdr.lcap_c.id,
                lcn->local_cid);
            lcapLayer_sendData(&cmd,sizeof(cmd));
            lcapLayer_deleteConnection(l2cap_ctx, lcn, true);
            break;
        }
        
        case LCAP_DISCONNECTION_RESPONSE    :
        {
            lcap_disconnect_request *req = (lcap_disconnect_request *) buffer;  
            log_indented(8, "lcap_c.id=[%d]  hci_handle(0x%04x) dest_cid=0x%04x src_cid=0x%04x",
                req->hdr.lcap_c.id,
                req->hdr.hci.handle,
                req->src_cid,
                req->dest_cid);
            lcapConnection *lcn = lcapLayer_findLocalCid(hci_context, req->hdr.hci.handle,req->src_cid);
            if (!lcn) return;
            if (lcn->pClient)
                lcn->pClient->lcapEvent(lcn->pClient, LCAP_EVENT_DISCONNECTED,lcn,0,0);
            lcapLayer_deleteConnection(l2cap_ctx, lcn,true);
            break;
        }
    
        default:
            log_error("      UNHANDLED LCAP DATA COMMAND=0x%02x", hdr->lcap_c.cmd);
            break;
    }
}



lcapConnection* lcapLayer_startConnection(l2cap_context_t *l2cap_ctx, u8 *addr, u16 psm) 
{
    log_indented(8, "startLcapConnection(%s,0x%04x\n)",addrToString(addr),psm);

    hciRemoteDevice *device = hciLayer_startConnection(l2cap_ctx->hci_context, addr);

    if (!device) return 0;
    assert(!(device->handle & HCI_HANDLE_ERROR));
    u16 hci_handle = device->handle & 0x0fff;

    // create the lcap connection record
    lcapConnection *lcn = lcapLayer_addConnection(l2cap_ctx, device, psm, 0 );
    assert(lcn);
    if (!lcn) return 0;

    // if hci's got a valid handle
    // start the lcap connection now
    if (!hci_handle)
    {
        lcn->lcap_state |= LCAP_STATE_CONNECTING;
    }
    else
    {
        lcap_connect_request cmd;
        
        cmd.hdr.hci.handle = hci_handle | 0x2000;   // word - handle or'd with flags
        cmd.hdr.lcap_d.cid = LCAP_CID;
        cmd.hdr.lcap_c.cmd = LCAP_CONNECTION_REQUEST;
        cmd.hdr.lcap_c.id = device->next_lcap_id++;     // byte - packet number = unique id
        cmd.hdr.lcap_c.cmd_len = 4;
        cmd.psm = psm;
        cmd.src_cid = lcn->local_cid;

        log_info("<-- LCAP - LCAP_CONNECTION_REQUEST[%d] hci_handle(0x%04x) lcid=0x%04x", hci_handle, cmd.hdr.lcap_c.id, lcn->local_cid);

        lcapLayer_sendData( &cmd, sizeof(cmd) );
    }
    
    return lcn;
}

void lcapLayer_closeConnection(lcapConnection *lcn) 
{
    assert(lcn && lcn->device);
    lcap_disconnect_request cmd;
    
    cmd.hdr.hci.handle = lcn->device->handle | 0x2000;  // word - handle or'd with flags
    cmd.hdr.lcap_d.cid = LCAP_CID;
    cmd.hdr.lcap_c.cmd = LCAP_DISCONNECTION_REQUEST;
    cmd.hdr.lcap_c.id = lcn->device->next_lcap_id++;        // byte - packet number = unique id
    cmd.hdr.lcap_c.cmd_len = 4;
    cmd.dest_cid = lcn->remote_cid;
    cmd.src_cid = lcn->local_cid;

    log_info("<-- LCAP - LCAP_DISCONNECT_REQUEST[%d] hci_handle(0x%04x) lcid=0x%04x",
        lcn->device->handle,
        cmd.hdr.lcap_c.id,
        lcn->local_cid);
    lcapLayer_sendData(&cmd, sizeof(cmd));

}



void lcapLayer_registerClient(l2cap_context_t *layer, lcapClient *pClient) 
{
    log_info("l2cap: Registering client: %s...", pClient->serviceName());

printf("test\n");

    if (layer->m_num_clients < MAX_LCAP_CLIENTS) {
        layer->m_pClients[layer->m_num_clients++] = pClient;
        log_info("l2cap: Registering client: %s Result: Success, Client Num:%d", 
            pClient->serviceName(), layer->m_num_clients - 1);
        return;
    }

    log_error("l2cap: Registering client: %s Result: Failed", 
        pClient->serviceName());
}



void lcapLayer_destroy(l2cap_context_t *layer) {
    // Proper cleanup, including freeing clients
    for (int i = 0; i < layer->m_num_clients; i++) {
        free(layer->m_pClients[i]);
    }
    free(layer);
}

//--------------------------------------------
 
l2cap_context_t * l2cap_init(hci_context_t *hci_context) {

    l2cap_context_t *l2cap_ctx = (l2cap_context_t *)malloc(sizeof(l2cap_context_t));
    if (!l2cap_ctx) return NULL;

// FIXME
//mylcapLayer = l2cap_ctx;

    l2cap_ctx->m_num_clients = 0;
    memset(l2cap_ctx->m_pClients, 0, sizeof(l2cap_ctx->m_pClients));
    memset(l2cap_ctx->m_connections, 0, sizeof(l2cap_ctx->m_connections));


    //context->mtu = 672; // Default L2CAP MTU
    l2cap_ctx->hci_context = hci_context;
    
    // Register L2CAP's callback function with the HCI layer
    hci_register_data_callback(hci_context, l2cap_handle_hci_data, l2cap_ctx);

    return l2cap_ctx;
}
