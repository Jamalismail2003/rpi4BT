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
#include "sdp.h"
#include "l2cap.h"
#include "hci.h"
#include "sdp_defs.h"



#define log_name "SDP"

#define TRACE_SDP   0

static void lcapEvent(void *user_data, u16 event_type, lcapConnection *lcn,  const u8* buffer,  u16 length);

static inline u16 be(u16 v) // big endian = MSB first
{
    return ((v & 0xff)<< 8) | (v >> 8);
}

u16 sdpDiscoveryRequest(u8 *buf, sdpRequest *req);
    // forward of static method
extern void parseServiceRecords(sdpRequest *req);
    // currently just an external static parser
    

#define SET_BYTE(p,b)           *p++ = ((u8)b)
#define SET_WORD(p,w)           { *((u16 *)p) = ((u16)w); p += 2; }
#define SET_COPY(d,s,l)         { memcpy(d,s,l); d+=l; }
//#define PACKET_LEN(b,p)           ((u32)p)-((u32)b)
#define PACKET_LEN(b,p) ((uint32_t)((p) - (b)))


u8  service_reply[] =
{
    0x00, 0x00,   // word - handle or'd with flags
    0x00, 0x00,   // hci length (filled in later)
    0x00, 0x00,   // lcap length (filled in later)
    0x00, 0x00,   // destination channel (remote cid)
    
    SDP_COMMAND_SEARCH_ATTR_RESPONSE,   // command byte == 0x06 = Search for "attributes" of ...
    
    0x00, 0x00,   // txn id = SDP uses word size packet id's
    0x00, 0xf9,   // sdp parameter length (this includes continuation byte ... is that correct?)
    0x00, 0xf6,   // attribute list byte count (does not include cont byte)
    
    // list of service records
    
    0x35, 0xf4,     // 152+2  + 88+2 = 244 = 0xf4
    
    // sdp service record
    
    0x35, 0x98,                                             // attribute list(152)
          0x09, 0x00, 0x00,                                 //    attr(0x0000,SrvRecHndl)
                0x0a, 0x00, 0x00, 0x00, 0x00,               //         uint32 0x00000000
          0x09, 0x00, 0x01,                                 //    attr(0x0001,SrvClassIDList)
                0x35, 0x03,                                 //        sequence(3)
                      0x19, 0x10, 0x00,                     //             uuid-16 0x1000 (SDPServer)
          0x09, 0x00, 0x04,                                 //    attr(0x0004,ProtocolDescList)
                0x35, 0x0d,                                 //        sequence(13)
                      0x35, 0x06,                           //            sequence(6)
                            0x19, 0x01, 0x00,               //                 uuid-16 0x0100 (L2CAP)
                            0x09, 0x00, 0x01,               //                 uint16 0x0001
                      0x35, 0x03,                           //            sequence(3)
                            0x19, 0x00, 0x01,               //                 split uuid-16 0x0001 (SDP)
          0x09, 0x00, 0x05,                                 //    attr(0x0005,BrwGrpList)
                0x35, 0x03,                                 //        sequence(3)
                      0x19, 0x10, 0x02,                     //             uuid-16 0x1002 (BrowseGroup)
          0x09, 0x00, 0x06,                                 //    attr(0x0006,LangBaseAttrIDList)
                0x35, 0x09,                                 //        sequence(9)
                      0x09, 0x65, 0x6e,                     //             uint16 0x656e
                      0x09, 0x00, 0x6a,                     //             uint16 0x006a
                      0x09, 0x01, 0x00,                     //             uint16 0x0100
          0x09, 0x01, 0x00,                                 //    attr(0x0100,SrvName)
                0x25, 0x12,                                 //         str "Service Discovery"
                      0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20,
                      0x44, 0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 0x72,
                      0x79, 0x00,
          0x09, 0x01, 0x01,                                 //    attr(0x0101,SrvDesc)
                0x25, 0x25,                                 //        str "Publishes services to remote devices"
                      0x50, 0x75, 0x62, 0x6c, 0x69, 0x73, 0x68, 0x65,
                      0x73, 0x20, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63,
                      0x65, 0x73, 0x20, 0x74, 0x6f, 0x20, 0x72, 0x65,
                      0x6d, 0x6f, 0x74, 0x65, 0x20, 0x64, 0x65, 0x76,
                      0x69, 0x63, 0x65, 0x73, 0x00,
          0x09, 0x01, 0x02,                                //    attr(0x0102,ProviderName)
                0x25, 0x0a,                                //         str "Microsoft" 
                      'r','P','i','S','D','P','S','v','c', 0x00,      // rPiSDPSvc
                      //0x4d, 0x69, 0x63, 0x72, 0x6f, 0x73, 0x6f, 0x66,
                      //0x74, 0x00,
          0x09, 0x02, 0x00,                                //    attr(0x0200,VersionNumList)
                0x35, 0x03,                                //        sequence(3)
                      0x09, 0x01, 0x00,                    //             uint16 0x0100
          0x09, 0x02, 0x01,                                //    attr(0x0201,SrvDBState)
                0x0a, 0x00, 0x00, 0x00, 0x08,              //         uint32 0x00000008

    // spp service record

    0x35, 0x58,                                             // attribute list(88)
          0x09, 0x00, 0x00,                                 //      attr(0x0000,SrvRecHndl)
                0x0a, 0x00, 0x01, 0x00, 0x07,               //           uint32 0x00010007
          0x09, 0x00, 0x01,                                 //      attr(0x0001,SrvClassIDList)
                0x35, 0x03,                                 //          sequence(3)
                      0x19, 0x11, 0x01,                     //               uuid-16 0x1101 (SP)
          0x09, 0x00, 0x04,                                 //      attr(0x0004,ProtocolDescList)
                0x35, 0x0c,                                 //          sequence(12)
                      0x35, 0x03,                           //              sequence(3)
                            0x19, 0x01, 0x00,               //                   uuid-16 0x0100 (L2CAP)
                      0x35, 0x05,                           //              sequence(5)
                            0x19, 0x00, 0x03,               //                   split uuid-16 0x0003 (RFCOMM)
                            0x08, 0x11,                     //                   uint8  0x03    (PRH - ours is 0x11 to be different)
            
        // OUR INCOMING SPP RFCOMM CHANNEL above: 0x11 = 17
    
          0x09, 0x00, 0x05,                                 //      attr(0x0005,BrwGrpList)
                0x35, 0x03,                                 //          sequence(3)
                      0x19, 0x10, 0x02,                     //               uuid-16 0x1002 (BrowseGroup)
          0x09, 0x00, 0x06,                                 //      attr(0x0006,LangBaseAttrIDList)
                0x35, 0x09,                                 //          sequence(9)
                      0x09, 0x65, 0x6e,                     //               uint16 0x656e
                      0x09, 0x00, 0x6a,                     //               uint16 0x006a
                      0x09, 0x01, 0x00,                     //               uint16 0x0100
          0x09, 0x00, 0x09,                                 //      attr(0x0009,BTProfileDescList)
                0x35, 0x08,                                 //          sequence(8)
                      0x35, 0x06,                           //              sequence(6)
                            0x19, 0x11, 0x01,               //                   uuid-16 0x1101 (SP)
                            0x09, 0x01, 0x02,               //                   uint16 0x0102
          0x09, 0x01, 0x00,                                 //      attr(0x0100,SrvName)
                0x25, 0x05, 'R','P','I','0','1',            //           str "COM27"
                // 0x25, 0x05, 0x43, 0x4f, 0x4d, 0x32, 0x37,   //           str "COM27"
          0x09, 0x01, 0x01,                                 //      attr(0x0101,SrvDesc)
                0x25, 0x05, 'R','P','I','0','1',            //           str "COM27"
                // 0x25, 0x05, 0x43, 0x4f, 0x4d, 0x32, 0x37,    //           str "COM27"
                
    // continuation byte
    
    0x00
};        

