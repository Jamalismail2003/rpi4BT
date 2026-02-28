#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "utils.h"
#include "hci.h"

#define FILE_NAME "/etc/hci_devices.txt"

static void receiveTransport(uint8_t hci_prefix, const void *const_buffer, unsigned length, void *user_data);
static void process_serial_data(hci_context_t *context);
void processEvent(hci_context_t *context, const void *buffer, unsigned length);

int hci_send_hci_command(hci_context_t *context, void *cmd, uint32_t cmd_size) {
    if (!context || !context->transport || !context->transport->send) {
        log_error("Invalid context or transport");
        return -1;
    }

    uint8_t *buffer = (uint8_t *)malloc(cmd_size + 1);
    if (!buffer) {
        log_error("Failed to allocate buffer for HCI command");
        return -1;
    }

    buffer[0] = HCI_PREFIX_CMD; // Add HCI command prefix
    memcpy(&buffer[1], cmd, cmd_size);

    int bytes_sent = context->transport->send(context->transport, buffer, cmd_size + 1);
    free(buffer);

    if (bytes_sent != cmd_size + 1) {
        log_error("Failed to send HCI command");
        return -1;
    }

    return bytes_sent;
}

int hci_send_hci_data(hci_context_t *context, void *data, uint32_t data_size) {
    if (!context || !context->transport || !context->transport->send) {
        log_error("Invalid context or transport");
        return -1;
    }

    uint8_t *buffer = (uint8_t *)malloc(data_size + 1);
    if (!buffer) {
        log_error("Failed to allocate buffer for HCI data");
        return -1;
    }

    buffer[0] = HCI_PREFIX_DATA; // Add HCI data prefix
    memcpy(&buffer[1], data, data_size);

    int bytes_sent = context->transport->send(context->transport, buffer, data_size + 1);
    free(buffer);

    if (bytes_sent != data_size + 1) {
        log_error("Failed to send HCI data");
        return -1;
    }

    return bytes_sent;
}

void hci_vendor_setup(hci_context_t *context) {
    hci_command_header cmd;
    cmd.opcode = HCI_OP_VENDOR_DOWNLOAD_MINIDRIVER;
    cmd.length = 0;
    hci_send_command(context, &cmd, sizeof cmd);
    usleep(50000);
    context->vendor_m_is_setup = false;
}

// Base setup
void hci_base_setup(hci_context_t *context) {
    hci_command_header cmd;
    cmd.opcode = HCI_OP_INFORM_READ_BD_ADDR;
    cmd.length = 0;
    hci_send_command(context, &cmd, sizeof cmd);
    context->base_m_is_setup = false;
}

void hci_base_reset(hci_context_t *context) {
    hci_command_header cmd;
    cmd .opcode = HCI_OP_BASEBAND_RESET;
    cmd.length = 0;
    hci_send_command(context, &cmd, sizeof cmd);
    context->base_m_is_setup = false;
}

// Send HCI command (enqueue)
int hci_send_command(hci_context_t *context, void *cmd, uint32_t cmd_size) {
    hci_command_header *hdr = (hci_command_header *)cmd;
    hdr->length = cmd_size - sizeof(hci_command_header);

    btBuffer *buffer = btBuffer_create(cmd_size);
    memcpy(buffer->buffer, (uint8_t *)cmd, cmd_size);
    buffer->length = cmd_size;

    display_bytes("<--sendCmd - hci>", (uint8_t *)cmd, cmd_size);

    btQueue_enqueueBuffer(&context->m_command_queue, buffer);

    return 0;
}

// Send HCI data (enqueue)
void hci_send_data(hci_context_t *context, void *data, unsigned length) {
    hci_data_header *hdr = (hci_data_header *)data;
    hdr->len = length - sizeof(hci_data_header);

    btBuffer *buffer = btBuffer_create(length);
    memcpy(buffer->buffer, (uint8_t *)data, length);
    buffer->length = length;

    btQueue_enqueueBuffer(&context->m_send_data_queue, buffer);
}

#if 0
int sendCommand(void *cmd, uint32_t cmd_size)
{
    hci_command_header *hdr = (hci_command_header *) cmd;
    hdr->length = cmd_size - sizeof(hci_command_header);

    btBuffer *buffer = btBuffer_create( cmd_size );
    memcpy(buffer->buffer, (uint8_t*)cmd, cmd_size);
    buffer->length = cmd_size;

    display_bytes("<--sendCmd - hci>",(u8 *)cmd, cmd_size);

    btQueue_enqueueBuffer(&myhciLayer->m_command_queue, buffer);

    return 0;
}

