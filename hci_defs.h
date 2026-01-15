#ifndef __HCI_DEFS_H
#define __HCI_DEFS_H

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint8_t u8;  // Defines u8 as an alias for uint8_t
typedef uint16_t u16; // Similarly for u16
typedef uint32_t u32; // And u32

// Add other type definitions as needed

#endif // TYPES_H


#define PACKED		__attribute__ ((packed))


#define  MAX_SIZE_BYTE              255

// an HCI event packet header merely consists of an event
// type byte, and the length byte, the maximum size of a
// HCI event packet is 257.

#define HCI_EVENT_HEADER_SIZE       2
#define HCI_EVENT_MAX_PACKET_SIZE   (HCI_EVENT_HEADER_SIZE + MAX_SIZE_BYTE)     // 257

// an HCI command packet has a 16 bit (word) opcode
// and a length byte, so is one byte larger than
// an event packet

#define HCI_COMMAND_HEADER_SIZE     3
#define HCI_COMMAND_MAX_PACKET_SIZE (HCI_COMMAND_HEADER_SIZE + MAX_SIZE_BYTE)   // 258

// an HCL (ACL) data packet has a 16 bit handle (with flags)
// and a 16 bit length word.   
//
// The maximum ACL packet size is determined by the
// underlying hardware and can be queried with the the
// hci informational command HCI_OP_INFORM_READ_BUFFER_SIZE.
// However, for my initial implementation, until I can at
// least get one byte over RFCOMM, I am letting the transport
// layer own the (fixed) memory, and using a constant. Thus this
// constant currently determines a fixed maximum packet
// size used anywhere within my system and is a kludge.

#define HCI_DATA_HEADER_SIZE     4
#define HCI_MAX_PACKET_SIZE      (HCI_DATA_HEADER_SIZE + MAX_SIZE_BYTE)         // 259



// In my system the "baseband" and "informational" opcodes are handled
// by my "base" sublayer, and we only enumerate the specific opcodes that
// we use in the minimal implementation.

#define HCI_OP_TYPE_BASEBAND                    (3 << 10)
#define HCI_OP_BASEBAND_SET_EVENT_MASK          (HCI_OP_TYPE_BASEBAND | 0x001)
#define HCI_OP_BASEBAND_RESET                   (HCI_OP_TYPE_BASEBAND | 0x003)
#define HCI_OP_BASEBAND_WRITE_LOCAL_NAME        (HCI_OP_TYPE_BASEBAND | 0x013)
#define HCI_OP_BASEBAND_WRITE_SCAN_ENABLE       (HCI_OP_TYPE_BASEBAND | 0x01A)
#define HCI_OP_BASEBAND_WRITE_CLASS_OF_DEVICE   (HCI_OP_TYPE_BASEBAND | 0x024)
#define HCI_OP_WRITE_EXTENDED_INQUIRY_RESPONSE  (HCI_OP_TYPE_BASEBAND | 0x052)

#define HCI_OP_READ_SIMPLE_PAIRING_MODE         (HCI_OP_TYPE_BASEBAND | 0x055)
#define HCI_OP_WRITE_SIMPLE_PAIRING_MODE        (HCI_OP_TYPE_BASEBAND | 0x056)

#define HCI_OP_TYPE_INFORM                      (4 << 10)
#define HCI_OP_INFORM_READ_BD_ADDR                  (HCI_OP_TYPE_INFORM | 0x009)
#define HCI_OP_INFORM_READ_BUFFER_SIZE              (HCI_OP_TYPE_INFORM | 0x005)
#define HCI_OP_INFORM_READ_LOCAL_SUPPORTED_COMMANDS (HCI_OP_TYPE_INFORM | 0x002)
#define HCI_OP_INFORM_READ_LOCAL_SUPPORTED_FEATURES (HCI_OP_TYPE_INFORM | 0x003)

    
#define HCI_OP_TYPE_VENDOR                      (0x3F << 10)

#define BT_STATUS_SUCCESS   0

#define HCI_EVENT_TYPE_COMMAND_COMPLETE                     0x0E            
#define HCI_EVENT_TYPE_COMMAND_STATUS                       0x0F            

