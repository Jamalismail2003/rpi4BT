#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/procmgr.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <pthread.h>
#include <assert.h>
#include "hci_defs.h"
#include "hci.h"
#include "rfcomm.h"
#include "hid.h"
#include "sdp.h"
#include "sdp_defs.h"
#include "utils.h"
#include "menu.h"


typedef struct 
{
	u16 uuid;
	const char *name;
} select_uuid_type;


typedef struct
{
	const char *addr;
	const char *name;
	u8 use_channel;
} static_device;


select_uuid_type select_uuids[] =
{
	{ UUID_PROTO_L2CAP,			 "PROTO_L2CAP" },
	{ UUID_PROTO_SDP,			 "PROTO_SDP" },			
	{ UUID_PROTO_RFCOMM,         "PROTO_RFCOMM" },
	{ UUID_SERVICE_SERIAL_PORT,  "SERVICE_SERIAL_PORT" },
};

#define NUM_UUIDS  			(sizeof(select_uuids)/sizeof(select_uuid_type))


#define PRE_POPULATE_DEVICES   	0

#define STATE_NONE              	0
#define STATE_SELECT_DEVICE         1
#define STATE_SELECT_UUID           2
#define STATE_STARTING             	99
 
hciRemoteDevice *m_selected_device = NULL;
//extern hci_context_t myhciLayer;
//extern rfcommLayer myRfcommLayer;

uint16_t m_state = STATE_STARTING;


void hci_startInquiry(hci_context_t *context, unsigned nSeconds)
{
	printf("startInquiry(%d)\n",nSeconds);
	assert(1 <= nSeconds && nSeconds <= 61);

	hci_inquiry_command cmd;
	cmd.header.opcode = HCI_OP_LINK_INQUIRY;
	cmd.header.length = 5; // FIXME: this is not needed, test before removing
	cmd.lap[0] = HCI_LINK_INQUIRY_LAP_GIAC       & 0xFF;
	cmd.lap[1] = HCI_LINK_INQUIRY_LAP_GIAC >> 8  & 0xFF;
	cmd.lap[2] = HCI_LINK_INQUIRY_LAP_GIAC >> 16 & 0xFF;
	cmd.inquiry_length = HCI_LINK_INQUIRY_LENGTH(nSeconds);
	cmd.num_responses = HCI_LINK_INQUIRY_NUM_RESPONSES_UNLIMITED;
	printf("HCI_OP_LINK_INQUIRY ...\n");
	//sendCommand(&cmd, sizeof cmd);
	hci_send_command(context, &cmd, sizeof cmd);
}


void listDevices(hci_context_t *context)
{
	int dev_num = 0;
	//hciLayer *pHCI = m_pBT->getHCILayer();
	//hciRemoteDevice *device = pHCI->getDeviceList();
	hciRemoteDevice *device = context->m_first_device;

	while (device)
	{
		char status[] = "not connected";
		if (device->handle)
			strcpy(status, "connected");

		printf("   %s[%d] %s  %-20s %-18s %s\n", device == m_selected_device ? "*" : " ",
			dev_num++, addrToString(device->addr), device->name, (const char *) status, device->link_key_type ? "PAIRED" : "");

		device = device->next;
	}
}

hciRemoteDevice *selectDevice_by_addr(const char *mac_addr)
{
#if 0
	hciRemoteDevice *device = myhciLayer->m_first_device;
	while (device)
	{
		if( !memcmp(addrToString(device->addr), mac_addr, 17) ) {
			m_selected_device = device;
			return m_selected_device;//device;
		}
		device = device->next;
	}
	log_error("Invalid device mac addr: %s\n", mac_addr);
#else
	hci_context_t *hci_ctx = hci_get_context();
	hciRemoteDevice *device = hci_ctx->m_first_device;

	while (device)
	{
		if( !memcmp(addrToString(device->addr), mac_addr, 17) ) {
			m_selected_device = device;
			return m_selected_device;
		}
		device = device->next;
	}
	log_error("Invalid device mac addr: %s\n", mac_addr);
#endif

	return NULL;
}

void selectDevice(hci_context_t *context, u8 num)
{
	int dev_num = 0;

	hciRemoteDevice *device = context->m_first_device;//pHCI->getDeviceList();
	while (device)
	{
		if (num == dev_num)
		{
			printf("    device %s(%s) selected\n",
				   addrToString(device->addr),
				   device->name);
			m_selected_device = device;
			return;
		}
		dev_num++;
		device = device->next;
	}
	printf("illegal device number: %d\n",num);
}


