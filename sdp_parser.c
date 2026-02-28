#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdarg.h>

#include "utils.h"
#include "sdp_defs.h"
#include "sdp.h"

// Set to 1 to show DE type parsing info, 0 to hide
#define SHOW_DE_TYPE   0

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

u16 be16(u16 v)     { return ((v >> 8) & 0xff) | ((v << 8) & 0xff00); }

bool isPrint(u8 c)  { return (c >= 32 && c < 127); }

// A helper function to append formatted output to the request's buffer
static void req_appendf(sdpRequest *req, const char *fmt, ...)
{
    if (!req->output_buf || req->output_len >= req->output_size)
        return; // No space or no buffer

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(req->output_buf + req->output_len,
                      req->output_size - req->output_len, fmt, ap);
    va_end(ap);

    if (n > 0) {
        req->output_len += n;
        if (req->output_len >= req->output_size) {
            // Truncate at the end of the buffer
            req->output_len = req->output_size - 1;
        }
    }
}

void display_bytes_to_buffer(sdpRequest *req, const char *label, const void *data, int len)
{
    // Similar to display_bytes, but append to req->output_buf
    req_appendf(req, "%s: ", label);
    const unsigned char *bytes = (const unsigned char *)data;
    for (int i = 0; i < len; i++) {
        req_appendf(req, "%02X ", bytes[i]);
    }
    req_appendf(req, "\n");
}

// Display raw bytes for debugging if needed
void printRaw(int level, sdpRequest *req)
{
    req_appendf(req, "frame(%d/%d) len=%d\n", req->parse_frame_num, req->num_frames, req->parse_frame_len);
    if (req->parse_ptr && req->parse_frame_len > 0) {
        display_bytes_to_buffer(req, "printRaw", req->parse_ptr, req->parse_frame_len);
    }
}

void printIndent(int level, sdpRequest *req)
{
    while (level > 0)
    {
        req_appendf(req, "    ");
        level--;
    }
}

// Increment parser pointer and handle frame boundaries
void inc_parse(sdpRequest *req, int amt) {
    if (amt > req->parse_frame_len) {
        req_appendf(req, "Error: Attempting to consume more bytes (%d) than available (%d)\n", amt, req->parse_frame_len);
        return;
    }

    req->parse_frame_len -= amt;
    req->parse_ptr += amt;
    req->bytes_parsed += amt;

    if (req->parse_frame_len == 0) {
        req->parse_frame_num++;
        if (req->parse_frame_num < req->num_frames) {
            req->parse_ptr = req->frame[req->parse_frame_num].data;
            req->parse_frame_len = req->frame[req->parse_frame_num].len;
        } else {
            // No more frames
            req->parse_ptr = NULL;
            req->parse_frame_len = 0;
        }
    }
}

u8 get_u8(sdpRequest *req)
{
    assert(req->parse_ptr);
    u8 val = *req->parse_ptr;
    inc_parse(req,1);
    return val;
}

u16 get_u16(sdpRequest *req)
{
    u16 val = (get_u8(req) << 8) + (get_u8(req) << 0);
    return val;
}

u32 get_u32(sdpRequest *req)
{
    u32 val =
        (get_u8(req) << 24) +
        (get_u8(req) << 16) +
        (get_u8(req) <<  8) +
        (get_u8(req) <<  0);
    return val;
}

uint64_t get_uint64_t(sdpRequest *req)
{
    uint64_t hi = get_u32(req);
    uint64_t lo = get_u32(req);
    uint64_t val = (hi << 32) + lo;
    return val;
}

void get_u128(sdpRequest *req, uint64_t *l, uint64_t *h)
{
    *h = get_uint64_t(req);
    *l = get_uint64_t(req);
}

#define DE_NULL   0
#define DE_UINT   1
#define DE_INT    2
#define DE_UUID   3
#define DE_STRING 4
#define DE_BOOL   5
#define DE_SEQ    6
#define DE_ALT    7
#define DE_URL    8

typedef struct 
{
    int addl_bits;
    int num_bytes;
} de_size_table_t;

static de_size_table_t de_size_table[] =
{
    { 0, 1  },  
    { 0, 2  },  
    { 0, 4  },  
    { 0, 8  },  
    { 0, 16 },  
    { 1, 1  },  
    { 1, 2  },  
    { 1, 4  },  
};

typedef struct
{
    int   uuid;
    const char* name;
} uuid_table_t;