#define HCI_OP_TYPE_VENDOR                      (0x3F << 10)
#define HCI_OP_VENDOR_DOWNLOAD_MINIDRIVER   (HCI_OP_TYPE_VENDOR | 0x02E)
#define HCI_OP_VENDOR_WRITE_RAM             (HCI_OP_TYPE_VENDOR | 0x04C)
#define HCI_OP_VENDOR_LAUNCH_RAM            (HCI_OP_TYPE_VENDOR | 0x04E)





 #define HCI_PREFIX_CMD      0x01
 #define HCI_PREFIX_DATA     0x02
 #define HCI_PREFIX_SDATA    0x03
 #define HCI_PREFIX_EVENT    0x04


#define PIN_CODE  "0000"

#define BT_ADDR_SIZE            6
#define BT_CLASS_SIZE           3
#define BT_PIN_CODE_SIZE        16
#define BT_LINK_KEY_SIZE        16
#define BT_NAME_SIZE            248



//------------------------------------------------------
// Constant (parameters) for certain HCI_LINK commands
//------------------------------------------------------

#define HCI_LINK_INQUIRY_LAP_GIAC                   0x9E8B33    // General Inquiry Access Code
#define HCI_LINK_INQUIRY_LENGTH_MIN                 0x01        // 1.28s
#define HCI_LINK_INQUIRY_LENGTH_MAX                 0x30        // 61.44s
#define HCI_LINK_INQUIRY_LENGTH(secs)               (((secs) * 100 + 64) / 128)
#define HCI_LINK_INQUIRY_NUM_RESPONSES_UNLIMITED    0x00

#define HCI_LINK_PAGE_SCAN_REPETITION_R0        0x00
#define HCI_LINK_PAGE_SCAN_REPETITION_R1        0x01
#define HCI_LINK_PAGE_SCAN_REPETITION_R2        0x02
#define HCI_LINK_CLOCK_OFFSET_INVALID           0       // bit 15 is not set

#define HCI_LINK_SCAN_ENABLE_NONE               0x00
#define HCI_LINK_SCAN_ENABLE_INQUIRY_ENABLED    0x01
#define HCI_LINK_SCAN_ENABLE_PAGE_ENABLED       0x02
#define HCI_LINK_SCAN_ENABLE_BOTH_ENABLED       0x03



#define LCAP_STARTING_TXN_ID   17
#define LCAP_STARTING_CID      0x70


//-------------------------
// HCI Link Commands
//-------------------------
// The first ones we enumerate are the "link" commands that have
// the "OGF LINK CONTROL" bit set.  We enumerate all the possible
// link commands, although we only implement some of them, as
// necessary in my minimal implementation.


#define HCI_OP_TYPE_LINK                    (1 << 10)                          // OGF_LINK_CONTROL
#define HCI_OP_LINK_INQUIRY                 (HCI_OP_TYPE_LINK | 0x0001)        // OP_CODE_INQUIRY
#define HCI_OP_LINK_CREATE_CONNECTION       (HCI_OP_TYPE_LINK | 0x0005)

#define HCI_OP_LINK_INQUIRY                                 (HCI_OP_TYPE_LINK | 0x0001)
    // Command used to enter Inquiry mode where
    // it discovers other Bluetooth devices.
#define HCI_OP_LINK_INQUIRY_CANCEL                          (HCI_OP_TYPE_LINK | 0x0002)
    // Command to cancel the Inquiry mode
    // in which the Bluetooth device is in.
#define HCI_OP_LINK_PERIODIC_INQUIRY_MODE                   (HCI_OP_TYPE_LINK | 0x0003)
    // Command to set the device to enter Inquiry modes
    // periodically according to the time interval set.
#define HCI_OP_LINK_EXIT_PERIODIC_INQUIRY_MODE              (HCI_OP_TYPE_LINK | 0x0004)
    // Command to exit the periodic Inquiry mode
#define HCI_OP_LINK_CREATE_CONNECTION                       (HCI_OP_TYPE_LINK | 0x0005)
    // Command to create an ACL connection to
    // the device specified by the BD_ADDR in
    // the parameters.