u32 addService(
    sdp_context_t *sdp_ctx,
    u16 uuid_service,       // must be SDP or SP for now
    u16 uuid_protocol,      // L2CAP protocol automatically added first
    const char *service_name,
    const char *service_desc,
    u8 rfcomm_channel)
{
    if (sdp_ctx->m_num_services >= MAX_SERVICES)
    {
        log_error("SDP: No more room for more services!");
        return 0;
    }
    localServiceRecord *service = &sdp_ctx->m_services[sdp_ctx->m_num_services++];
    service->service_id = sdp_ctx->m_next_service_id++;
    service->uuid_service   = uuid_service;        
    service->uuid_protocol  = uuid_protocol;  
    service->service_name   = service_name;
    service->service_desc   = service_desc;
    service->rfcomm_channel = rfcomm_channel;
    return service->service_id;
}

const char *sdpCommandToStr(u8 command)
{
    if (command == SDP_COMMAND_ERROR_RESPONSE)          return "0x01=SDP_ERROR_RESPONSE";
    if (command == SDP_COMMAND_SEARCH_REQUEST)          return "0x02=SDP_SEARCH_REQUEST";
    if (command == SDP_COMMAND_SEARCH_RESPONSE)         return "0x03=SDP_SEARCH_RESPONSE";
    if (command == SDP_COMMAND_ATTR_REQUEST)            return "0x04=SDP_ATTR_REQUEST";
    if (command == SDP_COMMAND_ATTR_RESPONSE)           return "0x05=SDP_ATTR_RESPONSE";
    if (command == SDP_COMMAND_SEARCH_ATTR_REQUEST)     return "0x06=SDP_SEARCH_ATTR_REQUEST";
    if (command == SDP_COMMAND_SEARCH_ATTR_RESPONSE)    return "0x07=SDP_SEARCH_ATTR_RESPONSE";
    return "UNKNOWN SDP COMMAND";
}

