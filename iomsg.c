#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <devctl.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/select.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <sys/syspage.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#include "hci.h"
#include "sdp.h"
#include "menu.h"
#include "client.h"
#include <rpi4bt/rpi4bt_msg.h>

#define TIMEOUT_MS 			20000

//void hci_startInquiry(hci_context_t *context, unsigned nSeconds);


//static int list_devices(resmgr_context_t *ctp, hci_context_t *context, custom_msg_t *input_msg, char *resp)
static int list_devices(resmgr_context_t *ctp, io_devctl_t *msg, hci_context_t *hci_ctx)
{

	int nbytes = 0;
	char resp[4096];
	//custom_msg_t *input_msg = _DEVCTL_DATA(msg->i);
	//char *resp = ((char*)input_msg + sizeof(custom_msg_t));
	extern hciRemoteDevice *m_selected_device;

	int dev_num = 0;
	hciRemoteDevice *device = hci_ctx->m_first_device;

/*
	if (device == NULL) {
		return -1;
	}
*/
	while (device)
	{
		char status[] = "not connected";
		int idx = dev_num++;
		if (device->handle)
			strcpy(status, "connected");

		printf("   %s[%d] %s  %-20s %-18s %s\n", device == m_selected_device ? "*" : " ",
			idx, addrToString(device->addr), device->name, status, device->link_key_type ? "PAIRED" : "");

		nbytes += sprintf(resp + nbytes, "   %s[%d] %s  %-20s %-18s %s\n", device == m_selected_device ? "*" : " ",
				idx, addrToString(device->addr), device->name, status, device->link_key_type ? "PAIRED" : "");

		device = device->next;
	}

	resp[nbytes] = '\0';
	if (nbytes != 0)
		nbytes += 1; // for null termination


SETIOV(ctp->iov + 0, &msg->o, sizeof(msg->o) + sizeof(custom_msg_t)); // FIXME: list cmd is not expecting a msg return, only buffer.
SETIOV(ctp->iov + 1, resp, nbytes);
//msg->o.nbytes = nbytes;
int status = MsgReplyv(ctp->rcvid, EOK, ctp->iov, 2);
if (status == -1) {
    printf("== MsgReplyv failed - ctp->rcvid:%ld\n", (long)ctp->rcvid);
}

	return EOK;
}

//static int start_scan(resmgr_context_t *ctp, hci_context_t *hci_ctx, custom_msg_t *input_msg, char *resp)
static int start_scan(resmgr_context_t *ctp, io_devctl_t *msg, hci_context_t *hci_ctx)
{

	hci_startInquiry(hci_ctx, INQUIRY_SECONDS);

	while( !hci_ctx->inquiry_complete ) {
		usleep(50000);
	}
	hci_ctx->inquiry_complete = false;

	return list_devices(ctp, msg, hci_ctx);
}

#if _NTO_VERSION <= 710
static uint64_t systime_ns(void) {
	uint64_t					nsec;
	static struct qtime_entry	*qtp;

	if(!qtp) {
		qtp = SYSPAGE_ENTRY(qtime);
	}

	/*
	 * Loop until we get two time values the same in case an interrupt
	 * comes in while we're reading the value.
	 */
	do {
		nsec = qtp->nsec;
	} while (nsec != qtp->nsec);

	if(qtp->nsec_inc == 0 || nsec == (-(uint64_t)1)) {
		/*
		 * If nsec field is -1, power managment has kicked in.
		 * If nsec_inc field is 0, there is no ticker
		 */
		ClockTime(CLOCK_MONOTONIC, 0, &nsec);
	}
	return nsec;
}
#define clock_gettime_mon_ns systime_ns
#endif // _NTO_VERSION <= 710

#define NSEC2MSEC(ns)	((unsigned)((ns) / 1000000))

unsigned systime_ms(void) {
	return NSEC2MSEC(clock_gettime_mon_ns());
}

//static int pair_device(hci_context_t *hci_ctx, uint8_t *addr, custom_msg_t *input_msg, char *resp)
static int pair_device(resmgr_context_t *ctp, io_devctl_t *msg, hci_context_t *hci_ctx)
{
	int ret = -1;
	int nbytes = 0;
	char resp[64];
	custom_msg_t *input_msg = _DEVCTL_DATA(msg->i);

	bool timeout = false;
	hciRemoteDevice *device = hciLayer_startConnection(hci_ctx, strToBtAddr(input_msg->mac_addr));
	if (device) {
		uint32_t start_time = systime_ms();
		while( (device->handle & HCI_HANDLE_CONNECTING) || 
			   (device->handle == 0) ||
			   (device->link_key_type == 0)) {
			timeout = (systime_ms() - start_time >= TIMEOUT_MS) ? true : false;
			if (timeout) break;
			usleep(50000);
		}
	}

	if (timeout || !device) {
		strcpy(resp, "Pairing failed");
		nbytes = strlen(resp) + 1;
		ret = -1;
	} else {
		// FIXME: it's possible it didn't timeout & valid device but link_key_type is zero... we should failed.
		strcpy(resp, "Pairing succeeded");
		nbytes = strlen(resp) + 1;
		ret = EOK;
	}

SETIOV(ctp->iov + 0, &msg->o, sizeof(msg->o) + sizeof(custom_msg_t)); // FIXME: list cmd is not expecting a msg return, only buffer.
SETIOV(ctp->iov + 1, resp, nbytes);
//msg->o.nbytes = nbytes;
int status = MsgReplyv(ctp->rcvid, EOK, ctp->iov, 2);
if (status == -1) {
    printf("== MsgReplyv failed - ctp->rcvid:%ld\n", (long)ctp->rcvid);
}


	return ret;
}