#define HCI_OP_LINK_DISCONNECT                              (HCI_OP_TYPE_LINK | 0x0006)
    // Command to terminate the existing
    // connection to a device
#define HCI_OP_LINK_ADD_SCO_CONNECTION                      (HCI_OP_TYPE_LINK | 0x0007)
    // Create an SCO connection defined by the
    // connection handle parameters.
#define HCI_OP_LINK_ACCEPT_CONNECTION_REQUEST               (HCI_OP_TYPE_LINK | 0x0009)
    // Command to accept a new connection
    // request
#define HCI_OP_LINK_REJECT_CONNECTION_REQUEST               (HCI_OP_TYPE_LINK | 0x000A)
    // Command to reject a new connection
    // request
#define HCI_OP_LINK_LINK_KEY_REQUEST_REPLY                  (HCI_OP_TYPE_LINK | 0x000B)
    // Reply command to a link key request event
    // sent from controller to the host
#define HCI_OP_LINK_LINK_KEY_REQUEST_NEGATIVE_REPLY         (HCI_OP_TYPE_LINK | 0x000C)
    // Reply command to a link key request event
    // from the controller to the host if there is no link
    // key associated with the connection.
#define HCI_OP_LINK_PIN_CODE_REQUEST_REPLY                  (HCI_OP_TYPE_LINK | 0x000D)
    // Reply command to a PIN code request event
    // sent from a controller to the host.
#define HCI_OP_LINK_PIN_CODE_REQUEST_NEGATIVE_REPLY         (HCI_OP_TYPE_LINK | 0x000E)
    // Reply command to a PIN code request event
    // sent from the controller to the host if there
    // is no PIN associated with the connection.
#define HCI_OP_LINK_CHANGE_CONNECTION_PACKET_TYPE           (HCI_OP_TYPE_LINK | 0x000F)
    // Command to change the type of packets to
    // be sent for an existing connection.
#define HCI_OP_LINK_AUTHENTICATION_REQUESTED                (HCI_OP_TYPE_LINK | 0x0011)
    // Command to establish authentication
    // between two devices specified by the
    // connection handle.
#define HCI_OP_LINK_SET_CONNECTION_ENCRYPTION               (HCI_OP_TYPE_LINK | 0x0013)
    // Command to enable or disable the link
    // level encryption.
#define HCI_OP_LINK_CHANGE_CONNECTION_LINK_KEY              (HCI_OP_TYPE_LINK | 0x0015)
    // Command to force the change of a link key
    // to a new one between two connected devices.
#define HCI_OP_LINK_MASTER_LINK_KEY                         (HCI_OP_TYPE_LINK | 0x0017)
    // Command to force two devices to use the
    // master’s link key temporarily.
#define HCI_OP_LINK_REMOTE_NAME_REQUEST                     (HCI_OP_TYPE_LINK | 0x019)
    // Command to determine the user friendly
    // name of the connected device. 
#define HCI_OP_LINK_READ_REMOTE_SUPPORTED_FEATURES          (HCI_OP_TYPE_LINK | 0x001B)
    // Command to determine the features
    // supported by the connected device.
#define HCI_OP_LINK_READ_REMOTE_EXTENDED_FEATURES          (HCI_OP_TYPE_LINK | 0x001C)
    // Command to determine the extended features
    // supported by the connected device.
#define HCI_OP_LINK_READ_REMOTE_VERSION_INFORMATION         (HCI_OP_TYPE_LINK | 0x001D)
    // Command to determine the version
    // information of the connected device.
#define HCI_OP_LINK_CLOCK_OFFSET                            (HCI_OP_TYPE_LINK | 0x001F)
    // Command to read the clock offset of the
    // remote device.
#define HCI_OP_LINK_IO_CAPABILITY_REPLY                  (HCI_OP_TYPE_LINK | 0x002B)
    // Command to resplond IO Capability
#define HCI_OP_LINK_USER_CONFIRMATION_REQUEST_REPLY      (HCI_OP_TYPE_LINK | 0x002C)
    // Command to confirm PassKey