//sdpRequest *sdpLayer::findRequest(lcapConnection *lcn, bool quiet)
static sdpRequest *findRequest(sdp_context_t *sdp_ctx, lcapConnection *lcn, bool quiet)
{
    assert(lcn);
    for (u16 i=0; i < MAX_SDP_REQUESTS; i++)
    {
        sdpRequest *r = &sdp_ctx->m_requests[i];
        if (r->lcn == lcn)
            return r;
    }
    if (!quiet)
    {
        log_error("SDP: Could not find sdpRequest for connection %s:0x%04x  lcid(0x%04x)\n",
            lcn->device ? addrToString(lcn->device->addr) : "unknown device",
            lcn->device ? lcn->device->handle : 0xffff,
            lcn->local_cid);
    }
    return 0;
}


//void sdpLayer::deleteRequest(sdpRequest *request)
static void deleteRequest(sdpRequest *request)
{
    assert(request && request->lcn && request->lcn->device);
    
    log_indented(8, "SDP: deleteRequest[0x%04x] to (0x%04x:%s) %s",
        request->txn_id,
        request->lcn->device->handle,
        addrToString(request->lcn->device->addr),
        request->lcn->device->name);
    
    for (u8 i=0; i<request->num_frames; i++)
    {
        sdpResultFrame *frame = &request->frame[i];
        if (frame->data) free(frame->data);
        if (frame->cont) free(frame->cont);
    }
    memset(request,0,sizeof(sdpRequest));
}