static uuid_table_t uuid_table[] =
{
    { UUID_PROTO_SDP,               "SDP"       },
    { UUID_PROTO_UDP,               "UDP"       },
    { UUID_PROTO_RFCOMM,            "RFCOMM"    },
    { UUID_PROTO_TCP,               "TCP"       },
    { UUID_PROTO_TCS_BIN,           "TCS_BIN"   },
    { UUID_PROTO_TCS_AT,            "TCS_AT"    },
    { UUID_PROTO_ATT,               "ATT"       },
    { UUID_PROTO_OBEX,              "OBEX"      },
    { UUID_PROTO_IP,                "IP"        },
    { UUID_PROTO_FTP,               "FTP"       },
    { UUID_PROTO_HTTP,              "HTTP"      },
    { UUID_PROTO_WSP,               "WSP"       },
    { UUID_PROTO_BNEP,              "BNEP"      },
    { UUID_PROTO_UPNP,              "UPNP"      },
    { UUID_PROTO_HIDP,              "HIDP"      },
    { UUID_PROTO_HCRP_CTRL,         "HCRP_CTRL" },
    { UUID_PROTO_HCRP_DATA,         "HCRP_DATA" },
    { UUID_PROTO_HCRP_NOTE,         "HCRP_NOTE" },
    { UUID_PROTO_AVCTP,             "AVCTP"     },
    { UUID_PROTO_AVDTP,             "AVDTP"     },
    { UUID_PROTO_CMTP,              "CMTP"      },
    { UUID_PROTO_UDI,               "UDI"       },
    { UUID_PROTO_MCAP_CTRL,         "MCAP_CTRL" },
    { UUID_PROTO_MCAP_DATA,         "MCAP_DATA" },
    { UUID_PROTO_L2CAP,             "L2CAP"     },

    { UUID_SERVICE_SDP_SERVER,              "SDPServer"       },
    { UUID_SERVICE_BROWSE_GROUP,            "BrowseGroupDsc"  },
    { UUID_SERVICE_PUBLIC_BROWSE_GROUP,     "BrowseGroup"     },
    { UUID_SERVICE_SERIAL_PORT,             "SP"              },
    { UUID_SERVICE_LAN_ACCESS,              "LAN"             },
    { UUID_SERVICE_DIALUP_NET,              "DUN"             },
    { UUID_SERVICE_IRMC_SYNC,               "IRMCSync"        },
    { UUID_SERVICE_OBEX_OBJ_PUSH,           "OBEXObjPush"     },
    { UUID_SERVICE_OBEX_FILE_TRANSFER,      "OBEXObjTrnsf"    },
    { UUID_SERVICE_IRMC_SYNC_CMD,           "IRMCSyncCmd"     },
    { UUID_SERVICE_HEADSET,                 "Headset"         },
    { UUID_SERVICE_CORDLESS_TELEPHONY,      "CordlessTel"     },
    { UUID_SERVICE_AUDIO_SOURCE,            "AudioSource"     },
    { UUID_SERVICE_AUDIO_SINK,              "AudioSink"       },
    { UUID_SERVICE_AV_REMOTE_TARGET,        "AVRemTarget"     },
    { UUID_SERVICE_ADVANCED_AUDIO,          "AdvAudio"        },
    { UUID_SERVICE_AV_REMOTE,               "AVRemote"        },
    { UUID_SERVICE_VIDEO_CONFERENCING,      "VideoConf"       },
    { UUID_SERVICE_INTERCOM,                "Intercom"        },
    { UUID_SERVICE_FAX,                     "Fax"             },
    { UUID_SERVICE_HEADSET_AUDIO_GATEWAY,   "HeadsetAG"       },
    { UUID_SERVICE_WAP,                     "WAP"             },
    { UUID_SERVICE_WAP_CLIENT,              "WAP Client"      },
    { UUID_SERVICE_PANU,                    "PANU"            },
    { UUID_SERVICE_NAP,                     "NAP"             },
    { UUID_SERVICE_GN,                      "GN"              },
    { UUID_SERVICE_DIRECT_PRINTING,         "DirectPrint"     },
    { UUID_SERVICE_REFERENCE_PRINTING,      "RefPrint"        },
    { UUID_SERVICE_IMAGING,                 "Imaging"         },
    { UUID_SERVICE_IMAGING_RESPONDER,       "ImagingResponder"},
    { UUID_SERVICE_IMAGING_ARCHIVE,         "ImagingArchive"  },
    { UUID_SERVICE_IMAGING_REF_OBJS,        "ImagingRefObjs"  },
    { UUID_SERVICE_HANDSFREE,               "Handsfree"       },
    { UUID_SERVICE_HANDSFREE_AUDIO_GATEWAY, "HandsfreeAG"     },
    { UUID_SERVICE_DIRECT_PRINTING_REF_OBJS,"RefObjsPrint"    },
    { UUID_SERVICE_REFLECTED_UI,            "ReflectedUI"     },
    { UUID_SERVICE_BASIC_PRINTING,          "BasicPrint"      },
    { UUID_SERVICE_PRINTING_STATUS,         "PrintStatus"     },
    { UUID_SERVICE_HID,                     "HID"             },
    { UUID_SERVICE_HARDCOPY_CABLE_REPLACE,  "HCRP"            },
    { UUID_SERVICE_HCR_PRINT,               "HCRPrint"        },
    { UUID_SERVICE_HCR_SCAN,                "HCRScan"         },
    { UUID_SERVICE_COMMON_ISDN_ACCESS,      "CIP"             },
    { UUID_SERVICE_VIDEO_CONFERENCING_GW,   "VideoConf_GW"    },
    { UUID_SERVICE_UDI_MT,                  "UDI_MT"          },
    { UUID_SERVICE_UDI_TA,                  "UDI_TA"          },
    { UUID_SERVICE_AUDIO_VIDEO,             "AudioVideo"      },
    { UUID_SERVICE_SIM_ACCESS,              "SAP"             },
    { UUID_SERVICE_PHONEBOOK_ACCESS_PCE,    "PBAP_PCE"        },
    { UUID_SERVICE_PHONEBOOK_ACCESS_PSE,    "PBAP PSE"        },
    { UUID_SERVICE_PHONEBOOK_ACCESS,        "PBAP"            },
    { UUID_SERVICE_MAP_MSE,                 "MAP_MSE"         },
    { UUID_SERVICE_MAP_MCE,                 "MAP_MCE"         },
    { UUID_SERVICE_MAP,                     "MAP"             },
    { UUID_SERVICE_GNSS,                    "GNSS"            },
    { UUID_SERVICE_GNSS_SERVER,             "GNSS_Server"     },
    { UUID_SERVICE_PNP_INFO,                "PNPInfo"         },
    { UUID_SERVICE_GENERIC_NETWORKING,      "Networking"      },
    { UUID_SERVICE_GENERIC_FILE_TRANSGRT,   "FileTrnsf"       },
    { UUID_SERVICE_GENERIC_AUDIO,           "Audio"           },
    { UUID_SERVICE_GENERIC_TELEPHONY,       "Telephony"       },
    { UUID_SERVICE_UPNP,                    "UPNP"            },
    { UUID_SERVICE_UPNP_IP,                 "UPNP IP"         },
    { UUID_SERVICE_UPNP_PAN,                "UPNP PAN"        },
    { UUID_SERVICE_UPNP_LAP,                "UPNP LAP"        },
    { UUID_SERVICE_UPNP_L2CAP,              "UPNP L2CAP"      },
    { UUID_SERVICE_VIDEO_SOURCE,            "VideoSource"     },
    { UUID_SERVICE_VIDEO_SINK,              "VideoSink"       },
    { UUID_SERVICE_VIDEO_DISTRIBUTION,      "VideoDist"       },
    { UUID_SERVICE_HDP,                     "HDP"             },
    { UUID_SERVICE_HDP_SOURCE,              "HDP_SOURCE"      },
    { UUID_SERVICE_HDP_SINK,                "HDP_SINK"        },
    { UUID_SERVICE_APPLE_AGENT,             "AppleAgent"      },
};