//---------------------------------------------------------------------------
// HCI EVENT PACKETS
//---------------------------------------------------------------------------
// HCI Event packets have a one byte event code (after the HCI prefix byte)
// followed by a one byte length, followed by upto 255 bytes of content

#define HCI_EVENT_TYPE_INQUIRY_COMPLETE                     0x01    
    // Indicates the Inquiry has finished.
#define HCI_EVENT_TYPE_INQUIRY_RESULT                       0x02
    // Indicates that Bluetooth device(s)
    // have responded for the inquiry.
#define HCI_EVENT_TYPE_CONNECTION_COMPLETE                  0x03
    // Indicates to both hosts that the
    // new connection has been formed.
#define HCI_EVENT_TYPE_CONNECTION_REQUEST                   0x04
    // Indicates that a new connection is
    // trying to be established.
#define HCI_EVENT_TYPE_DISCONNECTION_COMPLETE               0x05
    // Occurs when a connection has been
    // disconnected.
#define HCI_EVENT_TYPE_AUTHENTICATION_COMPLETE              0x06
    // Occurs when an authentication
    // has been completed.
#define HCI_EVENT_TYPE_REMOTE_NAME_REQUEST_COMPLETE         0x07
    // Indicates that the request for the
    // remote name has been completed.
#define HCI_EVENT_TYPE_ENCRYPTION_CHANGE                    0x08
    // Indicates that a change in the
    // encryption has been completed.
#define HCI_EVENT_TYPE_CHANGE_LINK_KEY_COMPLETE             0x09
    // Indicates that the change in the
    // link key has been completed.
#define HCI_EVENT_TYPE_MASTER_LINK_KEY_COMPLETE             0x0A
    // Indicates that the change in the temporary
    // link key or semi permanent link key on the
    // master device is complete.
#define HCI_EVENT_TYPE_READ_SUPPORTED_FEATURES_COMPLETE     0x0B
    // Indicates that the reading of the supported
    // features on the remote device is complete.
#define HCI_EVENT_TYPE_READ_REMOTE_VERSION_COMPLETE         0x0C
    // Indicates that the version number on the
    // remote device has been read and completed.
#define HCI_EVENT_TYPE_Q0S_SETUP_COMPLETE                   0x0D
    // Indicates that the Quality of Service
    // setup has been complete.
#define HCI_EVENT_TYPE_COMMAND_COMPLETE                     0x0E            
    // Used by controller to send status and event
    // parameters to the host for the particular
    // command.
#define HCI_EVENT_TYPE_COMMAND_STATUS                       0x0F            
    // Indicates that the command has been received 
    // is being processed in the host controller.
#define HCI_EVENT_TYPE_HARDWARE_ERROR                       0x10
    // Indicates a hardware failure of the
    // Bluetooth device.
#define HCI_EVENT_TYPE_FLUSH_OCCURED                        0x11
    // Indicates that the data has been flushed
    // for a particular connection.
#define HCI_EVENT_TYPE_ROLE_CHANGE                          0x12
    // Indicates that the current bluetooth role
    // for a connection has been changed.
#define HCI_EVENT_TYPE_NUMBER_OF_COMPLETED_PACKETS          0x13
    // Indicates to the host the number of data
    // packets sent compared to the last time the
    // same event was sent.
#define HCI_EVENT_TYPE_MODE_CHANGE                          0x14
    // Indicates the change in mode from hold,
    // sniff, park or active to another mode.
#define HCI_EVENT_TYPE_RETURN_LINK_KEYS                     0x15
    // Used to return stored link keys after
    // a Read_Stored_Link_Key command was issued.
#define HCI_EVENT_TYPE_PIN_CODE_REQUEST                     0x16
    // Indicates the a PIN code is required
    // for a new connection.
#define HCI_EVENT_TYPE_LINK_KEY_REQUEST                     0x17
    // Indicates that a link key is
    // required for the connection.
#define HCI_EVENT_TYPE_LINK_KEY_NOTIFICATION                0x18
    // Indicates to the host that a new
    // link key has been created.
