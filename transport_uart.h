#ifndef TRANSPORT_UART_H
#define TRANSPORT_UART_H

#include <stdint.h>
#include "transport.h"

#define PBASE 0xFE000000

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define LCRH_WLEN8_MASK     (3 << 5)
#define CR_UART_EN_MASK     (1 << 0)
#define CR_RXE_MASK         (1 << 9)
#define CR_TXE_MASK         (1 << 8)
#define CR_LBE_MASK         (1 << 7)
#define INT_OE              (1 << 10)
#define INT_BE              (1 << 9)
#define INT_PE              (1 << 8)
#define INT_FE              (1 << 7)
#define INT_RT              (1 << 6)
#define INT_TX              (1 << 5)
#define INT_RX              (1 << 4)
#define FR_RXFE_MASK        (1 << 4)
#define IFLS_RXIFSEL_SHIFT  3
#define LCRH_FEN_MASK       (1 << 4)
#define IFLS_RXIFSEL_MASK   (7 << IFLS_RXIFSEL_SHIFT)
#define IFLS_TXIFSEL_SHIFT  0
#define IFLS_TXIFSEL_MASK   (7 << IFLS_TXIFSEL_SHIFT)
#define IFLS_IFSEL_1_8      0
#define IFLS_IFSEL_1_4      1
#define IFLS_IFSEL_1_2      2

typedef volatile uint32_t reg32;

struct AuxRegs {
    reg32 irq_status;
    reg32 enables;
    reg32 reserved[14];
    reg32 mu_io;
    reg32 mu_ier;
    reg32 mu_iir;
    reg32 mu_lcr;
    reg32 mu_mcr;
    reg32 mu_lsr;
    reg32 mu_msr;
    reg32 mu_scratch;
    reg32 mu_control;
    reg32 mu_status;
    reg32 mu_baud_rate;
};

extern struct AuxRegs *REGS_AUX;

struct UartRegs {
    reg32 uart_dr;
    reg32 uart_rsrecr;
    reg32 reserved1[4];
    reg32 uart_fr;
    reg32 reserved2;
    reg32 uart_ilpr;
    reg32 uart_ibrd;
    reg32 uart_fbrd;
    reg32 uart_lcrh;
    reg32 uart_cr;
    reg32 uart_ifls;
    reg32 uart_imsc;
    reg32 uart_ris;
    reg32 uart_mis;
    reg32 uart_icr;
    reg32 uart_dmacr;
    reg32 reserved3[13];
    reg32 uart_itcr;
    reg32 uart_itip;
    reg32 uart_itop;
    reg32 uart_tdr;
};
extern struct UartRegs *REGS_UART;

struct GpioPinData {
    reg32 reserved;
    reg32 data[2];
};

struct GpioRegs {
    reg32 func_select[6];
    struct GpioPinData output_set;
    struct GpioPinData output_clear;
    struct GpioPinData level;
    struct GpioPinData ev_detect_status;
    struct GpioPinData re_detect_enable;
    struct GpioPinData fe_detect_enable;
    struct GpioPinData hi_detect_enable;
    struct GpioPinData lo_detect_enable;
    struct GpioPinData async_re_detect;
    struct GpioPinData async_fe_detect;
    reg32 reserved;
    reg32 pupd_enable;
    reg32 pupd_enable_clocks[2];
};
extern struct GpioRegs *REGS_GPIO;

typedef enum _GpioFunc {
    GFInput = 0,
    GFOutput = 1,
    GFAlt0 = 4,
    GFAlt1 = 5,
    GFAlt2 = 6,
    GFAlt3 = 7,
    GFAlt4 = 3,
    GFAlt5 = 2
} GpioFunc;

static void gpio_pin_set_func(u8 pinNumber, GpioFunc func);
static void gpio_pin_enable(u8 pinNumber);
int uart_init(transport_context_t *transport);
void uart_send(char c);
void uart_reset(void);

#endif // TRANSPORT_UART_H