//----------------------------------------------------
// lcap Event Handler
//----------------------------------------------------
static void lcapEvent(void *user_data, u16 event_type, lcapConnection *lcn,  const u8* buffer,  u16 length)
{
    assert(lcn->device);
    sdp_context_t *sdp_ctx = user_data;
    
    log_indented(8, "SDP: event(%s) hci(0x%04x) cid(0x%04x) len=%d\n",
        lcapEventName(event_type),
        lcn->device ? lcn->device->handle : 0xffff,
        lcn->local_cid, length);
    
    switch (event_type)
    {
        case LCAP_EVENT_DISCONNECTED :
            {
                assert(lcn);
                assert(lcn->device);
                sdpRequest *req = findRequest(sdp_ctx, lcn,true);
                if (!req) return;
                deleteRequest(req);
                break;
            }
        case LCAP_EVENT_CONNECTED :
            {
                assert(lcn);
                assert(lcn->device);
                sdpRequest *req = findRequest(sdp_ctx, lcn,true);
                if (!req) return;

                log_indented(8, "SDP: starting pending sdpRequest hci_handle(0x%04x) lcid(0x%04x) rcid(0x%04x)\n",
                    lcn->device->handle,
                    lcn->local_cid,
                    lcn->remote_cid);

                u8 buf[HCI_MAX_PACKET_SIZE];
                u16 len = sdpDiscoveryRequest(buf,req);
                log_info("<--  SDP: SDP_SEARCH_ATTR_REQUEST[%d] lcid=0x%04x svc_id=0x%04x begin(0x%04x) end(0x%04x)\n",
                    buf[9],
                    lcn->local_cid,
                    req->svc_id,
                    req->begin_attr,
                    req->end_attr);
                //display_bytes("<lcap", buf, len);
            lcapLayer_sendData(buf, len);       //m_pLCAP->sendData(buf,len);
            break;
          }
        
        case LCAP_EVENT_DATA :
            {
                //display_bytes(">sdp data", buffer, length);
                const u8 *p = buffer;
                u8 cmd = *p++;
                if (cmd == SDP_COMMAND_SEARCH_ATTR_RESPONSE)
                {
                    sdpRequest *req = findRequest(sdp_ctx, lcn,false);
                    if (!req) return;

                    u16 txn_id = be(*(u16 *) p);
                    p += 2;
                    u16 sdp_len = be(*(u16 *) p);
                    p += 2;
                    u16 attr_len = be(*(u16 *) p);
                    p += 2;
                    const u8 *cont = p + attr_len;

                    log_indented(8, "   SDP_SEARCH_ATTR_RESPONSE(%d)\n",cmd);
                    log_indented(8, "      txn_id(0x%04x)\n",txn_id);
                    log_indented(8, "      sdp_len(%d)\n",sdp_len);
                    log_indented(8, "      attr_len(%d)\n",attr_len);
                //display_bytes("      cont:",(u8 *)cont,sdp_len-attr_len-2);

                // allocated and save off the data

                    u16 frame_num = req->num_frames - 1;
                    sdpResultFrame *frame = &req->frame[frame_num];
                    assert(txn_id == frame->txn_id);

                    frame->data = (u8 *) malloc(attr_len);
                    assert(frame->data);
                    if (!frame->data) return;
                    frame->len = attr_len;

                    memcpy(frame->data, p, attr_len);

                // if there's a continuation indicator, add a frame
                // set it up, and do another request

                    if (*cont)
                    {
                        sdpResultFrame *next_frame = &req->frame[req->num_frames++];                        
                        next_frame->txn_id = sdp_ctx->m_next_txn_id++;
                        next_frame->cont_len = *cont++;
                        next_frame->cont = (u8 *) malloc(next_frame->cont_len);
                        assert(next_frame->cont);
                        if (!next_frame->cont) return;
                        memcpy(next_frame->cont,cont,next_frame->cont_len);

                        u8 buf[HCI_MAX_PACKET_SIZE];
                        u16 len = sdpDiscoveryRequest(buf,req);
                        log_info("<--  SDP: SDP_SEARCH_ATTR_REQUEST(continued) CONT[%d] lcid=0x%04x svc_id=0x%04x begin(0x%04x) end(0x%04x)\n",
                            buf[9],
                            lcn->local_cid,
                            req->svc_id,
                            req->begin_attr,
                            req->end_attr);
                        //display_bytes("<lcap",buf,len);
                        lcapLayer_sendData(buf, len); //m_pLCAP->sendData(buf,len);
                    }
                    else
                    {
                        lcapLayer_closeConnection(lcn); //m_pLCAP->closeConnection(lcn);
                        log_indented(8, "SDP: Parsing attributes\n");
                        parseServiceRecords(req);
                    }
                }
            else if (cmd == SDP_COMMAND_SEARCH_ATTR_REQUEST)
            {
                u16 txn_id = be(*(u16 *) p);
                p += 2;
                u16 sdp_len = be(*(u16 *) p);
                p += 2;

                log_indented(8, "   SDP_COMMAND_SEARCH_ATTR_REQUEST[%d] sdp_len(%d)\n",
                    txn_id,sdp_len);
                
                //  assert(sdp_len == 0x0f);
                //  assert(p[0] == 0x35);
                //  assert(p[1] == 0x03);
                //  assert(p[2] == 0x19);
                //  
                //  if (sdp_len == 0x0f && 
                //      p[0] == 0x35 && 
                //      p[1] == 0x03 &&
                //      p[2] == 0x19)
                //  {
                //      p += 3;
                //      u16 uuid = be(*(u16 *) p);
                //      p += 2;
                //      u16 max_len =  be(*(u16 *) p);
                //      p += 2;
                //  
                //      assert(p[0] == 0x35);
                //      assert(p[1] == 0x05);
                //      assert(p[2] == 0x0a);
                //      
                //      if (p[0] == 0x35 &&
                //          p[1] == 0x05 &&
                //          p[2] == 0x0a)
                //      {
                //          p += 3;
                //          u16 range_low = be(*(u16 *) p);
                //          p += 2;
                //          u16 range_high = be(*(u16 *) p);
                //          p += 2;
                //          
                //          PRINT_SDP("   uuid(0x%04x) low(0x%04x) high(0x%04x) max(%d)\n",
                //              uuid,range_low,range_high,max_len);
                //          
                //          // send an empty SDP response
                //  
                //          if (1 || uuid == UUID_PROTO_L2CAP)
                //          {
                log_indented(8, "SDP: sending full search response");
                u8 *p = service_reply;
                            SET_WORD(p,lcn->device->handle | 0x2000);   // word - handle or'd with flags
                            SET_WORD(p,0);                              // hci length (filled in later)
                            SET_WORD(p,0);                              // lcap length (filled in later)
                            SET_WORD(p,lcn->remote_cid);                // destination channel (remote)
                            SET_BYTE(p,SDP_COMMAND_SEARCH_ATTR_RESPONSE);// command byte == 0x06 = Search for "attributes" of ...
                            SET_WORD(p,be(txn_id));                 // id = SDP uses word size packet id's
                            u16 *test_len = (u16 *) p;
                            
                            if (be(*test_len) != sizeof(service_reply) -4-4-5)
                                log_info("<--  SDP: test_len(%d) != sizeof(service_reply)=%ld -4-4-5=%ld\n",
                                    be(*test_len),
                                    sizeof(service_reply),
                                    sizeof(service_reply) -4-4-5);
                            
                            lcapLayer_sendData(service_reply, sizeof(service_reply)); //m_pLCAP->sendData(service_reply,sizeof(service_reply));

                        //  }
                        //  else
                        //  {
                        //      LOG_SDP("sending empty search response",0);                     
                        //      u8 buf[512];
                        //      u8 *p = buf;
                        //      
                        //      SET_WORD(p,lcn->device->handle | 0x2000);   // word - handle or'd with flags
                        //      SET_WORD(p,0);                              // hci length (filled in later)
                        //      SET_WORD(p,0);                              // lcap length (filled in later)
                        //      SET_WORD(p,lcn->remote_cid);                // destination channel (remote)
                        //      SET_BYTE(p,SDP_COMMAND_SEARCH_ATTR_RESPONSE);// command byte == 0x06 = Search for "attributes" of ...
                        //      SET_WORD(p,be(txn_id));                 // id = SDP uses word size packet id's
                        //      
                        //      SET_WORD(p,be(0));                  // parameter length (0)
                        //      SET_BYTE(p,0);                      // no continuation byte
                        //  
                        //      u16 len = PACKET_LEN(buf,p);
                        //      m_pLCAP->sendData(buf,len);
                        //  }                       
                    // }
                // }

                        }
                        else
                        {
                            log_error("SDP: unhandled SDP DATA EVENT(0x%02x=%s) hci(0x%04x) cid(0x%04x) len=%d",
                                cmd,sdpCommandToStr(cmd),lcn->device->handle, lcn->local_cid, length);
                        }
                        break;
                    }            

                default:
                    log_error("SDP: unhandled SDP LCAP EVENT(%s) hci(0x%04x) cid(0x%04x) len=%d",
                        lcapEventName(event_type),lcn->device->handle, lcn->local_cid, length);
                }
            }