#define UUID_TABLE_SIZE (sizeof(uuid_table)/sizeof(uuid_table_t))

typedef struct
{
    int   attr_id;
    const char* name;
} attr_id_table_t;

static attr_id_table_t attr_id_table[] =
{
    { ATTR_ID_SERVICE_RECORD_HANDLE,             "SrvRecHndl"         },
    { ATTR_ID_SERVICE_CLASS_ID_LIST,             "SrvClassIDList"     },
    { ATTR_ID_SERVICE_RECORD_STATE,              "SrvRecState"        },
    { ATTR_ID_SERVICE_SERVICE_ID,                "SrvID"              },
    { ATTR_ID_PROTOCOL_DESCRIPTOR_LIST,          "ProtocolDescList"   },
    { ATTR_ID_BROWSE_GROUP_LIST,                 "BrwGrpList"         },
    { ATTR_ID_LANGUAGE_BASE_ATTRIBUTE_ID_LIST,   "LangBaseAttrIDList" },
    { ATTR_ID_SERVICE_INFO_TIME_TO_LIVE,         "SrvInfoTimeToLive"  },
    { ATTR_ID_SERVICE_AVAILABILITY,              "SrvAvail"           },
    { ATTR_ID_BLUETOOTH_PROFILE_DESCRIPTOR_LIST, "BTProfileDescList"  },
    { ATTR_ID_DOCUMENTATION_URL,                 "DocURL"             },
    { ATTR_ID_CLIENT_EXECUTABLE_URL,             "ClientExeURL"       },
    { ATTR_ID_ICON_URL,                          "IconURL"            },
    { ATTR_ID_ADDITIONAL_PROTOCOL_DESC_LISTS,    "AdditionalProtocolDescLists" },
    { ATTR_ID_SERVICE_NAME,                      "SrvName"            },
    { ATTR_ID_SERVICE_DESCRIPTION,               "SrvDesc"            },
    { ATTR_ID_PROVIDER_NAME,                     "ProviderName"       },
    { ATTR_ID_VERSION_NUMBER_LIST,               "VersionNumList"     },
    { ATTR_ID_GROUP_ID,                          "GrpID"              },
    { ATTR_ID_SERVICE_DATABASE_STATE,            "SrvDBState"         },
    { ATTR_ID_SERVICE_VERSION,                   "SrvVersion"         },
    { ATTR_ID_SECURITY_DESCRIPTION,              "SecurityDescription"},
    { ATTR_ID_SUPPORTED_DATA_STORES_LIST,        "SuppDataStoresList" },
    { ATTR_ID_SUPPORTED_FORMATS_LIST,            "SuppFormatsList"    },
    { ATTR_ID_NET_ACCESS_TYPE,                   "NetAccessType"      },
    { ATTR_ID_MAX_NET_ACCESS_RATE,               "MaxNetAccessRate"   },
    { ATTR_ID_IPV4_SUBNET,                       "IPv4Subnet"         },
    { ATTR_ID_IPV6_SUBNET,                       "IPv6Subnet"         },
    { ATTR_ID_SUPPORTED_CAPABILITIES,            "SuppCapabilities"   },
    { ATTR_ID_SUPPORTED_FEATURES,                "SuppFeatures"       },
    { ATTR_ID_SUPPORTED_FUNCTIONS,               "SuppFunctions"      },
    { ATTR_ID_TOTAL_IMAGING_DATA_CAPACITY,       "SuppTotalCapacity"  },
    { ATTR_ID_SUPPORTED_REPOSITORIES,            "SuppRepositories"   },
};

#define ATTR_ID_TABLE_SIZE (sizeof(attr_id_table)/sizeof(attr_id_table_t))

const char* getUUIDName(int uuid)
{
    for (unsigned int i = 0; i < UUID_TABLE_SIZE; i++)
    {
        if (uuid_table[i].uuid == uuid)
            return uuid_table[i].name;
    }
    return NULL;
}

const char* getAttrIDName(int attr_id)
{
    for (unsigned int i = 0; i < ATTR_ID_TABLE_SIZE; i++)
    {
        if (attr_id_table[i].attr_id == attr_id)
            return attr_id_table[i].name;
    }
    return "unknown ATTR_ID";
}

u8 parseDEHeader(sdpRequest *req, int *n)
{
    u8 de_hdr = get_u8(req);
    u8 de_type = de_hdr >> 3;
    u8 siz_idx = de_hdr & 0x07;
    
    if (de_size_table[siz_idx].addl_bits)
    {
        switch(de_size_table[siz_idx].num_bytes)
        {
            case 1: *n = get_u8(req); break;
            case 2: *n = get_u16(req); break;
            case 4: *n = get_u32(req); break;
            case 8: *n = (int)get_uint64_t(req); break;
            default:
                *n = 0;
                break;
        }
    }
    else
    {
        *n = de_size_table[siz_idx].num_bytes;
    }
    
#if SHOW_DE_TYPE
    char ts[100];
    snprintf(ts, sizeof(ts), "t(0x%02x=%d,%d,%d)", de_hdr, de_type, siz_idx, *n);
    req_appendf(req, "%-40s\n", ts);
#endif
    
    return de_type;
}

static bool next_uint_is_rfcomm_channel = false;

