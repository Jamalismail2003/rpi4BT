#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <unistd.h>
#include "hci_defs.h"
#include "hci.h"
#include "utils.h"
//#include "lcapLayer.h"
//#include "uartTransport.h"

static u8 Firmware[] =
{
	#include "BCM4345C0.h"
};


//extern hci_context_t myhciLayer;


const char *getBTErrorString(u8 status)
{
	switch(status)
	{
		case 0x01 : return "Unknown HCI Command";
		case 0x02 : return "No Connection";
		case 0x03 : return "Hardware Failure";
		case 0x04 : return "Page Timeout";
		case 0x05 : return "Authentication Failure";
		case 0x06 : return "Key Missing";
		case 0x07 : return "Memory Full";
		case 0x08 : return "Connection Timeout";
		case 0x09 : return "Max Number Of Connections";
		case 0x0A : return "Max Number Of SCO Connections To A Device";
		case 0x0B : return "ACL Connection Already Exists";
		case 0x0C : return "Command Disallowed";
		case 0x0D : return "Host Rejected Due To Limited Resources";
		case 0x0E : return "Host Rejected Due To Security Reasons";
		case 0x0F : return "Host Rejected Due To A Remote Device Only A Personal Device";
		case 0x10 : return "Host Timeout";
		case 0x11 : return "Unsupported Feature Or Parameter Value";
		case 0x12 : return "Invalid HCI Command Parameters";
		case 0x13 : return "Other End Terminated Connection: User Ended Connection";
		case 0x14 : return "Other End Terminated Connection: Low Resources";
		case 0x15 : return "Other End Terminated Connection: About To Power Off";
		case 0x16 : return "Connection Terminated By Local Host";
		case 0x17 : return "Repeated Attempts";
		case 0x18 : return "Pairing Not Allowed";
		case 0x19 : return "Unknown LMP PDU";
		case 0x1A : return "Unsupported Remote Feature";
		case 0x1B : return "SCO Offset Rejected";
		case 0x1C : return "SCO Interval Rejected";
		case 0x1D : return "SCO Air Mode Rejected";
		case 0x1E : return "Invalid LMP Parameters";
		case 0x1F : return "Unspecified Error";
		case 0x20 : return "Unsupported LMP Parameter";
		case 0x21 : return "Role Change Not Allowed";
		case 0x22 : return "LMP Response Timeout";
		case 0x23 : return "LMP Error Transaction Collision";
		case 0x24 : return "LMP PDU Not Allowed";
		case 0x25 : return "Encryption Mode Not Acceptable";
		case 0x26 : return "Unit Key Used";
		case 0x27 : return "QoS Not Supported";
		case 0x28 : return "Instant Passed";
		case 0x29 : return "Pairing With Unit Key Not Supported";
	}
	return "reserved For Future Use";
}

const char *getBTEventName(u8 event_code)
{
	switch(event_code)
	{
		case 0x01 : return "EVENT_INQUIRY_COMPLETE";						
		case 0x02 : return "EVENT_INQUIRY_RESULT";					
		case 0x03 : return "EVENT_CONNECTION_COMPLETE";
		case 0x04 : return "EVENT_CONNECTION_REQUEST";				
		case 0x05 : return "EVENT_DISCONNECTION_COMPLETE";
		case 0x06 : return "EVENT_AUTHENTICATION_COMPLETE";			
		case 0x07 : return "EVENT_REMOTE_NAME_REQUEST_COMPLETE";
		case 0x08 : return "EVENT_ENCRYPTION_CHANGE";		
		case 0x09 : return "EVENT_CHANGE_LINK_KEY_COMPLETE";
		case 0x0A : return "EVENT_MASTER_LINK_KEY_COMPLETE";			
		case 0x0B : return "EVENT_READ_SUPPORTED_FEATURES_COMPLETE";
		case 0x0C : return "EVENT_READ_REMOTE_VERSION_COMPLETE";	
		case 0x0D : return "EVENT_Q0S_SETUP_COMPLETE";		
		case 0x0E : return "EVENT_COMMAND_COMPLETE";				
		case 0x0F : return "EVENT_COMMAND_STATUS";					
		case 0x10 : return "EVENT_HARDWARE_ERROR";					
		case 0x11 : return "EVENT_FLUSH_OCCURED";					
		case 0x12 : return "EVENT_ROLE_CHANGE";					
		case 0x13 : return "EVENT_NUMBER_OF_COMPLETED_PACKETS";
		case 0x14 : return "EVENT_MODE_CHANGE";		
		case 0x15 : return "EVENT_RETURN_LINK_KEYS";
		case 0x16 : return "EVENT_PIN_CODE_REQUEST";					
		case 0x17 : return "EVENT_LINK_KEY_REQUEST";					
		case 0x18 : return "EVENT_LINK_KEY_NOTIFICATION";
		case 0x19 : return "EVENT_LOOPBACK_COMMAND";			
		case 0x1A : return "EVENT_DATA_BUFFER_OVERFLOW";
		case 0x1B : return "EVENT_MAX_SLOTS_CHANGE";				
		case 0x1C : return "EVENT_READ_CLOCK_OFFSET_COMPLETE";
		case 0x1D : return "EVENT_CONNECTION_PACKET_TYPE_CHANGED";
		case 0x1E : return "EVENT_QOS_VIOLATION";	
		case 0x1F : return "EVENT_PAGE_SCAN_MODE_CHANGE";
		case 0x20 : return "EVENT_PAGE_SCAN_REPETITION_MODE_CHANGE";
		case 0x23 : return "EVENT_HCI_EVENT_READ_RMT_EXT_FEATURES_COMP_EVT";
		case 0x31 : return "EVENT_HCI_EVENT_IO_CAPABILITY_REQUEST_EVT";
		case 0x32 : return "EVENT_HCI_EVENT_IO_CAPABILITY_RESPONSE_EVT";
		case 0x33 : return "EVENT_HCI_EVENT_USER_CONFIRMATION_REQUEST_EVT";
		case 0x36 : return "EVENT_HCI_EVENT_SIMPLE_PAIRING_COMPLETE_EVT";
	}
	return "unknown EVENT_CODE";
}


