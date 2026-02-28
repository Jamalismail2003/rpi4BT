#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "transport.h"
#include "transport_uart.h"
#include "utils.h"
#include "hci.h"
#include "hci_defs.h"


enum enumUartState {
    RxStateStart,
    RxStateDataHeader,
    RxStateEventHeader,
    RxStateEventLength,
    RxStateContent,
    RxStateUnknown
};

// Receive byte from UART
void Receive(transport_context_t *transport, uint8_t nChar) {

    switch (transport->m_nRxState) {
        case RxStateStart:
            transport->m_prefix_byte = nChar;
            transport->m_nRxInPtr = 0;
            transport->m_nRxParamLength = 0;
            if (transport->m_prefix_byte == HCI_PREFIX_EVENT)
                transport->m_nRxState = RxStateEventHeader;
            else if (transport->m_prefix_byte == HCI_PREFIX_DATA)
                transport->m_nRxState = RxStateDataHeader;
            break;

        case RxStateDataHeader:
            transport->m_RxBuffer[transport->m_nRxInPtr++] = nChar;
            if (transport->m_nRxInPtr == 4) {
                transport->m_nRxParamLength = *(uint16_t *)&transport->m_RxBuffer[2];
                if (transport->m_nRxParamLength == 0) {
                    if (transport->receive_callback) {
                        transport->receive_callback(transport->m_prefix_byte, transport->m_RxBuffer, transport->m_nRxInPtr, transport->receive_user_data);
                    }
                    transport->m_nRxState = RxStateStart;
                } else
                    transport->m_nRxState = RxStateContent;
            }
            break;

        case RxStateEventHeader:
            transport->m_RxBuffer[transport->m_nRxInPtr++] = nChar;
            transport->m_nRxState = RxStateEventLength;
            break;

        case RxStateEventLength:
            transport->m_RxBuffer[transport->m_nRxInPtr++] = nChar;
            if (nChar > 0) {
                transport->m_nRxParamLength = nChar;
                transport->m_nRxState = RxStateContent;
            } else {
                if (transport->receive_callback) {
                    transport->receive_callback(transport->m_prefix_byte, transport->m_RxBuffer, transport->m_nRxInPtr, transport->receive_user_data);
                }
                transport->m_nRxState = RxStateStart;
            }
            break;

        case RxStateContent:
            assert(transport->m_nRxInPtr < BUFFER_SIZE);
            transport->m_RxBuffer[transport->m_nRxInPtr++] = nChar;
            if (--transport->m_nRxParamLength == 0) {
                if (transport->receive_callback) {
                    transport->receive_callback(transport->m_prefix_byte, transport->m_RxBuffer, transport->m_nRxInPtr, transport->receive_user_data);
                }
                transport->m_nRxState = RxStateStart;
            }
            break;

        default:
            assert(0);
            break;
    }
}

// UART-specific send function
static int uart_transport_send(transport_context_t *transport, const uint8_t *data, size_t length) 
{
#if 0
    for (size_t i = 0; i < length; i++) {
        uart_send(data[i]);
    }
#else
    serial_send(transport, data, length);
#endif

    return length;
}

// UART-specific initialization
static int uart_transport_init(transport_context_t *transport) {
#if 0
    return uart_init(transport);
#else
    return serial_init(transport);
#endif
}

// Generic transport initialization function
transport_context_t *transport_init(int transport_type) {
    transport_context_t *transport = (transport_context_t *)malloc(sizeof(transport_context_t));
    if (!transport) {
        log_error("Failed to allocate transport context");
        return NULL;
    }

    transport->transport_type = transport_type;

    transport->m_RxBuffer = (uint8_t *)malloc(BUFFER_SIZE);
    if (!transport->m_RxBuffer) {
        log_error("Error: No memory available in %s", __func__);
        free(transport);
        return NULL;
    }

    transport->m_nRxState = RxStateStart;
    transport->receive_callback = NULL;
    transport->receive_user_data = NULL;

    if (transport_type == TRANSPORT_UART) {
        transport->send = uart_transport_send;
        transport->init = uart_transport_init;
        if (transport->init(transport) != 0) {
            log_error("UART transport initialization failed");
            free(transport->m_RxBuffer);
            free(transport);
            return NULL;
        }
    } else {
        log_error("Error: Invalid transport type");
        free(transport->m_RxBuffer);
        free(transport);
        return NULL;
    }

    return transport;
}

// Destroy transport context
void transport_destroy(transport_context_t *transport) {
    if (transport) {
        if (transport->m_RxBuffer) {
            free(transport->m_RxBuffer);
        }
        free(transport);
    }
}