void menu(hci_context_t *context)
{
	static rfChannel *m_rfChannel = NULL;

	if (m_state == STATE_STARTING)
	{
		m_state = STATE_NONE;
		printf("\n");
		printf("btTester started ....  Press:\n");
		printf("    i = to start an inquiry\n");
		printf("    l = to list devices\n");
		printf("    d = to select a device\n");
		printf("    g = to set idle request\n");
		printf("    p = to pair with device\n");
		printf("    u = to unpair with a device\n");
		printf("    s = to perform an SDP SEARCH_ATTR request\n");
		
		printf("    R = to connect (open) to a RFCOMM channel\n");
		printf("    o = to output (print) some characters to the RFCOMM channel\n");
		printf("    c = to disconnect (close) the RFCOMM channel\n");
		printf("    H = to connect HID device\n");

		printf("    h = help (to see this list)\n");
		printf("\n");	
	}

	char c[2];
	if (scanf(" %1s", c) == 1) // try scanf("%d")
	{
		// clear the state machine if !0..9 pressed
		
		if (c[0] < '0' || c[0] > '9')
		{
			m_state = STATE_NONE;
		}

		if (c[0] == 'h')
		{
			m_state = STATE_STARTING;
		}
		//-----------------------------
		// inquiry
		//-----------------------------
		
		else if (c[0] == 'i')
		{
			printf("Inquiry is running for %u seconds\n", INQUIRY_SECONDS);
			hci_startInquiry(context, INQUIRY_SECONDS);
			//hciLayer *hci = m_pBT->getHCILayer();
			//hci->startInquiry(INQUIRY_SECONDS);
		}

		//------------------------------
		// pairing
		//------------------------------
		else if (c[0] == 'p')
		{
			if (m_selected_device) {
				hciLayer_startConnection(context, m_selected_device->addr);
			} else {
				printf("Select device to pair\n");
			}
		}

		else if (c[0] == 'u')
		{
			assert(m_selected_device);
			assert(m_selected_device->link_key_type);
			if (m_selected_device &&
				m_selected_device->link_key_type)
			{
				printf("kernel Unpairing from %s(%s) ...\n",
					addrToString(m_selected_device->addr),
					m_selected_device->name);
				unpair(context, m_selected_device);
				
				printf("Unpairing from %s(%s) ...\n",
					addrToString(m_selected_device->addr),
					m_selected_device->name);
				m_selected_device->link_key_type = 0;
				memset(m_selected_device->link_key,0,BT_LINK_KEY_SIZE);
				saveDevices(context);
			}
		}
		
		//-----------------------------
		// list & select device & uuid
		//-----------------------------
		
		else if (c[0] == 'l')
		{
//			hciLayer *pHCI = m_pBT->getHCILayer();
//			printf("\n");
//			printf("Listing %d hciRemoteDevices\n",pHCI->getNumDevices());
			listDevices(context);
		}
		else if (c[0] == 'd')
		{
			printf("\n");
			printf("Select a device:\n");
			listDevices(context);
			m_state = STATE_SELECT_DEVICE;			
		}
		else if (c[0] == 's')
		{
			printf("\n");
			printf("Select a UUID for SDP request:\n");
			for (u8 i=0; i < NUM_UUIDS; i++)
			{
				printf("    [%d] 0x%04x  %s\n",
					i,
					select_uuids[i].uuid,
					select_uuids[i].name);
			}
			m_state = STATE_SELECT_UUID;			
		}
		else if (c[0] >= '0' && c[0] <= '9')
		{
			if (m_state == STATE_SELECT_DEVICE)
				selectDevice(context, (u8)atoi(c));
				
			// DO THE SDP REQUEST
#if 1
			if (m_state == STATE_SELECT_UUID)
			{
				u8 num = c[0] - '0';
				if (num > NUM_UUIDS)
				{
					printf("illegal UUID number %d\n",num);
				}
				else
				{
					select_uuid_type *pUUID = &select_uuids[num];
					assert(m_selected_device);
					if (m_selected_device)
					{
						printf("calling sdpRequest(%s(%s), 0x%04x(%s), 0x0000, 0xffff) ...\n",
							addrToString(m_selected_device->addr),m_selected_device->name,pUUID->uuid,pUUID->name);
					
						//sdpRequest *request = m_pSDP->doSdpRequest(m_selected_device->addr,pUUID->uuid,0x0000, 0xffff);
						sdpRequest *request = sdpLayer_doSdpRequest(context->user_data, m_selected_device->addr,pUUID->uuid,0x0000, 0xffff);
						if (!request)
							printf("Could not call doSdpRequest()!!");
					}
				}
			}
#endif
		}

		//-----------------------------
		// rfcomm
		//-----------------------------
		
		else if (c[0] == 'c')
		{
			assert(m_rfChannel);
			assert(m_rfChannel->channel_state & RF_CHANNEL_STATE_OPEN);			
			if (m_rfChannel && (m_rfChannel->channel_state & RF_CHANNEL_STATE_OPEN))
			{
				assert(m_rfChannel->session);
				assert(m_rfChannel->session->lcn);
				assert(m_rfChannel->session->lcn->device);
				
				printf("closing m_RFChannel(%s(%s), 0x%02x) ...\n",
					addrToString(m_rfChannel->session->lcn->device->addr),
					m_rfChannel->session->lcn->device->name,
					m_rfChannel->channel_num);
				
				closeRFChannel(m_rfChannel);
				m_rfChannel = 0;
			}
		}
		else if (c[0] == 'H')
		{
			printf("HID device - bd_addr: %s Name:%s ...\n", 
				addrToString(m_selected_device->addr),m_selected_device->name);

			assert(m_selected_device);

			if (m_selected_device)
			{
				hid_connect(context->user_data, m_selected_device->addr);
			}
		}
		else if (c[0] == 'r')
		{
			assert(m_selected_device);

			if (m_selected_device)
			{
				//u8 use_channel = 0xd; // 00:1A:7D:DA:71:0A
				u8 use_channel = 0x1; // iPhone "98:50:2E:6B:1A:C0"
				//u8 use_channel = 0x3; // M1 F8:4D:89:65:40:A3
				//u8 use_channel = 0x8; // Mac 14:7D:DA:4D:01:3C

#if 0
				if (!memcmp(m_selected_device->addr,strToBtAddr(lenovo),BT_ADDR_SIZE))
					use_channel = 0x03;		// rfcomm_channel 0x03 for lenovo, from SDP				
				else if (!memcmp(m_selected_device->addr,strToBtAddr(p10),BT_ADDR_SIZE))
					use_channel = 0x02;		// 0x02 for cmManagerBT, service handle 0x00010009
				else
					printf("WARNING: using default channel(0x%02x)\n",use_channel);
#endif
//raise(SIGSTOP);
				printf("calling openRFChannel(%s(%s), 0x%02x) ...\n", addrToString(m_selected_device->addr),m_selected_device->name, use_channel);

				m_rfChannel = openRFChannel(context->user_data, m_selected_device->addr, use_channel);
				//m_rfChannel = rfcommLayer_openRFChannel(&myRfcommLayer, m_selected_device->addr, use_channel);

			
				if (!m_rfChannel)
					printf("Could not openRFChannel()!!\n");
				else
					printf("opening channel(%d)\n",m_rfChannel->channel_num);
			}
		}

		else if (c[0] == 'o')
		{
		    rfChannel *avail[5];

			int num_avail = getOpenChannels(context->user_data, avail,5);
			if (!num_avail)
			{
				log_error("no open rfcomm channels available for output\n");
				return;
			}
			
			if (num_avail > 0)//1)
				printf("outputting to %d channels ...\n", num_avail);

			//u8 msg[] = "Hello from rPi Bluetooth\n";
			//u8 msg[] = "No Flow Control: Since flow control is off, neither side will wait for an explicit acknowledgment or control signal\0";
			u8 sync[] = {0xFF, 0x5A, 0x00, 0x1A, 0x80, 0xD9, 0x00, 0x00, 0x34, 0x01, 0x05, 0x08, 0x00, 0x05, 0xDC, 0x00, 0xFA, 0x1E, 0x03, 0x01, 0x00, 0x02, 0x02, 0x01, 0x01, 0xEF};
			for (int i=0; i < num_avail; i++)
			{
				rfChannel *channel = avail[i];
#if 0
				int j;
				for(j=0; j < strlen(msg); j+=127) {

					rfLayer_sendData(channel, &msg[j], (j < strlen(msg)) ? 127 : (127 - strlen(msg)));
				}

#else
				rfLayer_sendData(channel, sync, 26);
#endif
			}
		}
		
	}	// m_pSerial->read()

}	// CKernel::Run()
