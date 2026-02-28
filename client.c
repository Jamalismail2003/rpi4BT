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
#include <stdbool.h>

#include "utils.h"
#include "hci.h"
#include "rfcomm.h"
#include "hid.h"
#include "menu.h"
#include "client.h"

static int data_available = 0;  // Condition flag
btQueue client_data_queue;
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;



int bt_client_open(hci_context_t *hci_ctx, const char *mac_addr, int rf_channel) {

    hciRemoteDevice *device = selectDevice_by_addr(mac_addr);
    if (device == NULL) {
    	return -1;
    }

	rfChannel *m_rfChannel = openRFChannel(hci_ctx->user_data, strToBtAddr(mac_addr), (u8)rf_channel);

	if (!m_rfChannel) {
		log_error("Could not openRFChannel()!!\n");
		return 0;
	}
	else {
		log_info("Opening channel(%d)\n",m_rfChannel->channel_num);
	}


#define TIMEOUT_MS 			15000
	bool timeout = false;
	uint32_t start_time;

	start_time = systime_ms();
	while (!(m_rfChannel->channel_state & RF_CHANNEL_STATE_OPEN)) {
		timeout = systime_ms() - start_time >= TIMEOUT_MS;
		if (timeout) break;
		usleep(50000);
		printf("RF open in progress...\n");
	}


	if (!(m_rfChannel->channel_state & RF_CHANNEL_STATE_OPEN)) {
		printf("Error: RF Channel  failed\n");
		return -1;
	}
	printf("RF Channel Open success!!!\n");

    return 0;
}


int bt_client_close() {
    // Close the Bluetooth connection

    printf("TODO: Close the Bluetooth connection\n");
    return 0; // Success
}


int bt_client_write(void *buf, size_t nbytes) {
	rfChannel *avail[5];

log_error("bt_client_write: called...");

	hci_context_t *hci_ctx = hci_get_context();
	int num_avail = getOpenChannels(hci_ctx->user_data, avail, 5);

	if (!num_avail)
	{
		log_error("no open rfcomm channels available for output\n");
		return -1;
	}

	//u8 sync[] = {0xFF, 0x5A, 0x00, 0x1A, 0x80, 0xD9, 0x00, 0x00, 0x34, 0x01, 0x05, 0x08, 0x00, 0x05, 0xDC, 0x00, 0xFA, 0x1E, 0x03, 0x01, 0x00, 0x02, 0x02, 0x01, 0x01, 0xEF};
	
	for (int i=0; i < num_avail; i++)
	{
		rfChannel *channel = avail[i];
#if 0
		rfLayer_sendData(channel, buf, nbytes);
#else
#define MAX_CHUNK_SIZE 127

//	void sendDataInChunks(int channel, const char *buf, int nbytes) {
	    int offset = 0;
	    
	    while (offset < nbytes) {
	        // Calculate how many bytes to send in this chunk
	        int chunk_size = (nbytes - offset > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : nbytes - offset;

	        // Send the chunk
	        rfLayer_sendData(channel, (u8 *)buf + offset, chunk_size);

	        // Move the offset forward
	        offset += chunk_size;
	    }
//	}
#endif
	}
	return nbytes;
}

int bt_client_data_available()
{
	return data_available;
}

int bt_client_read(void *buf, size_t nbytes) 
{
	log_error("bt_client_read: called...");

    pthread_mutex_lock(&client_mutex);
    while (!data_available) {
        pthread_cond_wait(&client_cond, &client_mutex);
    }

	btBuffer *pEntry = btQueue_dequeue(&client_data_queue);
	if(pEntry) { // TODO: check if(nbytes < pEntry.length)
		int read_bytes = pEntry->length;

		log_error("bt_client_read: called... nbytes:%d", read_bytes);

		memcpy(buf, pEntry->buffer, read_bytes);
		btBuffer_destroy(pEntry);

		if(client_data_queue.m_pFirst == NULL) {
			data_available = 0;
		}
		pthread_mutex_unlock(&client_mutex);	
		return read_bytes;
	}

	pthread_mutex_unlock(&client_mutex);	

	log_error("bt_client_read: called... return nbytes:0");

	return 0;
}

void rf_client_callback(rfChannel *channel, const u8 *data, u16 length)
{
    log_info("Received data from RFCOMM - length:%d\n", length);
    display_bytes("> RFCOMM", data, length);

	pthread_mutex_lock(&client_mutex);	

	btBuffer *buf = btBuffer_create( length );
	memcpy(buf->buffer, (uint8_t*)data, length);
	buf->length = length;

	btQueue_enqueueBuffer(&client_data_queue, buf);

	data_available = 1;
    pthread_cond_signal(&client_cond);  // Signal the read thread that data is available
    
    pthread_mutex_unlock(&client_mutex);	
}


// Define buffer size
#define NUM_REPORT 40
#define REPORT_SIZE 12 // Simulating fixed-size HID reports

// Circular buffer structure
typedef struct {
    uint8_t buffer[NUM_REPORT][REPORT_SIZE]; // Circular buffer
    int head;                                // Write pointer
    int tail;                                // Read pointer
    bool full;                               // Flag to indicate buffer full
} CircularBuffer;

CircularBuffer *cb;

// Initialize the circular buffer
void circular_buffer_init(void) {
    cb->head = 0;
    cb->tail = 0;
    cb->full = false;
}

// Check if the buffer is full
bool circular_buffer_full(void) {
    return cb->full;
}

// Check if the buffer is empty
bool circular_buffer_empty(void) {
    return (!cb->full && (cb->head == cb->tail));
}

// Write data to the circular buffer
bool circular_buffer_write(const uint8_t *data) {
    if (circular_buffer_full()) {
        printf("Buffer full! Dropping data.\n");
        return false; // Cannot write, buffer full
    }

    // Write the report to the buffer
printf("cb->head:%d\n", cb->head);
    memcpy(cb->buffer[cb->head], data, REPORT_SIZE);

    // Advance the head pointer
    cb->head = (cb->head + 1) % NUM_REPORT;

    // Check if the buffer is now full
    if (cb->head == cb->tail) {
        cb->full = true;
    }

    return true;
}

// Read data from the circular buffer
int circular_buffer_read(uint8_t *data) {
    if (circular_buffer_empty()) {
        //printf("Buffer empty! No data to read.\n");
        return 0; // Cannot read, buffer empty
    }

    // Read the report from the buffer
    memcpy(data, cb->buffer[cb->tail], REPORT_SIZE);

    // Advance the tail pointer
    cb->tail = (cb->tail + 1) % NUM_REPORT;

    // Mark buffer as not full
    cb->full = false;

    return REPORT_SIZE;
}

void hid_client_callback(const u8 *data, u16 length)
{
    log_info("hid_client_callback...\n");
    display_bytes("> HID Event:", data, length);
    
    circular_buffer_write(data);

    //FIXME: put data into circular buffer & set cond flag
}

void client_init_queue(l2cap_context_t *l2cap_ctx)
{
	client_data_queue.m_pFirst = client_data_queue.m_pLast = NULL;	
	data_available = 0;
	rf_register_client(l2cap_ctx, rf_client_callback);	

	cb = (CircularBuffer*) malloc(sizeof(CircularBuffer));
	//cb->buffer = malloc(BUFFER_SIZE * REPORT_SIZE * 1024);

    circular_buffer_init();

	hid_register_client(l2cap_ctx, hid_client_callback);
}