#define HCI_EVENT_TYPE_LOOPBACK_COMMAND                     0x19
    // Indicates that command sent from
    // the host will be looped back.
#define HCI_EVENT_TYPE_DATA_BUFFER_OVERFLOW                 0x1A
    // Indicates that the data buffers on
    // the host has overflowed.
#define HCI_EVENT_TYPE_MAX_SLOTS_CHANGE                     0x1B
    // Informs the host when the LMP_Max_Slots
    // parameter changes.
#define HCI_EVENT_TYPE_READ_CLOCK_OFFSET_COMPLETE           0x1C
    // Indicates the completion of reading
    // the clock offset information.
#define HCI_EVENT_TYPE_CONNECTION_PACKET_TYPE_CHANGED       0x1D
    // Indicate the completion of the packet
    // type change for a connection.
#define HCI_EVENT_TYPE_QOS_VIOLATION                        0x1E
    // Indicates that the link manager is unable
    // to provide the required Quality of Service.
#define HCI_EVENT_TYPE_PAGE_SCAN_MODE_CHANGE                0x1F
    // Indicates that the remote device has
    // successfully changed the Page Scan mode.
#define HCI_EVENT_TYPE_PAGE_SCAN_REPETITION_MODE_CHANGE     0x20

// I created the above defines, and copied the above descriptions
// from a list of HCI event codes I found on the net.  Only later,
// after much work and digging through the specs, I realized that
// the above list is incomplete vis-a-vis the bt core spec

#define HCI_EVENT_FLOW_SPECIFICATION_COMP_EVT     0x21
#define HCI_EVENT_INQUIRY_RSSI_RESULT_EVT         0x22
#define HCI_EVENT_READ_RMT_EXT_FEATURES_COMP_EVT  0x23
#define HCI_EVENT_ESCO_CONNECTION_COMP_EVT        0x2C
#define HCI_EVENT_ESCO_CONNECTION_CHANGED_EVT     0x2D
#define HCI_EVENT_SNIFF_SUB_RATE_EVT              0x2E
#define HCI_EVENT_EXTENDED_INQUIRY_RESULT_EVT     0x2F
#define HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMP_EVT 0x30
#define HCI_EVENT_IO_CAPABILITY_REQUEST_EVT       0x31
#define HCI_EVENT_IO_CAPABILITY_RESPONSE_EVT      0x32
#define HCI_EVENT_USER_CONFIRMATION_REQUEST_EVT   0x33
#define HCI_EVENT_USER_PASSKEY_REQUEST_EVT        0x34
#define HCI_EVENT_REMOTE_OOB_DATA_REQUEST_EVT     0x35
#define HCI_EVENT_SIMPLE_PAIRING_COMPLETE_EVT     0x36
#define HCI_EVENT_LINK_SUPER_TIMEOUT_CHANGED_EVT  0x38
#define HCI_EVENT_ENHANCED_FLUSH_COMPLETE_EVT     0x39
#define HCI_EVENT_USER_PASSKEY_NOTIFY_EVT         0x3B
#define HCI_EVENT_KEYPRESS_NOTIFY_EVT             0x3C
#define HCI_EVENT_RMT_HOST_SUP_FEAT_NOTIFY_EVT    0x3D




#define HCI_HANDLE_CONNECTING        0x1000
#define HCI_HANDLE_ERROR             0x8000

// hci client events

#define HCC_EVENT_CONNECTED          0x0001
#define HCC_EVENT_DISCONNECTED       0x0002
#define HCC_EVENT_CONNECTION_ERROR   0x8000
#define HCC_INQUIRY_COMPLETE         0x0100
#define HCC_INQUIRY_DEVICE_FOUND     0x0200
#define HCC_INQUIRY_NAME_FOUND       0x0400


//#define HCI_DEVICE_INCLUDE_UNUSED_FIELDS   0

//---------------------------------------------------
// hci structure Definitions
//---------------------------------------------------