//static int sdp_device(hci_context_t *hci_ctx, custom_msg_t *input_msg, char *resp)
static int sdp_device(resmgr_context_t *ctp, io_devctl_t *msg, hci_context_t *hci_ctx)
{

	int ret = EOK;
	//int nbytes = 0;
	//char *resp = NULL;
	custom_msg_t *input_msg = _DEVCTL_DATA(msg->i);
	custom_msg_t *output_msg = _DEVCTL_DATA(msg->i);
	

	bool timeout = false;
	sdpRequest *request = sdpLayer_doSdpRequest(hci_ctx->user_data, strToBtAddr(input_msg->mac_addr), input_msg->sdp_request, 0x0000, 0xffff);
	if (request) {
		uint32_t start_time = systime_ms();
		while (!request->parse_complete) {
			timeout = systime_ms() - start_time >= TIMEOUT_MS;
			if (timeout) break;
			usleep(50000);
		}
	}


	//if (timeout || !request->parse_complete || !request->bytes_parsed) {
	if (timeout || !request || !request->output_buf) {
		output_msg->rf_channel_num = -1; // Invalid RF channel number
		ret = -1;
	} else {
		output_msg->rf_channel_num = request->rfcomm_channel;
	}

	
	if (ret) {
		char tmp[64] = "Failed to retrieve SDP";
		SETIOV(ctp->iov + 1, tmp, strlen(tmp) + 1);
		msg->o.ret_val = -1;
	}
	else {
		SETIOV(ctp->iov + 1, request->output_buf, request->output_len);
		msg->o.ret_val = EOK;
	}
	SETIOV(ctp->iov + 0, &msg->o, sizeof(msg->o) + sizeof(custom_msg_t));
	int status = MsgReplyv(ctp->rcvid, EOK, ctp->iov, 2);
	if (status == -1) {
	    printf("== MsgReplyv failed - ctp->rcvid:%ld\n", (long)ctp->rcvid);
	}

	if(request && request->output_buf) {
		free(request->output_buf);
		request->output_len = 0;
	}

	return ret;
}

static int rfcomm_device(resmgr_context_t *ctp, io_devctl_t *msg, hci_context_t *hci_ctx)
{
	custom_msg_t *input_msg = _DEVCTL_DATA(msg->i);
	int ret = bt_client_open(hci_ctx, input_msg->mac_addr, input_msg->rf_channel_num);

	char resp[64] = "Starting CarPlay OK";
	if (ret) {
		strcpy(resp, "Starting CarPlay failed");
	}

	int nbytes = strlen(resp) + 1;

	msg->o.ret_val = ret;
	SETIOV(ctp->iov + 0, &msg->o, sizeof(msg->o) + sizeof(custom_msg_t)); // FIXME: list cmd is not expecting a msg return, only buffer.
	SETIOV(ctp->iov + 1, resp, nbytes);
	int status = MsgReplyv(ctp->rcvid, EOK, ctp->iov, 2);
	if (status == -1) {
	    printf("== MsgReplyv failed - ctp->rcvid:%ld\n", (long)ctp->rcvid);
	}

	return ret;
}


//void devctl_process_command(resmgr_context_t *ctp, custom_msg_t *input_msg, char *resp)
int devctl_process_command(resmgr_context_t *ctp, io_devctl_t *msg)
{
	hci_context_t *hci_ctx = hci_get_context();
	custom_msg_t *input_msg = _DEVCTL_DATA(msg->i);
	int status;


	switch (input_msg->cmd) {
	case CMD_SCAN:
		{
			status = start_scan(ctp, msg, hci_ctx);
			break;
		}
	case CMD_STATUS:
		{
			status = list_devices(ctp, msg, hci_ctx);
			break;
		}
	case CMD_LIST:
		{
			status = list_devices(ctp, msg, hci_ctx);
			break;
		}
	case CMD_PAIR:
		{
			status = pair_device(ctp, msg, hci_ctx);
			break;
		}
	case CMD_SDP:
		{
            status = sdp_device(ctp, msg, hci_ctx);
			break;
		}
	case CMD_CARPLAY:
		{
            status = rfcomm_device(ctp, msg, hci_ctx);
            break;
		}
	default:
		{
			printf("Error: Invalid command %d\n", input_msg->cmd);
			return 0;
		}
	}
	return status;
}
