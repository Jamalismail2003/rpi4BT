#ifndef _rpi4bt_msg_h_
#define _rpi4bt_msg_h_

typedef struct {
    uint32_t cmd;
    char mac_addr[18];  
    uint16_t sdp_request;
    uint32_t rf_channel_num;
} custom_msg_t;

#define RPI4_CUSTOM_COMMAND __DIOTF(_DCMD_MISC, 10000, custom_msg_t)

#define CMD_SCAN          1  // Start scanning
#define CMD_PAIR          2  // Pair with a device
#define CMD_STATUS 		  3  // Get stack status
#define CMD_LIST		  4  // List devices
#define CMD_SDP           5  // Get SDP from Remote device
#define CMD_CARPLAY       6  // Open RFcomm for given Mac Addr & channel number

#define UUID_PROTO_SDP	                    0x0001
#define UUID_PROTO_RFCOMM	                0x0003
#define UUID_PROTO_L2CAP	                0x0100
#define UUID_SERVICE_SERIAL_PORT		    0x1101


#endif