void printInt(u8 de_type, int level, int num_bytes, sdpRequest *req)
{
    switch(de_type)
    {
        case DE_UINT: req_appendf(req, " uint"); break;
        case DE_INT:  req_appendf(req, " int");  break;
        case DE_BOOL: req_appendf(req, " bool"); break;
        default: break;
    }

    switch(num_bytes)
    {
        case 1:
        {
            u8 val = get_u8(req);
            req_appendf(req, "8  0x%02x", val);
            if (next_uint_is_rfcomm_channel && de_type == DE_UINT)
            {
                req->rfcomm_channel = val;
                next_uint_is_rfcomm_channel = false;
                req_appendf(req, " [RFCOMM channel]");
            }
            break;
        }
        case 2:
        {
            u16 val = get_u16(req);
            req_appendf(req, "16 0x%04x", val);
            break;
        }
        case 4:
        {
            u32 val = get_u32(req);
            req_appendf(req, "32 0x%08x", val);
            break;
        }
        case 8:
        {
            uint64_t val = get_uint64_t(req);
            req_appendf(req, "64 0x%016lx", val);
            break;
        }
        case 16:
        {
            uint64_t val1, val2;
            get_u128(req, &val1, &val2);
            req_appendf(req, "128 0x%016lx%016lx", val2, val1);
            break;
        }
        default:
        {
            req_appendf(req, " err");
            inc_parse(req, num_bytes);
            break;
        }
    }
}

void printUUID(int num_bytes, sdpRequest *req)
{
    u32 uuid = 0;
    const char *s = NULL;
    switch(num_bytes)
    {
        case 2:
            uuid = get_u16(req);
            s = "uuid-16";
            break;
        case 4:
            uuid = get_u32(req);
            s = "uuid-32";
            break;
        case 16:
            req_appendf(req, " uuid-128 ");
            for (int i = 0; i < 16; i++)
            {
                unsigned char c = ((unsigned char *) req->parse_ptr)[i];
                req_appendf(req, "%02x", c);
                if (i == 3 || i == 5 || i == 7 || i == 9)
                    req_appendf(req, "-");
            }
            inc_parse(req,16);
            return;
        default:
            req_appendf(req, " *err*");
            inc_parse(req,num_bytes);
            return;
    }
    
    req_appendf(req, " %s 0x%04x", s, uuid);
    const char *name = getUUIDName(uuid);
    if (name)
        req_appendf(req, " (%s)", name);

    if (uuid == UUID_PROTO_RFCOMM)
    {
        next_uint_is_rfcomm_channel = true;
    }
    else
    {
        next_uint_is_rfcomm_channel = false;
    }
}

void printString(int num_bytes, sdpRequest *req, const char *name)
{
    req_appendf(req, " %s \"", name);
    for (int i = 0; i < num_bytes; i++)
    {
        unsigned char c = ((unsigned char *) req->parse_ptr)[i];
        if (isPrint(c))
            req_appendf(req, "%c", c);
        else
            req_appendf(req, "\\x%02x", c);
    }
    req_appendf(req, "\"");
    inc_parse(req, num_bytes);
}

void parseDE(int level, sdpRequest *req);

void parseDEs(bool alt, u8 de_type, int level, int num_bytes, sdpRequest *req)
{
    req_appendf(req, "%ssequence(%d)\n", alt ? "alt " : "", num_bytes);
    int start_bytes = req->bytes_parsed;

    while (num_bytes > (req->bytes_parsed - start_bytes))
    {
        parseDE(level+1, req);
    }
}

void parseDE(int level, sdpRequest *req)
{
    int num_bytes = 0;
    printIndent(level, req);
    
    u8 de_type = parseDEHeader(req, &num_bytes);
    switch (de_type)
    {
        case DE_NULL:
            req_appendf(req, " null");
            break;
        case DE_UINT:
        case DE_INT:
        case DE_BOOL:
            printInt(de_type, level, num_bytes, req);
            break;
        case DE_UUID:
            printUUID(num_bytes, req);
            break;
        case DE_URL:
        case DE_STRING:
            printString(num_bytes, req, (de_type == DE_URL) ? "url" : "str");
            break;
        case DE_SEQ:
            parseDEs(false, de_type, level, num_bytes, req);
            return;
        case DE_ALT:
            parseDEs(true, de_type, level, num_bytes, req);
            return;
        default:
            req_appendf(req, " unknown type");
            inc_parse(req, num_bytes);
            break;
    }
    req_appendf(req, "\n");
}

bool parseAttrList(int level, sdpRequest *req)
{
    int num_bytes = 0;
    printIndent(level, req);
    if (parseDEHeader(req, &num_bytes) == DE_SEQ)
    {
        req_appendf(req, " attribute list(%d)\n", num_bytes);

        int start_bytes = req->bytes_parsed;
        while (num_bytes > req->bytes_parsed - start_bytes)
        {
            int id_len = 0;
            printIndent(level+1, req);
            u8 t = parseDEHeader(req, &id_len);
            if (t == DE_UINT && id_len == sizeof(u16))
            {
                u16 attr_id = get_u16(req);
                const char *name = getAttrIDName(attr_id);
                req_appendf(req, "attr(0x%04x,%s)\n", attr_id, name);
                parseDE(level + 2, req);
            }
            else
            {
                req_appendf(req, "\nERROR: expected an Attribute ID\n");
                printRaw(level, req);
                return false;
            }
        }
        return true;
    }

    req_appendf(req, "\nERROR: expected a DE sequence\n");
    printRaw(level, req);
    return false;
}

