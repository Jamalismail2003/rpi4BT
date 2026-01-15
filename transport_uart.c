#include <stdio.h>
#include <stdlib.h>
#include <sys/siginfo.h>
#include <sys/mman.h>
#include <sys/procmgr.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/neutrino.h>

#include "transport_uart.h"
#include "transport.h"
#include "utils.h"

#define REGS_GPIO_BASE  (PBASE + 0x00200000)
#define REGS_AUX_BASE   (PBASE + 0x00215000)
#define REGS_UART_BASE  (PBASE + 0x00201000) // For UART0

// UART registers
struct AuxRegs  *REGS_AUX;
struct GpioRegs *REGS_GPIO;
struct UartRegs *REGS_UART;

volatile struct sigevent int_event; // Event to wake up the thread

// Forward declarations
static void *BluetoothSerialReader(void *arg);
static void serial_ISR(transport_context_t *transport);
static const struct sigevent *interrupt_handler(void *arg, int id);
//static void gpio_pin_set_func(u8 pinNumber, GpioFunc func);
//static void gpio_pin_enable(u8 pinNumber);

// UART send function
void uart_send(char c) {
    // Wait until we can send
#if 0
    while (REGS_UART->uart_fr & 0x20) {
        asm volatile("nop");
    }
    // Write the character to the buffer
    REGS_UART->uart_dr = c;
#else
    do {
        asm volatile("nop");
    } while(REGS_UART->uart_fr & 0x20);
    // write the character to the buffer
    REGS_UART->uart_dr=c;
#endif
}

// UART initialization
int uart_init(transport_context_t *transport) {
    REGS_GPIO = (struct GpioRegs *)mmap_device_memory(0, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE, 0, REGS_GPIO_BASE);
    if (REGS_GPIO == MAP_FAILED) {
        perror("REGS_GPIO: MAP_FAILED\n");
        return -1;
    }

    REGS_AUX = (struct AuxRegs *)mmap_device_memory(0, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE, 0, REGS_AUX_BASE);
    if (REGS_AUX == MAP_FAILED) {
        perror("REGS_AUX: MAP_FAILED\n");
        return -1;
    }

    REGS_UART = (struct UartRegs *)mmap_device_memory(0, 0x200, PROT_READ | PROT_WRITE | PROT_NOCACHE, 0, REGS_UART_BASE);
    if (REGS_UART == MAP_FAILED) {
        perror("REGS_UART: MAP_FAILED\n");
        return -1;
    }

    // Create a thread to read data when UART interrupt occurs
    pthread_t uart_thread;
    pthread_create(&uart_thread, NULL, BluetoothSerialReader, transport);
    pthread_detach(uart_thread);

    return 0;
}

// UART reset function
void uart_reset(void) 
{
    while (!(REGS_UART->uart_cr & 0x1)) {
        usleep(1000);
    }
}

// GPIO pin function setup
static void gpio_pin_set_func(u8 pinNumber, GpioFunc func) {
    u8 bitStart = (pinNumber * 3) % 30;
    u8 reg = pinNumber / 10;

    u32 selector = REGS_GPIO->func_select[reg];
    selector &= ~(7 << bitStart);
    selector |= (func << bitStart);

    REGS_GPIO->func_select[reg] = selector;
}

// GPIO pin enable
static void gpio_pin_enable(u8 pinNumber) {
    REGS_GPIO->pupd_enable = 0;
    usleep(150);
    REGS_GPIO->pupd_enable_clocks[pinNumber / 32] = 1 << (pinNumber % 32);
    usleep(150);
    REGS_GPIO->pupd_enable = 0;
    REGS_GPIO->pupd_enable_clocks[pinNumber / 32] = 0;
}

// Serial Interrupt Service Routine
static void serial_ISR(transport_context_t *transport) {
    while (!(REGS_UART->uart_fr & FR_RXFE_MASK)) {
        uint8_t byte = REGS_UART->uart_dr & 0xFF;
#if 1
        printf("Received: %x\n", byte);
        Receive(transport, byte);
#else
        printf("Received: %x\n", byte);
#endif
    }
}

// Interrupt handler
static const struct sigevent *interrupt_handler(void *arg, int id) 
{
    volatile uint32_t mis = REGS_UART->uart_mis;

    if (mis & INT_OE) {
        // Overrun error
        REGS_UART->uart_icr = mis;
        return NULL;
    }

    REGS_UART->uart_icr = mis;
    return &int_event;
}

// Bluetooth Serial Reader Thread
static void *BluetoothSerialReader(void *arg) {
    transport_context_t *transport = (transport_context_t *)arg;
    int id;

    procmgr_ability(0, PROCMGR_ADN_ROOT | PROCMGR_AOP_ALLOW | PROCMGR_AID_INTERRUPT, PROCMGR_AID_EOL);
    SIGEV_INTR_INIT(&int_event);

    pthread_setname_np(pthread_self(), "UART_Reader");

    if (pthread_setschedprio(pthread_self(), 15) != EOK) {
        perror("Setting priority failed");
    }

    // Turn off UART
    REGS_UART->uart_cr = 0;
    REGS_UART->uart_imsc = 0;

    usleep(10000);

    // Setup GPIO pins for UART
    gpio_pin_set_func(30, GFAlt3);
    gpio_pin_set_func(31, GFAlt3);
    gpio_pin_set_func(32, GFAlt3);
    gpio_pin_set_func(33, GFAlt3);

    usleep(10000);

    // Flush the Rx buffer
    while (!(REGS_UART->uart_fr & FR_RXFE_MASK)) {
        uint8_t nData = REGS_UART->uart_dr & 0xFF;
        printf("Clearing buffer: %x\n", nData);
    }

#if _NTO_VERSION >= 800
    int isrid = InterruptAttachThread(153, 0);
    if (isrid == -1) {
        perror("InterruptAttachThread()");
        return NULL;
    }
#else
    id = InterruptAttach(153, interrupt_hdlr, NULL, 0, 0 );
    if (id == -1) {
        perror("InterruptAttachThread()");
        return NULL;
    }
#endif

    // UART configuration
    REGS_UART->uart_icr = 0x7FF;
    REGS_UART->uart_ibrd = 0x1A;  // Baud rate integer divisor
    REGS_UART->uart_fbrd = 0x3;   // Baud rate fractional divisor
    REGS_UART->uart_ifls = IFLS_IFSEL_1_2 << IFLS_RXIFSEL_SHIFT;
    REGS_UART->uart_lcrh = LCRH_WLEN8_MASK | LCRH_FEN_MASK; // 8N1 format
    REGS_UART->uart_cr = CR_UART_EN_MASK | CR_TXE_MASK | CR_RXE_MASK;
    REGS_UART->uart_cr   = 0xB01;
    REGS_UART->uart_imsc = INT_RX | INT_RT | INT_OE;

    while (1) {
        InterruptWait(_NTO_INTR_WAIT_FLAGS_UNMASK, NULL);

#if _NTO_VERSION >= 800
    volatile uint32_t nMIS = REGS_UART->uart_mis;

    if (nMIS & INT_OE) {
        REGS_UART->uart_icr = nMIS;
        return NULL;
    }

    REGS_UART->uart_icr = nMIS;
#endif

        serial_ISR(transport);
    }
    return NULL;
}