//--------------------------------------------------------
// static packet method
//--------------------------------------------------------


u16 sdpDiscoveryRequest(u8 *buf, sdpRequest *req)
    // the "search attributes" command, consists of
    //
    //       hci_header
    //       lcap_header
    //       command_byte
    //          DE   = variable length DE for the services to search
    //          WORD = 'bytes I'll accept back'
    //          DE   = variable length DE describing the atrribute I'm lookinng for
    //       continuation byte(s)
    //
    // Thus far I have only gotten it to work with LCAP_UUID on the
    // first lcap channel I open to the remote.
{
    
    assert(req);
    lcapConnection *lcn = req->lcn;
    assert(lcn->device);

    sdpResultFrame *frame = &req->frame[req->num_frames-1];
    
    u8 *p = buf;
    u16 *sdp_len = 0;

    SET_WORD(p,lcn->device->handle | 0x2000);   // word - handle or'd with flags
    SET_WORD(p,0);                              // hci length (filled in later)
    SET_WORD(p,0);                              // lcap length (filled in later)
    SET_WORD(p,lcn->remote_cid);                // destination channel (remote)
    SET_BYTE(p,SDP_COMMAND_SEARCH_ATTR_REQUEST);// command byte == 0x06 = Search for "attributes" of ...
    SET_WORD(p,be(frame->txn_id));              // id = SDP uses word size packet id's
    
    sdp_len = (u16 *) p;
    SET_WORD(p,be(0));                  // parameter length (filled in later)
        
    SET_BYTE(p,0x35);                   // 1st data element is a Data Element Sequence, it's data size in next 8 bits
    SET_BYTE(p,0x03);                   // the data element is 3 bytes long
    SET_BYTE(p,0x19);                   // 0x19 = type(00011)=UUID,  size(001)=16bits
    SET_WORD(p,be(req->svc_id));        // service uuid to look for
            
    SET_WORD(p,be(0x0ffff));            // we'll take as many result bytes as possible
        
    SET_BYTE(p,0x35);                   // Data Element Sequence, data size in next 8 bits
    SET_BYTE(p,0x05);                   // Data size
    SET_BYTE(p,0x0A);                   // nested DE type(00001) size(010) = unsigned int, 4 bytes
    SET_WORD(p,be(req->begin_attr));    // range from 0x0000...
    SET_WORD(p,be(req->end_attr));      // ... to 0xFFFF

    SET_BYTE(p,frame->cont_len);        // set continuation bytes if provided
    if (frame->cont_len)
        SET_COPY(p,frame->cont,frame->cont_len);

    // finished, set the lengths and return

    u16 len = PACKET_LEN(buf,p);
    *sdp_len = be(len -4-4-5);

    return len;
}