void sendData(void *data, unsigned length)
{
    hci_data_header *hdr = (hci_data_header *) data;
    hdr->len = length - sizeof(hci_data_header);

    btBuffer *buffer = btBuffer_create( length );
    memcpy(buffer->buffer, (uint8_t*)data, length);
    buffer->length = length;

//  display_bytes("<--sendData - lcap>",(u8 *)data,length);

    btQueue_enqueueBuffer(&myhciLayer->m_send_data_queue, buffer);
}


int SendHCICommand(void *cmd, uint32_t cmd_size)
{
    uart_send( HCI_PREFIX_CMD );

    uint8_t *ptr = (uint8_t*)cmd;

    for(uint32_t i = 0; i < cmd_size; i++) {
        uart_send( ptr[i] );
    }

    return cmd_size;
}
int SendHCIData(void *cmd, uint32_t cmd_size)
{
    uart_send( HCI_PREFIX_DATA );

    uint8_t *ptr = (uint8_t*)cmd;

    for(uint32_t i = 0; i < cmd_size; i++) {
        uart_send( ptr[i] );
    }

    return cmd_size;
}



void hciVendor_setup(void)
{
    hci_command_header cmd;
    cmd.opcode = HCI_OP_VENDOR_DOWNLOAD_MINIDRIVER;
    cmd.length = 0;
    sendCommand(&cmd, sizeof cmd);
    usleep(50000);
    myhciLayer->vendor_m_is_setup = false;
}

void hci_base_setup(void)
{
    hci_command_header cmd;
    cmd .opcode = HCI_OP_INFORM_READ_BD_ADDR;
    cmd.length = 0;
    sendCommand(&cmd, sizeof cmd);
    myhciLayer->base_m_is_setup = false;
}

void hci_base_reset(void)
{
    hci_command_header cmd;
    cmd .opcode = HCI_OP_BASEBAND_RESET;
    cmd.length = 0;
    sendCommand(&cmd, sizeof cmd);
    myhciLayer->base_m_is_setup = false;
}
#endif
// Process serial data
static void process_serial_data(hci_context_t *context) {
    pthread_setname_np(pthread_self(), "BT Cmd/Events/Data Handler");

    // TODO: use transport instead direct call of uart API
//    uart_reset();

    while (1)
    {
        btBuffer *pEntry = NULL;

        // Process incoming Events queue
        while ((pEntry = btQueue_dequeue(&context->m_event_queue))) {
            processEvent(context, pEntry->buffer, pEntry->length);
            btBuffer_destroy(pEntry);
        }

        // Process Received Data
        while ((pEntry = btQueue_dequeue(&context->m_recv_data_queue))) {
            if (context->data_callback) {
                context->data_callback(context, pEntry->buffer, pEntry->length);
                btBuffer_destroy(pEntry);
            } else {
                log_warning("Warning: callback function is not registered");
            }
        }

        // Process outging commands queues
        while (context->m_can_send_command && (pEntry = btQueue_dequeue(&context->m_command_queue)))
        {
            //if (!SendHCICommand(pEntry->buffer, pEntry->length)) {
            if (hci_send_hci_command(context, pEntry->buffer, pEntry->length) < 0) {
                log_error("HCI: Could not send HCI command\n");
                break;
            }
            context->m_can_send_command--;
            btBuffer_destroy( pEntry );
        }

        // Process outgoing data queue
        while (context->m_can_send_data && (pEntry = btQueue_dequeue(&context->m_send_data_queue))) {
            if (hci_send_hci_data(context, pEntry->buffer, pEntry->length) < 0) {
            //if (!SendHCIData(pEntry->buffer, pEntry->length)) {
                log_error("HCI: Could not send HCI command\n");
                break;
            }
            context->m_can_send_data--;
            btBuffer_destroy(pEntry);
        }

        usleep(10000);
    }
}


void  hciLayer_closeConnection(hci_context_t *context, hciRemoteDevice *device)
{
    log_indented(8, "closeConnection(%s) handle=0x%04x\n",addrToString(device->addr),device->handle);
    hci_disconnection_request cmd;
    cmd.header.opcode = HCI_OP_LINK_DISCONNECT;
    cmd.disconnect_handle = device->handle;
    cmd.reason        = 0x16;       // 0x16 == Connection Terminated by Local Host
    //sendCommand(&cmd, sizeof(cmd));
    hci_send_command(context, &cmd, sizeof(cmd));
}