void parseServiceRecords(sdpRequest *req)
{
    int num_bytes = 0;
    req_appendf(req, "parseServiceRecords()\n");

    printf("Start parseServiceRecords...\n");

    req->parse_frame_len = req->frame[0].len;
    req->parse_ptr = req->frame[0].data;
    req->parse_frame_num = 0;  // Start from frame 0
    req->bytes_parsed = 0;
    req->rfcomm_channel = -1;   // Initialize to 0 or invalid

// FIXME: use malloc instead of static
req->output_buf = malloc(32768);
if (req->output_buf == NULL) {
    printf("malloc failed\n");
    exit(0);
}
req->output_size = 32768;
req->output_len = 0;

    printIndent(1, req);
    if (parseDEHeader(req, &num_bytes) == DE_SEQ)
    {
        req_appendf(req, "parsing %d bytes of service records\n", num_bytes);

        int record_num = 0;
        int start_bytes = req->bytes_parsed;
        while (num_bytes > (req->bytes_parsed - start_bytes))
        {
            req_appendf(req, "\n");
            printIndent(1, req);
            req_appendf(req, "Service Record(%d)\n", record_num++);
            if (!parseAttrList(2, req))
                break;
        }
    }
    else
    {
        printIndent(1, req);
        req_appendf(req, "no service records found\n");
    }

    req->parse_complete = true;

    printf("SDP len:%d\n%s\n", req->output_len, req->output_buf);

}

#if 0
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
#include "sdp_defs.h"
#include "sdp.h"

// Set to 1 to show DE type parsing info, 0 to hide
#define SHOW_DE_TYPE   0

// Helper macros/types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

u16 be16(u16 v)     { return ((v >> 8) & 0xff) | ((v << 8) & 0xff00); }

bool isPrint(u8 c)  { return (c >= 32 && c < 127); }

// Display raw bytes for debugging if needed
void printRaw(int level, sdpRequest *req)
{
    printf("frame(%d/%d) len=%d\n", req->parse_frame_num, req->num_frames, req->parse_frame_len);
    display_bytes("printRaw", req->parse_ptr, req->parse_frame_len);
}

void printIndent(int level)
{
    while (level > 0)
    {
        printf("    ");
        level--;
    }
}

// Increment parser pointer and handle frame boundaries
void inc_parse(sdpRequest *req, int amt) {
    if (amt > req->parse_frame_len) {
        fprintf(stderr, "Error: Attempting to consume more bytes (%d) than available (%d)\n", amt, req->parse_frame_len);
        // Handle error: possibly return or abort parsing here
        return;
    }

    req->parse_frame_len -= amt;
    req->parse_ptr += amt;
    req->bytes_parsed += amt;

    if (req->parse_frame_len == 0) {
        req->parse_frame_num++;
        if (req->parse_frame_num < req->num_frames) {
            req->parse_ptr = req->frame[req->parse_frame_num].data;
            req->parse_frame_len = req->frame[req->parse_frame_num].len;
        } else {
            // No more frames
            req->parse_ptr = NULL;
            req->parse_frame_len = 0;
        }
    }

    // No assert needed if you handle errors gracefully
}
// Byte getters
u8 get_u8(sdpRequest *req)
{
    assert(req->parse_ptr);
    u8 val = *req->parse_ptr;
    inc_parse(req,1);
    return val;
}

u16 get_u16(sdpRequest *req)
{
    u16 val = (get_u8(req) << 8) + (get_u8(req) << 0);
    return val;
}

u32 get_u32(sdpRequest *req)
{
    u32 val =
        (get_u8(req) << 24) +
        (get_u8(req) << 16) +
        (get_u8(req) <<  8) +
        (get_u8(req) <<  0);
    return val;
}

uint64_t get_uint64_t(sdpRequest *req)
{
    uint64_t hi = get_u32(req);
    uint64_t lo = get_u32(req);
    uint64_t val = (hi << 32) + lo;
    return val;
}

void get_u128(sdpRequest *req, uint64_t *l, uint64_t *h)
{
    *h = get_uint64_t(req);
    *l = get_uint64_t(req);
}

// Data Element (DE) Type constants
#define DE_NULL   0
#define DE_UINT   1
#define DE_INT    2
#define DE_UUID   3
#define DE_STRING 4
#define DE_BOOL   5
#define DE_SEQ    6
#define DE_ALT    7
#define DE_URL    8

// Size lookup table for DEs
typedef struct 
{
    int addl_bits;
    int num_bytes;
} de_size_table_t;

static de_size_table_t de_size_table[] =
{
    { 0, 1  },  // 0
    { 0, 2  },  // 1
    { 0, 4  },  // 2
    { 0, 8  },  // 3
    { 0, 16 },  // 4
    { 1, 1  },  // 5
    { 1, 2  },  // 6
    { 1, 4  },  // 7
};

// UUID lookup table
typedef struct
{
    int   uuid;
    const char* name;
} uuid_table_t;

