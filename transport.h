#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

typedef struct transport_context_t transport_context_t;

// Function pointer types for transport operations
typedef int (*transport_send_func_t)(transport_context_t *transport, const uint8_t *data, size_t length);
typedef int (*transport_init_func_t)(transport_context_t *transport);
typedef void (*transport_receive_callback_t)(uint8_t hci_prefix, const void *buffer, unsigned length, void *user_data);

// Transport context structure
struct transport_context_t {
    int transport_type; // e.g., 0 for USB, 1 for UART

    // Receive state variables
    unsigned m_nRxState;
    unsigned m_nRxParamLength;
    unsigned m_nRxInPtr;
    uint8_t m_prefix_byte;
    uint8_t *m_RxBuffer;

    // Function pointers for transport operations
    transport_send_func_t send;
    transport_init_func_t init;

    // Receive callback and user data
    transport_receive_callback_t receive_callback;
    void *receive_user_data;
    void *serial_ctx; // For serial transport specific context
};

#define BUFFER_SIZE  0xFFFF

transport_context_t *transport_init(int transport_type);
void transport_destroy(transport_context_t *transport);
void Receive(transport_context_t *transport, uint8_t nChar);
int serial_send(transport_context_t *transport, const uint8_t *data, size_t length);
int serial_init(transport_context_t *transport);

enum {
    TRANSPORT_USB,
    TRANSPORT_UART
};

#endif // TRANSPORT_H