#define INQUIRY_RESP_SIZE                       14
#define INQUIRY_RESP_BD_ADDR(p, i)              (&(p)->data[(i)*BT_ADDR_SIZE])
#define INQUIRY_RESP_PAGE_SCAN_REP_MODE(p, i)   ((p)->data[(p)->num_responses*BT_ADDR_SIZE + (i)])
#define INQUIRY_RESP_CLASS_OF_DEVICE(p, i)      (&(p)->data[(p)->num_responses*(BT_ADDR_SIZE+1+2) + (i)*BT_CLASS_SIZE])

//--------------------
// event strutures
//--------------------

struct hci_event_header
{
    u8	event_code;
    u8	length;
}   PACKED;
typedef struct hci_event_header hci_event_header; // Forward declaration

struct hci_command_status_event
{
    hci_event_header	header;
    u8	status;
    u8	num_command_packets;
    u16	command_opcode;
}   PACKED;
typedef struct hci_command_status_event hci_command_status_event; // Forward declaration

////
struct hci_io_capability_request_event
{
    hci_event_header    header;
    u8  addr[BT_ADDR_SIZE];
} PACKED;
typedef struct hci_io_capability_request_event hci_io_capability_request_event;

struct hci_io_capability_response_event {
    hci_event_header    header;
    uint8_t addr[6]; // BD_ADDR of the remote device
    uint8_t io_capability; // Set this to 0x04 for KeyboardDisplay
    uint8_t oob_data_present; // Set this to 0x00 (No OOB data)
    uint8_t auth_req; // Authentication requirements (e.g., MITM protection)
} PACKED;
typedef struct hci_io_capability_response_event hci_io_capability_response_event;

struct hci_io_user_confirmation_request_event {
    hci_event_header    header;
    uint8_t addr[6]; // BD_ADDR of the remote device
    uint32_t pass_key;
} PACKED;
typedef struct hci_io_user_confirmation_request_event hci_io_user_confirmation_request_event;

struct hci_connection_request_event
{
    hci_event_header    header;
    u8  addr[BT_ADDR_SIZE];
    u16 packet_type;
    u8  page_rep_mode;
    u8  page_mode;
    u16 clock_offset;
    u16 switch_flag;
}   PACKED;
typedef struct hci_connection_request_event hci_connection_request_event;

struct hci_connection_complete_event
{
    hci_event_header    header;
    u8  status;
    u16 handle;
    u8  addr[BT_ADDR_SIZE];
    u8 link_type;
    u8 encrypt;
}   PACKED; 
typedef struct hci_connection_complete_event hci_connection_complete_event;

struct hci_disconnect_complete_event
{
    hci_event_header    header;
    u8  status;
    u16 handle;
    u8  reason;
}   PACKED;
typedef struct hci_disconnect_complete_event hci_disconnect_complete_event;

struct hci_pin_code_request_event
{
    hci_event_header    header;
    u8  addr[BT_ADDR_SIZE];
}   PACKED;
typedef struct hci_pin_code_request_event hci_pin_code_request_event;

struct hci_link_key_notification_event
{
    hci_event_header    header;
    u8  addr[BT_ADDR_SIZE];
    u8  link_key[BT_LINK_KEY_SIZE];
    u8  key_type;
}   PACKED;
typedef struct hci_link_key_notification_event hci_link_key_notification_event;

struct hci_authentication_complete_event
{
    hci_event_header    header;
    u8 status;
    u16 handle;
}   PACKED;
typedef struct hci_authentication_complete_event hci_authentication_complete_event;
      
struct hci_read_local_supported_features_event {
    hci_event_header header;   // Event header
    uint8_t status;            // Status of the operation (0x00 for success)
    uint8_t features[8];       // Local Supported Features (64 bits)
} PACKED;
typedef struct hci_read_local_supported_features_event hci_read_local_supported_features_event;

struct hci_read_remote_supported_features_event {
    hci_event_header    header;
    u8 status;     // Status of the operation (0x00 indicates success)
    u16 handle;    // Connection handle identifying the link
    u8 features[8]; // Array of 8 bytes indicating the supported features of the remote device
} PACKED;
typedef struct hci_read_remote_supported_features_event hci_read_remote_supported_features_event;