hciRemoteDevice *hciLayer_startConnection(hci_context_t *context, uint8_t *addr)
{
    log_indented(8, "HCI: hciLayer::startConnection(%s)\n",addrToString(addr));
    
    // see if there's an existing device
    
    hciRemoteDevice *device = findDeviceByAddr(context, addr);

    if (device && device->handle)
    {
        // TODO
        // assert(m_pClient);
        if (device->handle & HCI_HANDLE_CONNECTING)
        {
            log_warning("HCI: WARNING - device %s already has a pending hci connection\n",
                addrToString(device->addr));
            return device;
        }
        if (device->handle & HCI_HANDLE_ERROR)
        {
            log_warning("HCI: WARNING - restarting connection to errored device %s\n",
                addrToString(device->addr));
            // fall thru
        }
        else
        {
            log_warning("HCI: WARNING - device %s already has hci_handle=0x%04x\n",
                addrToString(device->addr),
                device->handle);
            // TODO
            // m_pClient->receiveEvent(HCC_EVENT_CONNECTED,device);
            return device;
        }
    }

    // add a new device if needed
    
    if (!device)
        device = addDevice(context, addr,0);
    device->handle = HCI_HANDLE_CONNECTING;
    
    // send the connnection request
    
    hci_create_connection_command cmd;
    cmd.header.opcode = HCI_OP_LINK_CREATE_CONNECTION;
    memcpy(cmd.addr,addr,BT_ADDR_SIZE);
    cmd.packet_type   = 0x0008;
    cmd.page_rep_mode = 0x01;
    cmd.page_mode     = 0x00;
    cmd.clock_offset  = 0x0000;
    cmd.switch_roles  = 0x01;
    //sendCommand(&cmd, sizeof(cmd));

// FIXME
//hci_context_t *context = NULL;
    hci_send_command(context, &cmd, sizeof(cmd));

    return device;
}

char *linkKeyToString(u8 *link_key)
{
    static char buf[2 * BT_LINK_KEY_SIZE + 1];
    char *p = buf;
    for (u8 i=0; i<BT_LINK_KEY_SIZE; i++)
    {
        u8 n1 = link_key[i];
        u8 n0 = n1 >> 4;
        n1 &= 0xf;
        *p++ = (n0 > 9) ? 'A' + n0-10 : '0' + n0;
        *p++ = (n1 > 9) ? 'A' + n1-10 : '0' + n1;
    }
    return buf;
}

static void receiveTransport(uint8_t hci_prefix, const void *const_buffer, unsigned length, void *user_data) {
    hci_context_t *context = (hci_context_t *)user_data;
    static uint16_t m_length = 0;
    static uint16_t m_offset = 0;
    static uint8_t m_packet_prefix = 0;
    static btBuffer *m_pBuffer = NULL;

    uint8_t *buffer = (uint8_t *)const_buffer;

    assert(buffer != 0);
    assert(length > 0);

    if (m_offset == 0) {
        m_packet_prefix = hci_prefix;
        if (m_packet_prefix == HCI_PREFIX_DATA) {
            if (length < 4) {
                printf("Short ACL packet ignored\n");
                return;
            }
            m_length = (*(uint16_t *)&buffer[2]) + 4;
        } else if (length < sizeof(hci_event_header)) {
            printf("Short Event Packet ignored\n");
            return;
        } else {
            m_length = buffer[1] + 2;
        }
        assert(m_pBuffer == 0);
        m_pBuffer = btBuffer_create(m_length);
    } else {
        assert(hci_prefix == m_packet_prefix);
    }

    memcpy(m_pBuffer->buffer + m_offset, buffer, length);
    m_offset += length;
    if (m_offset < m_length)
        return;

    if (m_packet_prefix == HCI_PREFIX_DATA) {
        btQueue_enqueueBuffer(&context->m_recv_data_queue, m_pBuffer);
    }
    else {
        btQueue_enqueueBuffer(&context->m_event_queue, m_pBuffer);
    }

    m_pBuffer = 0;
    m_length = 0;
    m_offset = 0;
}

void loadDevices(hci_context_t *context)
{
    FILE *fp = fopen(FILE_NAME, "a+");
    if( fp != NULL ) {
        
        #define MAX_LINE    255

        char buf[MAX_LINE];
        char *line = fgets(buf,MAX_LINE,fp);
        while (line)
        {           
            char *addr_str = line;
            char *p = &line[3*BT_ADDR_SIZE + 1];
            const char *name = (const char *) p;
            
            
            while (*p != '"') p++;  // better be one!
            *p++ = 0;               // set the closing quote to a zero
            
            hciRemoteDevice *device = addDevice(context, strToBtAddr(addr_str), name );
            assert(device);
            
            p++;                    // skip the comma
            u8 n1 = *p++;
            u8 n2 = *p++;
            device->link_key_type =
                (n2 >= 'A' ? 10+n2-'A' : n2-'0') +
                ((n1 >= 'A' ? 10+n1-'A' : n1-'0') << 4);

            p++;                    // skip the comma
            for (u8 i=0; i<BT_LINK_KEY_SIZE; i++)
            {
                u8 n1 = *p++;
                u8 n2 = *p++;
                device->link_key[i] = 
                    (n2 >= 'A' ? 10+n2-'A' : n2-'0') +
                    ((n1 >= 'A' ? 10+n1-'A' : n1-'0') << 4);
            }

            line = fgets(buf,MAX_LINE,fp);
        }
        fclose(fp);
    }
    else
    {
        log_error("HCI: warning: could not open %s",FILE_NAME);
    }
}