// Add entries as in original code
static uuid_table_t uuid_table[] =
{
    { UUID_PROTO_SDP,               "SDP"       },
    { UUID_PROTO_UDP,               "UDP"       },
    { UUID_PROTO_RFCOMM,            "RFCOMM"    },
    { UUID_PROTO_TCP,               "TCP"       },
    { UUID_PROTO_TCS_BIN,           "TCS_BIN"   },
    { UUID_PROTO_TCS_AT,            "TCS_AT"    },
    { UUID_PROTO_ATT,               "ATT"       },
    { UUID_PROTO_OBEX,              "OBEX"      },
    { UUID_PROTO_IP,                "IP"        },
    { UUID_PROTO_FTP,               "FTP"       },
    { UUID_PROTO_HTTP,              "HTTP"      },
    { UUID_PROTO_WSP,               "WSP"       },
    { UUID_PROTO_BNEP,              "BNEP"      },
    { UUID_PROTO_UPNP,              "UPNP"      },
    { UUID_PROTO_HIDP,              "HIDP"      },
    { UUID_PROTO_HCRP_CTRL,         "HCRP_CTRL" },
    { UUID_PROTO_HCRP_DATA,         "HCRP_DATA" },
    { UUID_PROTO_HCRP_NOTE,         "HCRP_NOTE" },
    { UUID_PROTO_AVCTP,             "AVCTP"     },
    { UUID_PROTO_AVDTP,             "AVDTP"     },
    { UUID_PROTO_CMTP,              "CMTP"      },
    { UUID_PROTO_UDI,               "UDI"       },
    { UUID_PROTO_MCAP_CTRL,         "MCAP_CTRL" },
    { UUID_PROTO_MCAP_DATA,         "MCAP_DATA" },
    { UUID_PROTO_L2CAP,             "L2CAP"     },

    { UUID_SERVICE_SDP_SERVER,              "SDPServer"       },
    { UUID_SERVICE_BROWSE_GROUP,            "BrowseGroupDsc"  },
    { UUID_SERVICE_PUBLIC_BROWSE_GROUP,     "BrowseGroup"     },
    { UUID_SERVICE_SERIAL_PORT,             "SP"              },
    { UUID_SERVICE_LAN_ACCESS,              "LAN"             },
    { UUID_SERVICE_DIALUP_NET,              "DUN"             },
    { UUID_SERVICE_IRMC_SYNC,               "IRMCSync"        },
    { UUID_SERVICE_OBEX_OBJ_PUSH,           "OBEXObjPush"     },
    { UUID_SERVICE_OBEX_FILE_TRANSFER,      "OBEXObjTrnsf"    },
    { UUID_SERVICE_IRMC_SYNC_CMD,           "IRMCSyncCmd"     },
    { UUID_SERVICE_HEADSET,                 "Headset"         },
    { UUID_SERVICE_CORDLESS_TELEPHONY,      "CordlessTel"     },
    { UUID_SERVICE_AUDIO_SOURCE,            "AudioSource"     },
    { UUID_SERVICE_AUDIO_SINK,              "AudioSink"       },
    { UUID_SERVICE_AV_REMOTE_TARGET,        "AVRemTarget"     },
    { UUID_SERVICE_ADVANCED_AUDIO,          "AdvAudio"        },
    { UUID_SERVICE_AV_REMOTE,               "AVRemote"        },
    { UUID_SERVICE_VIDEO_CONFERENCING,      "VideoConf"       },
    { UUID_SERVICE_INTERCOM,                "Intercom"        },
    { UUID_SERVICE_FAX,                     "Fax"             },
    { UUID_SERVICE_HEADSET_AUDIO_GATEWAY,   "HeadsetAG"       },
    { UUID_SERVICE_WAP,                     "WAP"             },
    { UUID_SERVICE_WAP_CLIENT,              "WAP Client"      },
    { UUID_SERVICE_PANU,                    "PANU"            },
    { UUID_SERVICE_NAP,                     "NAP"             },
    { UUID_SERVICE_GN,                      "GN"              },
    { UUID_SERVICE_DIRECT_PRINTING,         "DirectPrint"     },
    { UUID_SERVICE_REFERENCE_PRINTING,      "RefPrint"        },
    { UUID_SERVICE_IMAGING,                 "Imaging"         },
    { UUID_SERVICE_IMAGING_RESPONDER,       "ImagingResponder"},
    { UUID_SERVICE_IMAGING_ARCHIVE,         "ImagingArchive"  },
    { UUID_SERVICE_IMAGING_REF_OBJS,        "ImagingRefObjs"  },
    { UUID_SERVICE_HANDSFREE,               "Handsfree"       },
    { UUID_SERVICE_HANDSFREE_AUDIO_GATEWAY, "HandsfreeAG"     },
    { UUID_SERVICE_DIRECT_PRINTING_REF_OBJS,"RefObjsPrint"    },
    { UUID_SERVICE_REFLECTED_UI,            "ReflectedUI"     },
    { UUID_SERVICE_BASIC_PRINTING,          "BasicPrint"      },
    { UUID_SERVICE_PRINTING_STATUS,         "PrintStatus"     },
    { UUID_SERVICE_HID,                     "HID"             },
    { UUID_SERVICE_HARDCOPY_CABLE_REPLACE,  "HCRP"            },
    { UUID_SERVICE_HCR_PRINT,               "HCRPrint"        },
    { UUID_SERVICE_HCR_SCAN,                "HCRScan"         },
    { UUID_SERVICE_COMMON_ISDN_ACCESS,      "CIP"             },
    { UUID_SERVICE_VIDEO_CONFERENCING_GW,   "VideoConf_GW"    },
    { UUID_SERVICE_UDI_MT,                  "UDI_MT"          },
    { UUID_SERVICE_UDI_TA,                  "UDI_TA"          },
    { UUID_SERVICE_AUDIO_VIDEO,             "AudioVideo"      },
    { UUID_SERVICE_SIM_ACCESS,              "SAP"             },
    { UUID_SERVICE_PHONEBOOK_ACCESS_PCE,    "PBAP_PCE"        },
    { UUID_SERVICE_PHONEBOOK_ACCESS_PSE,    "PBAP PSE"        },
    { UUID_SERVICE_PHONEBOOK_ACCESS,        "PBAP"            },
    { UUID_SERVICE_MAP_MSE,                 "MAP_MSE"         },
    { UUID_SERVICE_MAP_MCE,                 "MAP_MCE"         },
    { UUID_SERVICE_MAP,                     "MAP"             },
    { UUID_SERVICE_GNSS,                    "GNSS"            },
    { UUID_SERVICE_GNSS_SERVER,             "GNSS_Server"     },
    { UUID_SERVICE_PNP_INFO,                "PNPInfo"         },
    { UUID_SERVICE_GENERIC_NETWORKING,      "Networking"      },
    { UUID_SERVICE_GENERIC_FILE_TRANSGRT,   "FileTrnsf"       },
    { UUID_SERVICE_GENERIC_AUDIO,           "Audio"           },
    { UUID_SERVICE_GENERIC_TELEPHONY,       "Telephony"       },
    { UUID_SERVICE_UPNP,                    "UPNP"            },
    { UUID_SERVICE_UPNP_IP,                 "UPNP IP"         },
    { UUID_SERVICE_UPNP_PAN,                "UPNP PAN"        },
    { UUID_SERVICE_UPNP_LAP,                "UPNP LAP"        },
    { UUID_SERVICE_UPNP_L2CAP,              "UPNP L2CAP"      },
    { UUID_SERVICE_VIDEO_SOURCE,            "VideoSource"     },
    { UUID_SERVICE_VIDEO_SINK,              "VideoSink"       },
    { UUID_SERVICE_VIDEO_DISTRIBUTION,      "VideoDist"       },
    { UUID_SERVICE_HDP,                     "HDP"             },
    { UUID_SERVICE_HDP_SOURCE,              "HDP_SOURCE"      },
    { UUID_SERVICE_HDP_SINK,                "HDP_SINK"        },
    { UUID_SERVICE_APPLE_AGENT,             "AppleAgent"      },
};

