#ifndef RFCOMM_H
#define RFCOMM_H

#include <stdint.h>
#include "l2cap.h"

// The usage model for this RFCOMM layer has not been determined.
//
// Theoretically there can be multiple different services above this,
// which each register a channel for incoming connections. For outgoing
// connections those services will have used SDP to find out (or even
// let the user select) a rfcomm channel number on the remote machine
// of an appropriate service.
//
// My ad-hoc SDP layer always advertises an SPP service on channel 17,
// and I know, from doing manual SDP requests, the channel numbers to
// a few valid SPP services on specific other machines.
//
// So, in lieu of implementing an SPP service that uses SDP to find
// sister service's rfcomm channel numberss on remote machines, which
// only accepts connections on channel 17, and which then presents
// each channel, incoming or outgoing, as a separate io stream to
// yet another level of client, I am just glomming them all together
// into a single callback to my test framework (for incoming data),
// and make the connection table public, so the test framework can
// send example output to ALL open channels.
//
// So the public API is temporary, and the missing ability to
// register a service, as well as the whole SPP layer using SDP
// is, as of yet, missing.

#define RF_MAX_CHANNELS  5
#define RF_MAX_SESSIONS  5


// ANALYSIS_CHANGE_NUMBER_3
// added PN_SENT and PN_RECEIVED before SABM for non-zero channels

#define RF_CHANNEL_STATE_NONE                       0x00000

#define RF_CHANNEL_STATE_PN_COMMAND_SENT            0x00001  // required
#define RF_CHANNEL_STATE_PN_RESPONSE_RECEIVED       0x00002  // required
#define RF_CHANNEL_STATE_PN_COMMAND_RECEIVED        0x00004  // optional (for incoming)
#define RF_CHANNEL_STATE_PN_RESPONSE_SENT           0x00008  // optional (for incoming)
#define RF_CHANNEL_STATE_SABM                       0x00010  // sent or received
#define RF_CHANNEL_STATE_UA                         0x00020  // received or sent
#define RF_CHANNEL_STATE_SIGNALS_SENT               0x00100
#define RF_CHANNEL_STATE_SIGNALS_RECEIVED           0x00200
#define RF_CHANNEL_STATE_SIGNAL_RESPONSE_RECEIVED   0x00400
#define RF_CHANNEL_STATE_SIGNAL_RESPONSE_SENT       0x00800
#define RF_CHANNEL_STATE_RPN_COMMAND_SENT           0x01000  // required
#define RF_CHANNEL_STATE_RPN_RESPONSE_RECEIVED      0x02000  // required
#define RF_CHANNEL_STATE_RPN_COMMAND_RECEIVED       0x04000  // optional (for incoming)
#define RF_CHANNEL_STATE_RPN_RESPONSE_SENT          0x08000  // optional (for incoming)
#define RF_CHANNEL_STATE_OPEN                       0x10000
#define RF_CHANNEL_STATE_CLOSING                    0x20000
#define RF_CHANNEL_STATE_CLOSED                     0x40000
#define RF_CHANNEL_STATE_INCOMING                   0x80000

    // whether the SABMs were initially sent, or received, by this
    // session/channel determines the format of the outgoing
    // OUTER LEVEL C/R bits or'd into the address byte. If it was
    // not incoming, then we are sending COMMANDS, and the OUTER
    // LEVEL address c/r bit is set to 1.  If it was incoming, then
    // we are sending RESPONSES (even if they contain sub-commands)
    // and the bit is cleared.

typedef struct rfChannel rfChannel;
typedef struct rfSession rfSession;

typedef struct
{
    u8 signals;
    u8 breaks;      // not used on any machine I connect to
} rfSignals;



struct rfChannel
{
    rfSession *session;
    u8      channel_num;        // 0 is special, 1 is reserved,
        // 0x11 (17) is our "SPP" service,
        // 0x03 is SPP on lenovo,
        // 0x02 SPP on android
        
    u32     channel_state;      // state machine bits
    u8      priority;           // always 0 
    u16     mtu;                // 0x200 for us
    u8      flags;              // not used
    
    rfSignals remote;
    rfSignals local;
};

struct rfSession
{
    lcapConnection *lcn;
    rfChannel channel[RF_MAX_CHANNELS];       
};


typedef void (*rfCallback)(rfChannel *channel, const u8 *buffer, u16 length);

typedef struct {
    lcapClient base;
    rfSession m_sessions[RF_MAX_SESSIONS];
    rfCallback m_pClient;   
    void      *m_pClientThis;
    l2cap_context_t *l2cap_ctx;
} rfcomm_context_t;

void rfcommLayer_create(l2cap_context_t *pLCAP);

rfChannel *openRFChannel(l2cap_context_t *l2cap_ctx, u8 *addr, u8 channel_num);
void closeRFChannel(rfChannel *channel);
void rfLayer_sendData(rfChannel *channel, const u8 *data, u16 len);
int getOpenChannels(l2cap_context_t *l2cap_ctx, rfChannel **buf, int max);

rfSession *addSession(rfcomm_context_t *rf_ctx, lcapConnection *lcn);
void deleteSession(rfSession *session);
rfSession *findSession(rfcomm_context_t *rf_ctx, lcapConnection *lcn);

rfChannel *addChannel(rfSession *session, u8 channel_num);
void deleteChannel(rfChannel *channel);
rfChannel *findChannel(rfSession *session, u8 channel_num);

void sendPacket(rfChannel *channel, bool command, u8 frame_type, u8 *data, u16 data_len);

void rfEventHandler(rfcomm_context_t *rf_ctx, lcapConnection *lcn, const u8 *buffer, u16 length);
void processChannel(rfChannel *channel);

void controlModemStatus(rfChannel *channel, bool is_command, u8 *data, u8 len);
void controlParamNegotiation(rfChannel *channel, bool is_command, u8 *data, u8 len);
void controlRemotePortNegotiation(rfChannel *channel, bool is_command, u8 *data, u8 len);

void sendSignals(rfChannel *channel);






//--------------------------------

int rfcomm_init(l2cap_context_t *l2cap_ctx);
void rf_register_client(l2cap_context_t *l2cap_ctx, rfCallback pClient);

#endif // RFCOMM_H
