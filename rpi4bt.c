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

#include "transport.h"
#include "hci.h"
#include "l2cap.h"

void * pairing(hci_context_t *context) { 
    while(1) {
        // TODO: replace with function call
        if(context->base_m_is_setup == false) {
            sleep(1);
            continue;
        }

        menu(context);
    }
}

#if 1
// FIXME: Remove global variable
hci_context_t *ctx = NULL;
hci_context_t *hci_get_context(void) 
{
    printf("ctx: %p\n", ctx);
    return ctx;
}
#endif

int main(void)
{
    int result = 0;
    
//    result |= transport_init();
    transport_context_t *transport = transport_init(TRANSPORT_UART);
    if (!transport) {
        log_error("Failed to initialize transport\n");
        return -1;
    }



    hci_context_t *hci_ctx = hci_init(transport);
    if (!hci_ctx) {
        // Handle HCI initialization error
        transport_destroy(transport);
        return -1;
    }

ctx = hci_ctx; // FIXME
    
    l2cap_context_t *l2cap_ctx = l2cap_init(hci_ctx);
    if(!l2cap_ctx) {
        log_error("Failed to initialize l2cap\n");
        // FIXME: free resource 
        return -1;        
    }
printf("here - 1\n");
    result = sdp_init(l2cap_ctx);
    if(result) {
        log_error("Failed to initialize SDP\n");
        // FIXME: free resource 
        return -1;        
    }
printf("here - 2\n");
    
    result = rfcomm_init(l2cap_ctx);
    if(result) {
        log_error("Failed to initialize RFCOMM\n");
        // FIXME: free resource 
        return -1;        
    }

    result = hid_init(l2cap_ctx);
    if(result) {
        log_error("Failed to initialize HID\n");
        // FIXME: free resource 
        return -1;        
    }

    client_init_queue(l2cap_ctx);

    pthread_t t3;
    pthread_create(&t3, NULL, pairing, hci_ctx);
    pthread_detach(t3);

    // TODO: We need to know if the initialization is done before calling reset()
    //       Better function name.
    hci_base_reset(hci_ctx);

    setup_resource_manager();

    return 0;
}