void saveDevices(hci_context_t *context)
{   
    FILE *fp = fopen(FILE_NAME, "a+");
    if (fp != NULL) {
        hciRemoteDevice *device = context->m_first_device;

        while (device)
        {
            if (device->link_key_type)
            {
                int ok = fprintf(fp,"%s,\"%s\",%02x,%s\n",
                    addrToString(device->addr),
                    device->name,
                    device->link_key_type,
                    linkKeyToString(device->link_key));
                if (!ok)
                {
                    log_error("HCI: Could not write device to %s ...\n", "/stage/hci_devices.txt");
                }
            }
            device = device->next;
        }
        fclose(fp);
    }
    else
    {
        log_error("HCI: Could not open %s for writing!\n", "/stage/hci_devices.txt");
    }
}



//----------------------
#if 0
hci_context_t *hci_init(transport_context_t *transport) {

    hci_context_t *context = (hci_context_t *)malloc(sizeof(hci_context_t));
    if (!context) {
        log_error("Failed to allocate hci_context");
        return NULL;
    }

    myhciLayer->device_handle = 0;
    myhciLayer->data_callback = NULL;
    myhciLayer->user_data = NULL;


    myhciLayer->m_num_devices = 0;
    myhciLayer->m_first_device = NULL;
    myhciLayer->m_last_device = NULL;
    myhciLayer->base_m_is_setup = false;
    myhciLayer->vendor_m_is_setup = false;
    myhciLayer->m_can_send_command = 1;
    myhciLayer->m_can_send_data = 1; // TODO: set to 5, to send 5 consecutive packets
    myhciLayer->m_device_class = 0x7a020c;//0x8c2104;
    myhciLayer->m_num_name_requests = 0;
    myhciLayer->m_nFirmwareOffset = 0;

    //myhciLayer->m_pBuffer = NULL;


    myhciLayer->m_event_queue.m_pFirst = myhciLayer->m_event_queue.m_pLast = NULL;
    myhciLayer->m_command_queue.m_pFirst = myhciLayer->m_command_queue.m_pLast = NULL;
    myhciLayer->m_send_data_queue.m_pFirst = myhciLayer->m_send_data_queue.m_pLast = NULL;
    myhciLayer->m_recv_data_queue.m_pFirst = myhciLayer->m_recv_data_queue.m_pLast = NULL;

    myhciLayer->m_next_hci_handle = 0;

    loadDevices();

    pthread_t t2;
    pthread_create(&t2, NULL, process_serial_data, NULL);
    pthread_detach(t2);

    return context;
}
#else
hci_context_t *hci_init(transport_context_t *transport) {
    hci_context_t *context = (hci_context_t *)malloc(sizeof(hci_context_t));
    if (!context) {
        log_error("Failed to allocate hci_context");
        return NULL;
    }

    context->device_handle = 0;
    context->data_callback = NULL;
    context->user_data = NULL;
    context->m_num_devices = 0;
    context->m_first_device = NULL;
    context->m_last_device = NULL;
    context->base_m_is_setup = false;
    context->vendor_m_is_setup = false;
    context->inquiry_complete = false;
    context->m_can_send_command = 1;
    context->m_can_send_data = 1; // Set appropriately
    context->m_device_class = 0x7a020c;
    context->m_num_name_requests = 0;
    context->m_nFirmwareOffset = 0;

    btQueue_init(&context->m_event_queue);
    btQueue_init(&context->m_command_queue);
    btQueue_init(&context->m_send_data_queue);
    btQueue_init(&context->m_recv_data_queue);

    context->m_next_hci_handle = 0;
    context->transport = transport;

    // Set the receive callback in the transport context
    context->transport->receive_callback = receiveTransport;
    context->transport->receive_user_data = context;

    loadDevices(context);

    pthread_t thread;
    pthread_create(&thread, NULL, (void *(*)(void *))process_serial_data, context);
    pthread_detach(thread);



    return context;
}
#endif

int hci_register_data_callback(hci_context_t *context, hci_data_callback_t callback, void *user_data) {
    context->data_callback = callback;
    context->user_data = user_data;
    return 0;
}

void hci_receive_data(hci_context_t *context, uint8_t *data, size_t length) {
    if (context->data_callback) {
        context->data_callback(context, data, length);
    }
}