//----------------------------------------------------
// client API
//----------------------------------------------------

sdpRequest *sdpLayer_doSdpRequest(l2cap_context_t *l2cap_ctx, const u8 *addr, u16 svc_id,  u16 begin_attr,  u16 end_attr )
{
    log_indented(8, "SDP: sdp_doSdpRequest(%s, 0x%04x, 0x%04x, 0x%04x)", addrToString(addr), svc_id, begin_attr, end_attr);    

    sdp_context_t *sdp_ctx = (sdp_context_t *)lcapLayer_findClientByPSM(l2cap_ctx, SDP_PSM);

    sdpRequest *req = 0;
    for (u16 i=0; i<MAX_SDP_REQUESTS; i++)
    {
        sdpRequest *r = &sdp_ctx->m_requests[i];
        if (!r->txn_id)
        {
            req = r;
            break;
        }
    }
    if (!req)
    {
        perror("no room for more sdp requests");
        return 0;
    }

    lcapConnection *lcn = lcapLayer_startConnection(l2cap_ctx, addr, SDP_PSM);
    
    if (!lcn) return 0;
    
    memset(req,0,sizeof(sdpRequest));

    req->txn_id       = sdp_ctx->m_next_txn_id++;
    req->svc_id       = svc_id;
    req->begin_attr   = begin_attr;
    req->end_attr     = end_attr;    
    req->lcn          = lcn;

    req->num_frames = 1;
    req->frame[0].txn_id = req->txn_id;

    return req;
}


static u16 getPSM()
{
    return SDP_PSM;
}

static const char *serviceName()
{
    return "SDP";
}

int sdp_init(l2cap_context_t *l2cap_ctx)
{
    sdp_context_t *sdp_ctx = (sdp_context_t *)malloc(sizeof(sdp_context_t));
    if(!sdp_ctx) {
        log_error("Failed to allocate memory\n");
        return -1;
    }

    memset(sdp_ctx->m_requests, 0, MAX_SDP_REQUESTS * sizeof(sdpRequest));
    sdp_ctx->m_next_txn_id = 0xAAAA;

    memset(sdp_ctx->m_services, 0, MAX_SERVICES * sizeof(localServiceRecord));
    sdp_ctx->m_next_service_id = 0x0001000;
    sdp_ctx->m_num_services = 0;

    addService(sdp_ctx,
        UUID_SERVICE_SDP_SERVER,
        UUID_PROTO_SDP,
        "rpi SDP Server",
        "serves SDP for rPi",
        0);

    addService(sdp_ctx,
        UUID_SERVICE_SERIAL_PORT,
        UUID_PROTO_RFCOMM,
        "rpiSPP",
        "rpiCOM1",
        0x07);      // we gonna use channel 7


    sdp_ctx->base.getPSM = getPSM;
    sdp_ctx->base.serviceName = serviceName;
    sdp_ctx->base.lcapEvent = lcapEvent;

    lcapLayer_registerClient(l2cap_ctx, (lcapClient*)sdp_ctx);

    sdp_ctx->l2cap_ctx = l2cap_ctx;

    return 0;
}