#define BUF_SIZE 18
#if 1
const char *addrToString(const unsigned char *addr) {
    // Static buffer to hold the formatted string
    static char buf[BUF_SIZE];

    // Format the string directly into the static buffer
    snprintf(buf, BUF_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    // Return the static buffer
    return buf;
}
#else
const char *addrToString(const unsigned char *addr) 
{
    // Allocate memory for a string
    char *s = (char *)malloc(BUF_SIZE * sizeof(char));
    if (s == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Format the string
    snprintf(s, BUF_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X", 
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    // Allocate memory for the static buffer
    static char buf[BUF_SIZE];

    // Copy the formatted string to the static buffer
    strncpy(buf, s, BUF_SIZE - 1);
    buf[BUF_SIZE - 1] = '\0'; // Ensure null-termination

    // Free the allocated memory
    free(s);

    // Return the static buffer
    return buf;
}
#endif
const char *deviceClassToString(const unsigned char *cls) 
{
    // Allocate memory for a string
    char *s = (char *)malloc(BUF_SIZE * sizeof(char));
    if (s == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Format the string
    snprintf(s, BUF_SIZE, "%02X%02X%02X", cls[0], cls[1], cls[2]);

    // Allocate memory for the static buffer
    static char buf[BUF_SIZE];

    // Copy the formatted string to the static buffer
    strncpy(buf, s, BUF_SIZE - 1);
    buf[BUF_SIZE - 1] = '\0'; // Ensure null-termination

    // Free the allocated memory
    free(s);

    // Return the static buffer
    return buf;
}


const u8 *strToBtAddr(const char *str)
{
	static u8 buf[BT_ADDR_SIZE];
	u8 *op = &buf[BT_ADDR_SIZE-1];
	const char *ip = str;
	
	int len = 0;
	while (len < BT_ADDR_SIZE)
	{
		char n1 = *ip++;
		char n2 = *ip++;
		u8 val =
			 (n2 >= 'A' ? 10+n2-'A' : n2-'0') +
			((n1 >= 'A' ? 10+n1-'A' : n1-'0') << 4);
		*op-- = val;
		ip++;
		len++;
	}
	return (const u8 *) buf;
}


//----------------------------------
// device primitives
//----------------------------------


bool checkDeviceName(hci_context_t *context, hciRemoteDevice *device)
{
	if (!device->name[0])
	{
		hci_remote_name_request_command cmd;
		cmd.header.opcode = HCI_OP_LINK_REMOTE_NAME_REQUEST;
		memcpy(cmd.addr, device->addr, BT_ADDR_SIZE);
		#if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
			cmd.page_scan_repetition_mode = device->page_mode;
		#else
			cmd.page_scan_repetition_mode = 0;
		#endif
		cmd.reserved = 0;
		cmd.clock_offset = HCI_LINK_CLOCK_OFFSET_INVALID;
		log_info("<--  HCI - REMOTE_NAME_REQUEST to %s",addrToString(device->addr));
		//sendCommand(&cmd, sizeof cmd);
		hci_send_command(context, &cmd, sizeof cmd);
		return false;
	}
	return true;
}


#if 0
void removeDevice(hciRemoteDevice *device)
{
	log_indented(8, "Removing device %s",addrToString(device->addr));

	hciRemoteDevice *prev = device->prev;
	hciRemoteDevice *next = device->next;
	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;
	if (device == myhciLayer->m_first_device)
		myhciLayer->m_first_device = next;
	if (device == myhciLayer->m_last_device)
		myhciLayer->m_first_device = prev;
	myhciLayer->m_num_devices--;
	free(device);
}
#endif

hciRemoteDevice *addDevice(hci_context_t *context, const u8 *addr, const char *name)
{
	log_indented(8, "addDevice(%s) %s",addrToString(addr),name ? name : "\n");
	hciRemoteDevice *device = findDeviceByAddr(context, addr);
	if (device)
	{
		log_indented(8, "device %s already exists",addrToString(addr));
		return device;
	}
	
	device = (hciRemoteDevice *) malloc(sizeof(hciRemoteDevice));
	assert(device);
	if (!device) return 0;
	
	context->m_num_devices++;
	memset(device,0,sizeof(hciRemoteDevice));
		
	device->prev = context->m_last_device;
	if (context->m_last_device)
		context->m_last_device->next = device;
	context->m_last_device = device;
	
	if (!context->m_first_device)
		context->m_first_device = device;
		
	memcpy(device->addr,addr,BT_ADDR_SIZE);
	device->next_lcap_id = LCAP_STARTING_TXN_ID;
	device->next_lcap_cid = LCAP_STARTING_CID;
	if (name)
		strcpy(device->name,name);

	return device;
}




hciRemoteDevice *findDeviceByAddr(hci_context_t *context, const u8 *addr)
{
	hciRemoteDevice *device = context->m_first_device;
	while (device)
	{
		if (!memcmp(addr,device->addr,BT_ADDR_SIZE))
			return device;
		device = device->next;
	}
	return 0;
}

#if 0
hciRemoteDevice *findDeviceByName(const char *name)
{
	hciRemoteDevice *device = myhciLayer->m_first_device;
	while (device)
	{
		if (!strcmp(name,device->name))
			return device;
		device = device->next;
	}
	return 0;
}
#endif

hciRemoteDevice *findDeviceByHandle(hci_context_t *context, u16 handle)
{
	assert(handle);
	hciRemoteDevice *device = context->m_first_device;
	while (device)
	{
		if (handle == (device->handle  & 0x0fff))
			return device;
		device = device->next;
	}
	return 0;
}

void unpair(hci_context_t *context, hciRemoteDevice *device)
{
	assert(device);
	assert(device->link_key_type);
	if (device &&
		device->link_key_type)
	{
		log_indented(8, "unpair %s(%s) ...\n",
			addrToString(device->addr),
			device->name);
		device->link_key_type = 0;
		memset(device->link_key,0,BT_LINK_KEY_SIZE);
		#if HCI_USE_FAT_DATA_FILE
			saveDevices(context);
		#endif
	}
}




bool hcibase_processCommandCompleteEvent(hci_context_t *context, const void *buffer, u16 length)
{
	assert(length >= sizeof(hci_event_header));
	hci_command_complete_event *pCommandComplete = (hci_command_complete_event *) buffer;

	switch (pCommandComplete->command_opcode)
	{		
		case HCI_OP_INFORM_READ_BD_ADDR:
		{
			assert(length >= sizeof(hci_read_bdaddr_complete_event));
			hci_read_bdaddr_complete_event *pEvent = (hci_read_bdaddr_complete_event *) buffer;
			memcpy(context->m_local_addr, pEvent->addr, BT_ADDR_SIZE);

			log_indented(8, "Local BT Address: %02X:%02X:%02X:%02X:%02X:%02X",
				(unsigned) context->m_local_addr[5],
				(unsigned) context->m_local_addr[4],
				(unsigned) context->m_local_addr[3],
				(unsigned) context->m_local_addr[2],
				(unsigned) context->m_local_addr[1],
				(unsigned) context->m_local_addr[0]);

			log_indented(8, "Local Device Class: %06x",context->m_device_class);
					// we only send out 3 bytes
#if 1
			// 
			hci_command_header cmd;
			cmd .opcode = HCI_OP_INFORM_READ_LOCAL_SUPPORTED_FEATURES;
			cmd.length = 0;
			//sendCommand(&cmd, sizeof cmd);
			hci_send_command(context, &cmd, sizeof cmd);
#else					
			hci_write_class_of_device_command cmd;
			cmd.header.opcode = HCI_OP_BASEBAND_WRITE_CLASS_OF_DEVICE;
			cmd.class_of_device[0] = myhciLayer->m_device_class       & 0xFF;
			cmd.class_of_device[1] = myhciLayer->m_device_class >> 8  & 0xFF;
			cmd.class_of_device[2] = myhciLayer->m_device_class >> 16 & 0xFF;

			log_info("<--  HCI - HCI_OP_BASEBAND_WRITE_CLASS_OF_DEVICE");
			sendCommand(&cmd, sizeof cmd);
#endif
			return true;
		}

		case HCI_OP_INFORM_READ_LOCAL_SUPPORTED_FEATURES:
		{
			log_info("--> HCI_OP_INFORM_READ_LOCAL_SUPPORTED_FEATURES");

			hci_read_local_supported_features_event *pEvent = (hci_read_local_supported_features_event *) buffer;
			// TODO: printing the feature using the structure seems to be wrong by 3 bytes
			//       printing raw data seems to be correct
			//       0e 0c 01 03 10 00 - [bf fe cf fe db ff 7b 87]
			//       This is not critial, it just print the supported feature by the local Host
			log_indented(8, "Local Supported Feature: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", 
				pEvent->features[0], pEvent->features[1], pEvent->features[2], pEvent->features[3], 
				pEvent->features[4], pEvent->features[5], pEvent->features[6], pEvent->features[7]);
			/* 
				6.	Byte 6 (0xff):
				•	Bit 0: Extended Inquiry Response
				•	Bit 1: Simultaneous LE and BR/EDR (controller)
				•	Bit 2: Secure Simple Pairing (SSP)
				•	Bit 3: Encapsulated PDU
				•	Bit 4: Erroneous Data Reporting
				•	Bit 5: Non-flushable Packet Boundary Flag
				•	Bit 6: Link Supervision Timeout Changed Event
				•	Bit 7: Inquiry TX Power Level*/			


			hci_write_class_of_device_command cmd;
			cmd.header.opcode = HCI_OP_BASEBAND_WRITE_CLASS_OF_DEVICE;
			cmd.class_of_device[0] = context->m_device_class       & 0xFF;
			cmd.class_of_device[1] = context->m_device_class >> 8  & 0xFF;
			cmd.class_of_device[2] = context->m_device_class >> 16 & 0xFF;

			log_info("<--  HCI - HCI_OP_BASEBAND_WRITE_CLASS_OF_DEVICE");
			//sendCommand(&cmd, sizeof cmd);
			hci_send_command(context, &cmd, sizeof cmd);
			return true;
		}


		case HCI_OP_BASEBAND_WRITE_CLASS_OF_DEVICE:
		{
			strcpy((char*)context->m_local_name, "iBashe BT Stack");
			log_indented(8, "Local Name: '%s'",context->m_local_name);
			hci_write_local_name_command cmd;
			cmd.header.opcode = HCI_OP_BASEBAND_WRITE_LOCAL_NAME;
			memcpy(cmd.local_name, context->m_local_name, sizeof cmd.local_name);
			log_info("<--  HCI - HCI_OP_BASEBAND_WRITE_CLASS_OF_DEVICE");
			hci_send_command(context, &cmd, sizeof cmd);
			return true;
		}

		case HCI_OP_BASEBAND_WRITE_LOCAL_NAME:
		{
			// SCAN_ENABLE turns the radio on!
			// So we are effectively "started" after this command
#if 0
			hci_write_scan_enable_command cmd;
			cmd.header.opcode = HCI_OP_BASEBAND_WRITE_SCAN_ENABLE;
			cmd.scan_enable = HCI_LINK_SCAN_ENABLE_BOTH_ENABLED;
			log_info("<--  HCI - HCI_OP_BASEBAND_WRITE_LOCAL_NAME");
			hci_send_command(context, &cmd, sizeof cmd);
#else
			hci_write_simple_pairing_mode cmd;
			cmd.header.opcode = HCI_OP_WRITE_SIMPLE_PAIRING_MODE;
			cmd.simple_pairing_mode = 1;
			hci_send_command(context, &cmd, sizeof cmd);

			log_info("<--  HCI - HCI_OP_WRITE_SIMPLE_PAIRING_MODE");
#endif
			return true;
		}

		case HCI_OP_WRITE_SIMPLE_PAIRING_MODE:
		{
#if 0
			hci_write_scan_enable_command cmd;
			cmd.header.opcode = HCI_OP_BASEBAND_WRITE_SCAN_ENABLE;
			cmd.scan_enable = HCI_LINK_SCAN_ENABLE_BOTH_ENABLED;
			log_info("<--  HCI - HCI_OP_BASEBAND_WRITE_LOCAL_NAME");
			hci_send_command(context, &cmd, sizeof cmd);
			return true;
#else
			hci_command_header cmd;
			cmd .opcode = HCI_OP_READ_SIMPLE_PAIRING_MODE;
			hci_send_command(context, &cmd, sizeof cmd);
			log_info("<--  HCI - HCI_OP_READ_SIMPLE_PAIRING_MODE");
			return true;
#endif
		}

		case HCI_OP_READ_SIMPLE_PAIRING_MODE:
		{
				hci_write_scan_enable_command cmd;
				cmd.header.opcode = HCI_OP_BASEBAND_WRITE_SCAN_ENABLE;
				cmd.scan_enable = HCI_LINK_SCAN_ENABLE_BOTH_ENABLED;
				log_info("<--  HCI - HCI_OP_BASEBAND_WRITE_LOCAL_NAME");
				hci_send_command(context, &cmd, sizeof cmd);
				return true;
		}

		case HCI_OP_BASEBAND_WRITE_SCAN_ENABLE:
			// LOG("finished WRITE_SCAN_ENABLE",0);
			log_indented(8, "Setup complete");

/* FIXME: create command structure "HCI_Set_Event_Mask"
	•	01: HCI Command Packet
	•	01 0C: Opcode for HCI_Set_Event_Mask
	•	08: Parameter Length (8 bytes)
	•	FF FF FF FF FF FF FF FF: Event Mask (all events enabled)
*/
u8 cmd[] = {0x01, 0x0C, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
hci_send_command(context, &cmd, sizeof cmd);

			context->base_m_is_setup = true;
			return true;
		default:
			log_error("Unhandled event:%d", pCommandComplete->command_opcode);
	}	// switch

	return false;	// the event was not handled
}




bool m_pVendor_processCommandCompleteEvent(hci_context_t *context, const void *buffer, u16 length)
{
	assert(length >= sizeof(hci_event_header));
	hci_command_complete_event *pCommandComplete = (hci_command_complete_event *) buffer;
	
	switch (pCommandComplete->command_opcode)
	{
		case HCI_OP_VENDOR_DOWNLOAD_MINIDRIVER:
		case HCI_OP_VENDOR_WRITE_RAM:
		{
			// if (m_nFirmwareOffset == 0)
			// 	LOG("WRITE_RAM(0x%04x)",	// offset=%d/%d",
			// 		pCommandComplete->command_opcode,
			// 		m_nFirmwareOffset,
			// 		sizeof(Firmware));
			assert(context->m_nFirmwareOffset+3 <= sizeof(Firmware));
		
			u16 nopcode  = Firmware[context->m_nFirmwareOffset++];
				nopcode |= Firmware[context->m_nFirmwareOffset++] << 8;
			u8 len = Firmware[context->m_nFirmwareOffset++];

			hci_vendor_command cmd;
			cmd.hdr.opcode = nopcode;
			cmd.hdr.length = len;
			for (unsigned i=0; i<len; i++)
			{
				assert(context->m_nFirmwareOffset < sizeof Firmware);
				cmd.data[i] = Firmware[context->m_nFirmwareOffset++];
			}

			static int counter;
			counter++;
			int percentage = (counter * 100) / 286;
   	        printf("\rFirmware Loading... %d%%", percentage);
	        fflush(stdout);
			hci_send_command(context, &cmd, sizeof cmd.hdr + len);
			return true;
		}

		case HCI_OP_VENDOR_LAUNCH_RAM:
#if 0
			LOG("setup complete",0);
			CScheduler::Get()->MsSleep(250);
			m_is_setup = true;
			m_pHCIBase->setup();
			return true;
#endif
			context->vendor_m_is_setup = true;
			usleep(250000); // FIXME: this is needed by the BT chip
			hci_base_setup(context);
			return true;
	}

	return false;
}

void processEvent(hci_context_t *context, const void *buffer, unsigned length)
//void processEvent(const void *buffer, unsigned length)
{
	if(length < sizeof(hci_event_header))
		return;

	assert(length >= sizeof(hci_event_header));
	hci_event_header *pHeader = (hci_event_header *) buffer;

	log_info("-->  %s", getBTEventName(pHeader->event_code));

	switch (pHeader->event_code)
	{
		case HCI_EVENT_TYPE_COMMAND_STATUS:
		{
			assert(length >= sizeof(hci_command_status_event));

			hci_command_status_event *pCommandstatus = (hci_command_status_event *) pHeader;
			context->m_can_send_command = pCommandstatus->num_command_packets;
			log_indented(8, "m_can_send_command=%d", pCommandstatus->num_command_packets);
			break;
		}
			
		//---------------------------------------------------------
		// COMMAND_COMPLETE on RESET drives a state machine
		//---------------------------------------------------------
		
		case HCI_EVENT_TYPE_COMMAND_COMPLETE:
		{
			assert(length >= sizeof(hci_command_complete_event));
			hci_command_complete_event *pCommandComplete =(hci_command_complete_event *) pHeader;
			context->m_can_send_command = pCommandComplete->num_command_packets;

			if (pCommandComplete->status != BT_STATUS_SUCCESS)
			{
				log_indented(8, "Command 0x%X failed (status 0x%X) %c",
					(unsigned) pCommandComplete->command_opcode,
					(unsigned) pCommandComplete->status,
					pCommandComplete->status);
				return;
			}

			// a completed COMMAND_RESET takes two different paths
			// depending on if we are using the onboard BT module,
			// in which case we have to upload the firmware ..
			
			switch (pCommandComplete->command_opcode)
			{
				case HCI_OP_BASEBAND_RESET:
#if 0
					if (m_pVendor)
						m_pVendor->setup();
					else
						m_HCIBase.setup();
#else
					//hciVendor_setup(context);
					hci_vendor_setup(context);
#endif
					break;
				
				default:	
				if( context->vendor_m_is_setup == false )				
					m_pVendor_processCommandCompleteEvent(context, buffer,length);
				else if (context->base_m_is_setup == false) {
					log_indented(8, "Command_opcode: 0x%X length:%d\n", pCommandComplete->command_opcode, length);
					hcibase_processCommandCompleteEvent(context, buffer,length);
				}
				else {
					log_indented(8, "Command_opcode: 0x%X\n", pCommandComplete->command_opcode);
					log_indented(8, "Command 0x%X (status 0x%X) %c",
						(unsigned) pCommandComplete->command_opcode, (unsigned) pCommandComplete->status, pCommandComplete->status);
				}
#if 0
					if (m_pVendor && !m_pVendor->isSetup())
						m_pVendor->processCommandCompleteEvent(buffer,length);
					else if (!m_HCIBase.isSetup())
						m_HCIBase.processCommandCompleteEvent(buffer,length);
#endif
					break;
			}
			break;
			
		}	// HCI_EVENT_TYPE_COMMAND_COMPLETE
		
				
		//----------------------------------------------
		// the bulk of HCI events go here
		//----------------------------------------------
		

		case HCI_EVENT_TYPE_INQUIRY_RESULT:
		{

			log_indented(8, "len=%d",length);
			assert(length >= sizeof(hci_inquiry_result_event));
			hci_inquiry_result_event *pEvent = (hci_inquiry_result_event *) pHeader;
			
			for (unsigned i = 0; i < pEvent->num_responses; i++)
			{;
				u8 addr[BT_ADDR_SIZE];
				u8 cls[BT_CLASS_SIZE];
				memcpy(addr, INQUIRY_RESP_BD_ADDR(pEvent, i), BT_ADDR_SIZE);
				memcpy(cls, INQUIRY_RESP_CLASS_OF_DEVICE(pEvent, i), BT_CLASS_SIZE);
				
				hciRemoteDevice *device = findDeviceByAddr(context, addr);
				log_indented(8, "inquiry %s %s class(%s) pmode(%d) %s",
					device ? "update" : "new",
					addrToString(addr),
					deviceClassToString(cls),
					INQUIRY_RESP_PAGE_SCAN_REP_MODE(pEvent, i),
					device ? device->name : "(unknown)");
				if (!device)
				{
					device = addDevice(context, addr,0);
					if (!device) return;
				}
				memcpy(device->addr,addr,BT_ADDR_SIZE);
				memcpy(device->device_class,cls,BT_CLASS_SIZE);
				
				#if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
					device->page_rep_mode = INQUIRY_RESP_PAGE_SCAN_REP_MODE(pEvent, i);					
				#endif
				
				if (!checkDeviceName(context, device))
					context->m_num_name_requests++;

				receiveEvent(context, HCC_INQUIRY_DEVICE_FOUND, device);
			}
			break;
		}

		case HCI_EVENT_TYPE_INQUIRY_COMPLETE:
		{
			assert(length >= sizeof(hci_inquiry_complete_event));
			hci_inquiry_complete_event *pEvent = (hci_inquiry_complete_event *) pHeader;
			log_indented(8, "%s %s",
				pEvent->status == BT_STATUS_SUCCESS ? "OK" : "ERROR",
				pEvent->status == BT_STATUS_SUCCESS ? "" :
				getBTErrorString(pEvent->status));
			if (!context->m_num_name_requests)
				receiveEvent(context, HCC_INQUIRY_COMPLETE,0);

			context->inquiry_complete = true;
			break;
		}

		case HCI_EVENT_TYPE_REMOTE_NAME_REQUEST_COMPLETE:
		{
			hci_remote_name_complete_event *pEvent = (hci_remote_name_complete_event *) pHeader;		
			
			log_indented(8, "%s %s %s",
				addrToString(pEvent->addr),
				pEvent->status == BT_STATUS_SUCCESS ? "OK" : "ERROR",
				pEvent->status == BT_STATUS_SUCCESS ? (char *) pEvent->remote_name : getBTErrorString(pEvent->status));
				
			if (pEvent->status == BT_STATUS_SUCCESS)
			{
				hciRemoteDevice *device = findDeviceByAddr(context, pEvent->addr);
				if (!device)
				{
					log_error("unknown device in name complete event!");
					return;
				}
				memcpy(device->name,pEvent->remote_name,BT_NAME_SIZE);
				receiveEvent(context, HCC_INQUIRY_NAME_FOUND,device);
			}
			
			if (context->m_num_name_requests)
			{
				context->m_num_name_requests--;
				if (!context->m_num_name_requests)
					receiveEvent(context, HCC_INQUIRY_COMPLETE,0);
			}
			
			break;			
		}

	
		//-----------------------
		// prh my additions
		//-----------------------

		case HCI_EVENT_TYPE_CONNECTION_REQUEST:
		{
			hci_connection_request_event *pEvent = (hci_connection_request_event *) pHeader;		
			log_indented(8, "%s packet_type(0x%04x) pr_mode(0x%02x) p_mode(0x%02x) clock(0x%04x) switch(0x%04x)",
				addrToString(pEvent->addr),
				pEvent->packet_type,
				pEvent->page_rep_mode,
				pEvent->page_mode,
				pEvent->clock_offset,
				pEvent->switch_flag);

			hciRemoteDevice *device = findDeviceByAddr(context, pEvent->addr);
			if (!device)
			{
				device = addDevice(context, pEvent->addr,0);
				if (!device) return;
			}

			#if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
				device->packet_type     = pEvent->packet_type;
				device->page_rep_mode   = pEvent->page_rep_mode;
				device->page_mode		= pEvent->page_mode;
				device->clock_offset    = pEvent->clock_offset;
			#endif
			
			// if the device already has an open handle,
			// we will obviate it and start a new one here
			
			if (device->handle)
			{
				log_indented(8, "Warning: handle(0x%04x) for %s already existed",
					device->handle,
					addrToString(device->addr));
					receiveEvent(context, HCC_EVENT_DISCONNECTED,device);
				device->handle = 0;
			}

			// assign a new handle			
			device->handle = context->m_next_hci_handle++;
			checkDeviceName(context, device);
			receiveEvent(context, HCC_EVENT_CONNECTED,device);
			
			// send a connection accepted command


			hci_accept_connection_command cmd;
			cmd.header.opcode = HCI_OP_LINK_ACCEPT_CONNECTION_REQUEST;
			memcpy(cmd.addr, pEvent->addr, BT_ADDR_SIZE);
			cmd.role = 1;
			log_info("<-- HCI - ACCEPT_CONNECTION_REQUEST");
			//sendCommand(&cmd, sizeof(cmd));
			hci_send_command(context, &cmd, sizeof cmd);
			break;
		}
		
		case HCI_EVENT_TYPE_CONNECTION_COMPLETE:
		{
			hci_connection_complete_event *pEvent = (hci_connection_complete_event *) pHeader;

			log_indented(8, "Result: %s %s %s  handle=0x%04x",
				pEvent->status == BT_STATUS_SUCCESS ? "OK" : "ERROR",
				pEvent->status == BT_STATUS_SUCCESS ? "" : getBTErrorString(pEvent->status),
				addrToString(pEvent->addr),
				pEvent->handle);

			hciRemoteDevice *device = findDeviceByAddr(context, pEvent->addr);

			if (pEvent->status == BT_STATUS_SUCCESS)
			{
				if (!device)
				{
					device = addDevice(context, pEvent->addr, 0);
					if (!device) return;
				}
				device->handle 	  = pEvent->handle;
				#if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
					device->link_type = pEvent->link_type;
					device->encrypt   = pEvent->encrypt;
				#endif

				// FIXME: find another way of checking if device is already paired.
				if(device->link_key_type == 0) {
					log_info("<--  HCI: HCI_OP_LINK_READ_REMOTE_SUPPORTED_FEATURES");
					hci_read_remote_supported_features_command cmd;
					cmd.handle = pEvent->handle;
					cmd.header.opcode = HCI_OP_LINK_READ_REMOTE_SUPPORTED_FEATURES;
					hci_send_command(context, &cmd, sizeof cmd);
				} else {
					receiveEvent(context, HCC_EVENT_CONNECTED,device);
				}
			}
			else
			{
				receiveEvent(context, HCC_EVENT_CONNECTION_ERROR,device);
				if (device)
					device->handle = 0;
			}
			checkDeviceName(context, device);

			break;
		}

		case HCI_EVENT_TYPE_READ_SUPPORTED_FEATURES_COMPLETE:
		{
			hci_read_remote_supported_features_event *pEvent = (hci_read_remote_supported_features_event *) pHeader;

			log_indented(8, "handle(0x%04x) %s %s",
				pEvent->handle,
				pEvent->status ? "ERROR: ": "OK",
				pEvent->status ? getBTErrorString(pEvent->status) : "");

//FIXME: you need to parse the features
			log_indented(8, "Remote Supported Feature: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", 
				pEvent->features[0], pEvent->features[1], pEvent->features[2], pEvent->features[3], 
				pEvent->features[4], pEvent->features[5], pEvent->features[6], pEvent->features[7]);


			hci_read_remote_extended_features_command cmd;
			cmd.handle = pEvent->handle;
			cmd.page_number = 1; // Page number to retrieve the extended features (page 0 is the default set of features)
			cmd.header.opcode = HCI_OP_LINK_READ_REMOTE_EXTENDED_FEATURES;

			log_info("<--  HCI: HCI_OP_LINK_READ_REMOTE_EXTENDED_FEATURES");
			//sendCommand(&cmd, sizeof(cmd));
			hci_send_command(context, &cmd, sizeof cmd);
		}
		break;

		case HCI_EVENT_READ_RMT_EXT_FEATURES_COMP_EVT:
		{
			hci_read_remote_extended_features_event *pEvent = (hci_read_remote_extended_features_event *) pHeader;
			log_indented(8, "handle(0x%04x) %s %s",
				pEvent->handle,
				pEvent->status ? "ERROR: ": "OK",
				pEvent->status ? getBTErrorString(pEvent->status) : "");

//FIXME: you need to parse the features
			log_indented(8, "Remote Ext. Supported Feature: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", 
				pEvent->features[0], pEvent->features[1], pEvent->features[2], pEvent->features[3], 
				pEvent->features[4], pEvent->features[5], pEvent->features[6], pEvent->features[7]);
			
				log_info("<--  HCI: HCI_OP_LINK_AUTHENTICATION_REQUESTED");
				hci_authentication_request_command cmd;
				cmd.handle = pEvent->handle;
				cmd.header.opcode = HCI_OP_LINK_AUTHENTICATION_REQUESTED;

				hci_send_command(context, &cmd, sizeof cmd);
		}
		break;

		case HCI_EVENT_IO_CAPABILITY_REQUEST_EVT:
		{
			hci_io_capability_request_event *pEvent = (hci_io_capability_request_event *) pHeader;
			hci_io_capability_request_reply_cmd cmd;
			
			cmd.header.opcode = HCI_OP_LINK_IO_CAPABILITY_REPLY;
			memcpy(cmd.addr, pEvent->addr, BT_ADDR_SIZE);
			cmd.io_capability = 0x1; // DisplayYesNo (0x01)
			cmd.oob_data_present = 0;
			cmd.auth_req = 0;//4;//0; // No Bonding - MITM not required (0x00)

			hci_send_command(context, &cmd, sizeof cmd);
			break;
		}
		case HCI_EVENT_IO_CAPABILITY_RESPONSE_EVT:
		{
			hci_io_capability_response_event *pEvent = (hci_io_capability_response_event *) pHeader;

			log_info("Address: %s\n", addrToString(pEvent->addr));
	        log_info("IO capability: %d\n", pEvent->io_capability); // NoInputNoOutput (0x03)
	        log_info("OOB data: %d\n", pEvent->oob_data_present);   // Authentication data not present (0x00)
	        log_info("Authentication: %d\n", pEvent->auth_req);      // General Bonding - MITM not required (0x04)
			break;
		}

		case HCI_EVENT_SIMPLE_PAIRING_COMPLETE_EVT:
		{
			// FIXME: create structure to print Address

			break;
		}

		case HCI_EVENT_USER_CONFIRMATION_REQUEST_EVT:
		{
			hci_io_user_confirmation_request_event *pEvent = (hci_io_user_confirmation_request_event *) pHeader;
			log_info("Address: %s\n", addrToString(pEvent->addr));
	        log_info("PassKey: %u\n", pEvent->pass_key); // NoInputNoOutput (0x03)


	        hci_io_user_confirmation_request_reply_cmd cmd;
	        cmd.header.opcode = HCI_OP_LINK_USER_CONFIRMATION_REQUEST_REPLY;
	        memcpy(cmd.addr, pEvent->addr, BT_ADDR_SIZE);
	        hci_send_command(context, &cmd, sizeof cmd);
			break;
		}

		case HCI_EVENT_TYPE_DISCONNECTION_COMPLETE:
		{
			hci_disconnect_complete_event *pEvent = (hci_disconnect_complete_event *) pHeader;		
			log_indented(8, "handle(0x%04x) %s %s %s",
				pEvent->handle,
				pEvent->status ? "ERROR: ": "OK",
				pEvent->status ? getBTErrorString(pEvent->status) : "",
				pEvent->reason ? getBTErrorString(pEvent->reason) : "");
			
			hciRemoteDevice *device = findDeviceByHandle(context, pEvent->handle);
			if (device)
			{
				receiveEvent(context, HCC_EVENT_DISCONNECTED,device);
				device->handle = 0;
			}
			break;
		}

		case HCI_EVENT_TYPE_PIN_CODE_REQUEST:
		{
			hci_pin_code_request_event *pEvent = (hci_pin_code_request_event *) pHeader;
			log_indented(8, "from %s SENDING '%s'", addrToString(pEvent->addr), PIN_CODE);

			hci_pin_code_reply_command cmd;
			cmd.header.opcode = HCI_OP_LINK_PIN_CODE_REQUEST_REPLY;
			cmd.pin_code_len = strlen(PIN_CODE);
			memset(cmd.pin_code, 0, BT_PIN_CODE_SIZE);
			memcpy(cmd.addr, pEvent->addr, BT_ADDR_SIZE);
			memcpy(cmd.pin_code, PIN_CODE, strlen(PIN_CODE));

			//sendCommand(&cmd, sizeof(cmd));
			hci_send_command(context, &cmd, sizeof cmd);
			break;
		}

		case HCI_EVENT_TYPE_AUTHENTICATION_COMPLETE:
		{
			printf("Authentication complete\n");
			hci_authentication_complete_event *pEvent = (hci_authentication_complete_event *) pHeader;
			log_indented(8, "handle(0x%04x) %s%s",
				pEvent->handle,
				pEvent->status ? "ERROR: ": "OK",
				pEvent->status ? getBTErrorString(pEvent->status) : "");
			hciRemoteDevice *device = findDeviceByHandle(context, pEvent->handle);
			assert(device);

			display_bytes("Authentication:", buffer, length);



#if 0  		// FIXME: create structure - HCI_Set_Connection_Encryption
			u8 cmd[] = {0x03, 0x08, 0x03, 0x00, 0x00, 0x01};
		    cmd[3] = pEvent->handle & 0xFF; 		  // Connection Handle LSB
		    cmd[4] = (pEvent->handle >> 8) & 0xFF; // Connection Handle MSB

			hci_send_command(context, &cmd, sizeof cmd);
#endif

#if 0 // FIXME - suppose to delete below
			hci_authentication_request_command cmd;
			cmd.handle = pEvent->handle;
			cmd.header.opcode = HCI_OP_LINK_AUTHENTICATION_REQUESTED;
			hci_send_command(context, &cmd, sizeof cmd);
#endif
			break;
		}

		case HCI_EVENT_TYPE_ENCRYPTION_CHANGE:
		{
			// FIXME: create structe
			log_info("status:%x  Encryption Enable: %x\n", pHeader[0], pHeader[3]);
			break;
		}

		case HCI_EVENT_TYPE_LINK_KEY_NOTIFICATION:
		{
			hci_link_key_notification_event *pEvent = (hci_link_key_notification_event *) pHeader;

			log_indented(8, "%s key_type(0x%02x)",
				addrToString(pEvent->addr),
				((hci_link_key_notification_event *) pHeader)->key_type);


			hciRemoteDevice *device = findDeviceByAddr(context, pEvent->addr);
			assert(device);
			memcpy(device->link_key,pEvent->link_key,BT_LINK_KEY_SIZE);
			
			// unfortunately the "combined" type is 0x00
			// and I want to use the link_key_type to indicate pairing.
			// so I set it to the otherwise unused value of 0x02 if it zero

			printf("\n\n - link_key_type:%d\n\n", pEvent->key_type);
			if (!pEvent->key_type) {
				pEvent->key_type = 0x02;
			}
			device->link_key_type = pEvent->key_type;
			
			saveDevices(context);
			break;
		}

		case HCI_EVENT_TYPE_LINK_KEY_REQUEST :
		{
			// if there is a link_key, as evidenced by device->link_key_type,
			// send a link_key_response, otherwise, reject the request and the
			// remote device will prompt for a pin
			
			hci_link_key_notification_event *pEvent = (hci_link_key_notification_event *) pHeader;
			log_indented(8, "from %s",addrToString(pEvent->addr));
			hciRemoteDevice *device = findDeviceByAddr(context, pEvent->addr);
			assert(device);
			if (device && device->link_key_type)
			{
				hci_link_key_request_reply_command cmd;
				cmd.header.opcode = HCI_OP_LINK_LINK_KEY_REQUEST_REPLY;
				memcpy(cmd.addr, device->addr, BT_ADDR_SIZE);
				memcpy(cmd.link_key, device->link_key, BT_LINK_KEY_SIZE);
				log_info("<--  HCI LINK_KEY_REPLY_COMMAND");
				//sendCommand(&cmd, sizeof cmd);
				hci_send_command(context, &cmd, sizeof cmd);
			}
			else
			{
				hci_link_key_request_reject_command cmd;
				cmd.header.opcode = HCI_OP_LINK_LINK_KEY_REQUEST_NEGATIVE_REPLY;
				memcpy(cmd.addr, pEvent->addr, BT_ADDR_SIZE);
				log_info("<-- HCI LINK_KEY_REJECT_COMMAND");
				//sendCommand(&cmd, sizeof cmd);
				hci_send_command(context, &cmd, sizeof cmd);
			}
			break;
		}
		case HCI_EVENT_TYPE_PAGE_SCAN_REPETITION_MODE_CHANGE:
		{
			hci_page_scan_rep_mode_change_event *pEvent = (hci_page_scan_rep_mode_change_event *) pHeader;
			log_indented(8, "%s mode=0x%02x",
				addrToString(pEvent->addr),
				pEvent->page_rep_mode);
			#if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
				hciRemoteDevice *device = findDeviceByAddr(context, pEvent->addr);
				assert(device);
				if (device)
					device->page_rep_mode = pEvent->page_rep_mode;
			#endif
			break;
		}
		case HCI_EVENT_TYPE_MAX_SLOTS_CHANGE:
		{
			log_indented(8, "handle=0x%04x  slots=0x%02x",
				((hci_max_slots_change_event *) pHeader)->handle,
				((hci_max_slots_change_event *) pHeader)->max_slots);
			#if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
				hciRemoteDevice *device = findDeviceByHandle(context,
					((hci_max_slots_change_event *) pHeader)->handle);
				assert(device);
				device->max_slots = ((hci_max_slots_change_event *) pHeader)->max_slots;
			#endif
			break;
		}

		case HCI_EVENT_LINK_SUPER_TIMEOUT_CHANGED_EVT :
		{
			hci_link_timeout_changed_event *pEvent = (hci_link_timeout_changed_event *) pHeader;
			log_indented(8, "handle=0x%04x timeout=0x%04x",
				pEvent->handle,
				pEvent->timeout);
			#if HCI_DEVICE_INCLUDE_UNUSED_FIELDS
				hciRemoteDevice *device = findDeviceByHandle(context, pEvent->handle);
				assert(device);
				device->timeout = pEvent->timeout;
			#endif
			break;
		}

		case HCI_EVENT_TYPE_NUMBER_OF_COMPLETED_PACKETS:
		{
			// TODO: should implement this to increment per-handle counter
			// for sends ?!?!!

			hci_number_of_completed_packets_event *pEvent = (hci_number_of_completed_packets_event *) pHeader;
			assert(pEvent->num_entries);
			log_indented(8, "num(%d) handle[0]=0x%04x completed[0]=%d",
				pEvent->num_entries,
				pEvent->data[0],
				pEvent->data[pEvent->num_entries]);
			context->m_can_send_data += pEvent->data[pEvent->num_entries];

			if (pEvent->num_entries > 1)
			{
				for (u8 i=1; i<pEvent->num_entries; i++)
				{
					log_indented(8, "handle[%d]=0x%04x completed[%d]=%d",
						i, pEvent->data[i], i, pEvent->data[i+pEvent->num_entries]);
					context->m_can_send_data += pEvent->data[i+pEvent->num_entries];
				}
			}
			break;
		}

		default:
			log_error("unhandled 0x%02x %s len=%d",
				pHeader->event_code,
				getBTEventName(pHeader->event_code),
				pHeader->length);
			break;
			
	}	//  switch(event_code)
}	// 	hciLayer::processEvent()