#define UUID_TABLE_SIZE (sizeof(uuid_table)/sizeof(uuid_table_t))

// Attribute ID lookup
typedef struct
{
    int   attr_id;
    const char* name;
} attr_id_table_t;

static attr_id_table_t attr_id_table[] =
{
    { ATTR_ID_SERVICE_RECORD_HANDLE,             "SrvRecHndl"         },
    { ATTR_ID_SERVICE_CLASS_ID_LIST,             "SrvClassIDList"     },
    { ATTR_ID_SERVICE_RECORD_STATE,              "SrvRecState"        },
    { ATTR_ID_SERVICE_SERVICE_ID,                "SrvID"              },
    { ATTR_ID_PROTOCOL_DESCRIPTOR_LIST,          "ProtocolDescList"   },
    { ATTR_ID_BROWSE_GROUP_LIST,                 "BrwGrpList"         },
    { ATTR_ID_LANGUAGE_BASE_ATTRIBUTE_ID_LIST,   "LangBaseAttrIDList" },
    { ATTR_ID_SERVICE_INFO_TIME_TO_LIVE,         "SrvInfoTimeToLive"  },
    { ATTR_ID_SERVICE_AVAILABILITY,              "SrvAvail"           },
    { ATTR_ID_BLUETOOTH_PROFILE_DESCRIPTOR_LIST, "BTProfileDescList"  },
    { ATTR_ID_DOCUMENTATION_URL,                 "DocURL"             },
    { ATTR_ID_CLIENT_EXECUTABLE_URL,             "ClientExeURL"       },
    { ATTR_ID_ICON_URL,                          "IconURL"            },
    { ATTR_ID_ADDITIONAL_PROTOCOL_DESC_LISTS,    "AdditionalProtocolDescLists" },
    { ATTR_ID_SERVICE_NAME,                      "SrvName"            },
    { ATTR_ID_SERVICE_DESCRIPTION,               "SrvDesc"            },
    { ATTR_ID_PROVIDER_NAME,                     "ProviderName"       },
    { ATTR_ID_VERSION_NUMBER_LIST,               "VersionNumList"     },
    { ATTR_ID_GROUP_ID,                          "GrpID"              },
    { ATTR_ID_SERVICE_DATABASE_STATE,            "SrvDBState"         },
    { ATTR_ID_SERVICE_VERSION,                   "SrvVersion"         },
    { ATTR_ID_SECURITY_DESCRIPTION,              "SecurityDescription"},
    { ATTR_ID_SUPPORTED_DATA_STORES_LIST,        "SuppDataStoresList" },
    { ATTR_ID_SUPPORTED_FORMATS_LIST,            "SuppFormatsList"    },
    { ATTR_ID_NET_ACCESS_TYPE,                   "NetAccessType"      },
    { ATTR_ID_MAX_NET_ACCESS_RATE,               "MaxNetAccessRate"   },
    { ATTR_ID_IPV4_SUBNET,                       "IPv4Subnet"         },
    { ATTR_ID_IPV6_SUBNET,                       "IPv6Subnet"         },
    { ATTR_ID_SUPPORTED_CAPABILITIES,            "SuppCapabilities"   },
    { ATTR_ID_SUPPORTED_FEATURES,                "SuppFeatures"       },
    { ATTR_ID_SUPPORTED_FUNCTIONS,               "SuppFunctions"      },
    { ATTR_ID_TOTAL_IMAGING_DATA_CAPACITY,       "SuppTotalCapacity"  },
    { ATTR_ID_SUPPORTED_REPOSITORIES,            "SuppRepositories"   },
};

#define ATTR_ID_TABLE_SIZE (sizeof(attr_id_table)/sizeof(attr_id_table_t))

const char* getUUIDName(int uuid)
{
    for (unsigned int i = 0; i < UUID_TABLE_SIZE; i++)
    {
        if (uuid_table[i].uuid == uuid)
            return uuid_table[i].name;
    }
    return NULL;
}

const char* getAttrIDName(int attr_id)
{
    for (unsigned int i = 0; i < ATTR_ID_TABLE_SIZE; i++)
    {
        if (attr_id_table[i].attr_id == attr_id)
            return attr_id_table[i].name;
    }
    return "unknown ATTR_ID";
}

// Parse Data Element header
u8 parseDEHeader(sdpRequest *req, int *n)
{
    u8 de_hdr = get_u8(req);
    u8 de_type = de_hdr >> 3;
    u8 siz_idx = de_hdr & 0x07;
    
    if (de_size_table[siz_idx].addl_bits)
    {
        switch(de_size_table[siz_idx].num_bytes)
        {
            case 1: *n = get_u8(req); break;
            case 2: *n = get_u16(req); break;
            case 4: *n = get_u32(req); break;
            case 8: *n = (int)get_uint64_t(req); break;
            default:
                *n = 0;
                break;
        }
    }
    else
    {
        *n = de_size_table[siz_idx].num_bytes;
    }
    
#if SHOW_DE_TYPE
    char ts[100];
    snprintf(ts, sizeof(ts), "t(0x%02x=%d,%d,%d)", de_hdr, de_type, siz_idx, *n);
    printf("%-40s\n", ts);
#endif
    
    return de_type;
}

static bool next_uint_is_rfcomm_channel = false;