struct hci_read_remote_extended_features_event {
    hci_event_header    header;
    u8 status;      // Status of the operation (0x00 indicates success)
    u16 handle;     // Connection handle identifying the link
    u8 page_number; // Page number of the extended features set
    u8 max_page_number; // Maximum page number supported by the remote device
    u8 features[8]; // Array of 8 bytes representing the extended features of the remote device
} PACKED;
typedef struct hci_read_remote_extended_features_event hci_read_remote_extended_features_event;

struct hci_link_timeout_changed_event
{
    hci_event_header    header;
    u16 handle;
    u16 timeout;
}   PACKED;
typedef struct hci_link_timeout_changed_event hci_link_timeout_changed_event;
////

struct hci_command_complete_event
{
    hci_event_header	header;
    u8	num_command_packets;
    u16	command_opcode;
    u8	status;				// normally part of param[]
    u8	param[0];
}   PACKED;
typedef struct hci_command_complete_event hci_command_complete_event;


struct hci_read_bdaddr_complete_event
{
    hci_command_complete_event  header;
    u8  addr[BT_ADDR_SIZE];
}   PACKED;
typedef struct hci_read_bdaddr_complete_event hci_read_bdaddr_complete_event;

struct hci_data_header
{
    u16 handle;
    u16 len;
}   PACKED;
typedef struct hci_data_header hci_data_header;

struct hci_command_header
{
    u16 opcode;
    u8  length;
}   PACKED;
typedef struct hci_command_header       hci_command_header;

struct hci_io_capability_request_reply_cmd {
    hci_command_header  header;
    uint8_t addr[6]; // BD_ADDR of the remote device
    uint8_t io_capability; // Set this to 0x04 for KeyboardDisplay
    uint8_t oob_data_present; // Set this to 0x00 (No OOB data)
    uint8_t auth_req; // Authentication requirements (e.g., MITM protection)
} PACKED;
typedef struct hci_io_capability_request_reply_cmd hci_io_capability_request_reply_cmd;

struct hci_io_user_confirmation_request_reply_cmd {
    hci_command_header  header;
    uint8_t addr[6]; // BD_ADDR of the remote device
} PACKED;
typedef struct hci_io_user_confirmation_request_reply_cmd hci_io_user_confirmation_request_reply_cmd;

struct hci_write_simple_pairing_mode 
{
    hci_command_header  header;
    u8 simple_pairing_mode; // 0x01 to enable SSP, 0x00 to disable
}  PACKED;
typedef struct hci_write_simple_pairing_mode hci_write_simple_pairing_mode;

struct hci_create_connection_command
{
    hci_command_header  header;
    u8  addr[BT_ADDR_SIZE];
    u16 packet_type;
    u8  page_rep_mode;
    u8  page_mode;
    u16 clock_offset;
    u8  switch_roles;
}   PACKED;
typedef struct hci_create_connection_command hci_create_connection_command;


struct hci_read_remote_extended_features_command 
{
    hci_command_header  header;
    u16 handle; 
    u8 page_number; // Page number to retrieve the extended features (page 0 is the default set of features)
} PACKED;
typedef struct hci_read_remote_extended_features_command hci_read_remote_extended_features_command;

struct hci_read_remote_supported_features_command 
{
    hci_command_header  header;
    u16 handle; // Connection handle (specifies which connection the authentication is for)
} PACKED;
typedef struct hci_read_remote_supported_features_command hci_read_remote_supported_features_command;

struct hci_authentication_request_command 
{
    hci_command_header  header;
    u16 handle; // Connection handle (specifies which connection the authentication is for)
} PACKED;
typedef struct hci_authentication_request_command hci_authentication_request_command;

struct hci_pin_code_reply_command
{
    hci_command_header  header;
    u8  addr[BT_ADDR_SIZE];
    u8  pin_code_len;
    char pin_code[BT_PIN_CODE_SIZE];
} PACKED;
typedef struct hci_pin_code_reply_command hci_pin_code_reply_command;

struct hci_accept_connection_command
{
    hci_command_header  header;
    u8  addr[BT_ADDR_SIZE];
    u8  role;
}   PACKED;
typedef struct hci_accept_connection_command hci_accept_connection_command;

struct hci_disconnection_request
{
    hci_command_header  header;
    u16 disconnect_handle;
    u8 reason;
}   PACKED;
typedef struct hci_disconnection_request hci_disconnection_request;


struct hci_vendor_command
{
    hci_command_header hdr;
    u8  data[255];
}   PACKED;
typedef struct hci_vendor_command       hci_vendor_command;

struct hci_write_class_of_device_command
{
    hci_command_header  header;
    u8  class_of_device[BT_CLASS_SIZE];
}   PACKED;
typedef struct hci_write_class_of_device_command hci_write_class_of_device_command;



struct hci_write_local_name_command
{
    hci_command_header  header;
    u8  local_name[BT_NAME_SIZE];
}   PACKED;
typedef struct hci_write_local_name_command hci_write_local_name_command;


struct hci_write_scan_enable_command
{
    hci_command_header  header;
    u8  scan_enable;
}   PACKED;
typedef struct hci_write_scan_enable_command hci_write_scan_enable_command;

struct hci_link_key_request_reply_command
{
    hci_command_header  header;
    u8  addr[BT_ADDR_SIZE];
    u8  link_key[BT_LINK_KEY_SIZE];
}   PACKED;
typedef struct hci_link_key_request_reply_command hci_link_key_request_reply_command;

struct hci_link_key_request_reject_command
{
    hci_command_header  header;
    u8  addr[BT_ADDR_SIZE];
}   PACKED;
typedef struct hci_link_key_request_reject_command hci_link_key_request_reject_command;

struct hci_read_simple_pairing_mode_rsp {
    hci_event_header    header;
    u8 status;
    u8 simple_pairing_mode; // 0x01 if enabled
} PACKED;
typedef struct hci_read_simple_pairing_mode_rsp hci_read_simple_pairing_mode_rsp;


struct hci_page_scan_rep_mode_change_event
{
    hci_event_header    header;
    u8  addr[BT_ADDR_SIZE];
    u8  page_rep_mode;
}   PACKED;
typedef struct hci_page_scan_rep_mode_change_event hci_page_scan_rep_mode_change_event;

struct hci_max_slots_change_event
{
    hci_event_header    header;
    u16 handle;
    u8  max_slots;
}   PACKED;
typedef struct hci_max_slots_change_event hci_max_slots_change_event;

struct hci_number_of_completed_packets_event
{
    hci_event_header    header;
    u8 num_entries;
        // followed by num_entries handles
        // and num_entries 16 bit counts
    u16 data[0];        
}   PACKED;
typedef struct hci_number_of_completed_packets_event hci_number_of_completed_packets_event;

struct hci_inquiry_command
{
    hci_command_header  header;
    u8  lap[BT_CLASS_SIZE];
    u8  inquiry_length;
    u8  num_responses;
}   PACKED;
typedef struct hci_inquiry_command hci_inquiry_command;


struct hci_inquiry_result_event
{
    hci_event_header    header;
    u8  num_responses;
    u8  data[0];
}   PACKED;
typedef struct hci_inquiry_result_event hci_inquiry_result_event;

/*
struct hciRemoteDevice
{
    struct hciRemoteDevice *prev;
    struct hciRemoteDevice *next;
    
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
typedef struct hciRemoteDevice hciRemoteDevice;
*/

struct hci_remote_name_request_command
{
    hci_command_header  header;
    u8  addr[BT_ADDR_SIZE];
    u8  page_scan_repetition_mode;
    u8  reserved;   // set to 0
    u16 clock_offset;
}   PACKED;
typedef struct hci_remote_name_request_command hci_remote_name_request_command;









struct hci_inquiry_complete_event
{
    hci_event_header    header;
    u8  status;
}   PACKED;
typedef struct hci_inquiry_complete_event hci_inquiry_complete_event;

struct hci_remote_name_complete_event
{
    hci_event_header    header;
    u8  status;
    u8  addr[BT_ADDR_SIZE];
    u8  remote_name[BT_NAME_SIZE];
}   PACKED;
typedef struct hci_remote_name_complete_event hci_remote_name_complete_event;


#endif