void printInt(u8 de_type, int level, int num_bytes, sdpRequest *req)
{
    switch(de_type)
    {
        case DE_UINT: printf(" uint"); break;
        case DE_INT:  printf(" int");  break;
        case DE_BOOL: printf(" bool"); break;
        default: break;
    }

    switch(num_bytes)
    {
        case 1:
        {
            u8 val = get_u8(req);
            printf("8  0x%02x", val);
            if (next_uint_is_rfcomm_channel && de_type == DE_UINT)
            {
                // This is the RFCOMM channel number
                req->rfcomm_channel = val;
                next_uint_is_rfcomm_channel = false;
                printf(" [RFCOMM channel]");
            }
            break;
        }
        case 2:
        {
            u16 val = get_u16(req);
            printf("16 0x%04x", val);
            break;
        }
        case 4:
        {
            u32 val = get_u32(req);
            printf("32 0x%08x", val);
            break;
        }
        case 8:
        {
            uint64_t val = get_uint64_t(req);
            printf("64 0x%016lx", val);
            break;
        }
        case 16:
        {
            uint64_t val1, val2;
            get_u128(req, &val1, &val2);
            printf("128 0x%016lx%016lx", val2, val1);
            break;
        }
        default:
        {
            printf(" err");
            inc_parse(req, num_bytes);
            break;
        }
    }
}

void printUUID(int num_bytes, sdpRequest *req)
{
    u32 uuid = 0;
    const char *s = NULL;
    switch(num_bytes)
    {
        case 2:
            uuid = get_u16(req);
            s = "uuid-16";
            break;
        case 4:
            uuid = get_u32(req);
            s = "uuid-32";
            break;
        case 16:
            printf(" uuid-128 ");
            for (int i = 0; i < 16; i++)
            {
                unsigned char c = ((unsigned char *) req->parse_ptr)[i];
                printf("%02x", c);
                if (i == 3 || i == 5 || i == 7 || i == 9)
                    printf("-");
            }
            inc_parse(req,16);
            return;
        default:
            printf(" *err*");
            inc_parse(req,num_bytes);
            return;
    }
    
    printf(" %s 0x%04x", s, uuid);
    const char *name = getUUIDName(uuid);
    if (name)
        printf(" (%s)", name);

    // Check if this is the RFCOMM UUID
    if (uuid == UUID_PROTO_RFCOMM)
    {
        // The next DE_UINT should be the channel number
        next_uint_is_rfcomm_channel = true;
    }
    else
    {
        next_uint_is_rfcomm_channel = false;
    }
}

void printString(int num_bytes, sdpRequest *req, const char *name)
{
    printf(" %s ", name);
    // Print all bytes, do not stop at '\0'
    printf("\"");
    for (int i = 0; i < num_bytes; i++)
    {
        unsigned char c = ((unsigned char *) req->parse_ptr)[i];
        if (isPrint(c))
            printf("%c", c);
        else
            printf("\\x%02x", c);
    }
    printf("\"");
    inc_parse(req, num_bytes);
}

void parseDE(int level, sdpRequest *req);

void parseDEs(bool alt, u8 de_type, int level, int num_bytes, sdpRequest *req)
{
    printf("%ssequence(%d)\n", alt ? "alt " : "", num_bytes);
    int start_bytes = req->bytes_parsed;

    while (num_bytes > (req->bytes_parsed - start_bytes))
    {
        parseDE(level+1, req);
    }
}

void parseDE(int level, sdpRequest *req)
{
    int num_bytes = 0;
    printIndent(level);
    
    u8 de_type = parseDEHeader(req, &num_bytes);
    switch (de_type)
    {
        case DE_NULL:
            printf(" null");
            break;
        case DE_UINT:
        case DE_INT:
        case DE_BOOL:
            printInt(de_type, level, num_bytes, req);
            break;
        case DE_UUID:
            printUUID(num_bytes, req);
            break;
        case DE_URL:
        case DE_STRING:
            printString(num_bytes, req, (de_type == DE_URL) ? "url" : "str");
            break;
        case DE_SEQ:
            parseDEs(false, de_type, level, num_bytes, req);
            return;
        case DE_ALT:
            parseDEs(true, de_type, level, num_bytes, req);
            return;
        default:
            printf(" unknown type");
            inc_parse(req, num_bytes);
            break;
    }
    printf("\n");
}

bool parseAttrList(int level, sdpRequest *req)
{
    int num_bytes = 0;
    printIndent(level);
    if (parseDEHeader(req, &num_bytes) == DE_SEQ)
    {
        printf(" attribute list(%d)\n", num_bytes);

        int start_bytes = req->bytes_parsed;
        while (num_bytes > req->bytes_parsed - start_bytes)
        {
            int id_len = 0;
            printIndent(level+1);
            u8 t = parseDEHeader(req, &id_len);
            if (t == DE_UINT && id_len == sizeof(u16))
            {
                u16 attr_id = get_u16(req);
                const char *name = getAttrIDName(attr_id);
                printf("attr(0x%04x,%s)\n", attr_id, name);
                parseDE(level + 2, req);
            }
            else
            {
                printf("\nERROR: expected an Attribute ID\n");
                printRaw(level, req);
                return false;
            }
        }
        return true;
    }

    printf("\nERROR: expected a DE sequence\n");
    printRaw(level, req);
    return false;
}

void parseServiceRecords(sdpRequest *req)
{
    int num_bytes = 0;
    printf("parseServiceRecords()\n");

    req->parse_frame_len = req->frame[0].len;
    req->parse_ptr = req->frame[0].data;
    req->parse_frame_num = 0;  // Start from frame 0
    req->bytes_parsed = 0;

    printIndent(1);
    if (parseDEHeader(req, &num_bytes) == DE_SEQ)
    {
        printf("parsing %d bytes of service records\n", num_bytes);

        int record_num = 0;
        int start_bytes = req->bytes_parsed;
        while (num_bytes > (req->bytes_parsed - start_bytes))
        {
            printf("\n");
            printIndent(1);
            printf("Service Record(%d)\n", record_num++);
            if (!parseAttrList(2, req))
                break;
        }
    }
    else
    {
        printIndent(1);
        printf("no service records found\n");
    }

    req->parse_complete = true;
}
#endif
