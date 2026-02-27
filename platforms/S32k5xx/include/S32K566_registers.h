#pragma once
// =============================================================================
// FILE    : bsp/s32k566/include/S32K566_registers.h
// PURPOSE : Minimal register definitions for S32K566 peripherals used by the
//           BSP CAN driver.  In a full project, replace this with the NXP
//           RTD/CMSIS device header  <S32K566.h>  from the S32K5 SDK.
//
//           Register base addresses taken from S32K5 Reference Manual
//           Rev. 1, Chapter 3 (Memory Map).
// =============================================================================

#include <cstdint>

// =============================================================================
// Helper macro — converts a base address + byte offset into a volatile u32 ref
// =============================================================================
#define S32_REG32(base, offset) \
    (*reinterpret_cast<volatile uint32_t*>((base) + (offset)))

// =============================================================================
// PCC — Peripheral Clock Controller
//   Base: 0x40065000
// =============================================================================
static constexpr uint32_t PCC_BASE         = 0x40065000UL;

// PCC slot offsets for each FlexCAN instance
static constexpr uint32_t PCC_FlexCAN0     = 0x090UL;
static constexpr uint32_t PCC_FlexCAN1     = 0x094UL;
static constexpr uint32_t PCC_FlexCAN2     = 0x098UL;
static constexpr uint32_t PCC_FlexCAN3     = 0x09CUL;
static constexpr uint32_t PCC_FlexCAN4     = 0x0A0UL;
static constexpr uint32_t PCC_FlexCAN5     = 0x0A4UL;

static constexpr uint32_t PCC_CGC_MASK     = (1UL << 30);  // Clock Gate Control
static constexpr uint32_t PCC_PCS_MASK     = (0x7UL << 24); // Peripheral Clock Source
static constexpr uint32_t PCC_PCS_SPLLDIV2 = (0x6UL << 24); // SPLLDIV2 (80 MHz typ.)

// =============================================================================
// SIUL2 — Signal I/O Unit Layer 2 (pin mux)
//   Base: 0x40290000
// =============================================================================
static constexpr uint32_t SIUL2_BASE       = 0x40290000UL;
static constexpr uint32_t SIUL2_MSCR_BASE  = 0x240UL;  // MSCR[0] offset

// MSCR bit fields
static constexpr uint32_t SIUL2_MSCR_SSS(uint32_t func) { return (func & 0xFUL); }
static constexpr uint32_t SIUL2_MSCR_OBE      = (1UL << 21);  // Output buffer enable
static constexpr uint32_t SIUL2_MSCR_IBE      = (1UL << 19);  // Input  buffer enable
static constexpr uint32_t SIUL2_MSCR_PUE      = (1UL << 13);  // Pull enable
static constexpr uint32_t SIUL2_MSCR_PUS      = (1UL << 12);  // Pull select (1=pull-up)
static constexpr uint32_t SIUL2_MSCR_DSE_HIGH = (0x7UL << 4); // High drive strength

// IMCR base — input mux control registers
static constexpr uint32_t SIUL2_IMCR_BASE  = 0xA40UL;

// S32K566 EVB: CAN0 default pins
//   TX → PTC7 = SIUL2 MSCR[55]  ALT5
//   RX → PTC6 = SIUL2 MSCR[54]  ALT5 + IMCR[512] = 2
static constexpr uint32_t CAN0_TX_MSCR_IDX  = 55UL;  // PTC7
static constexpr uint32_t CAN0_RX_MSCR_IDX  = 54UL;  // PTC6
static constexpr uint32_t CAN0_RX_IMCR_IDX  = 512UL; // IMCR for CAN0_RX
static constexpr uint32_t CAN0_TX_ALT_FUNC  = 5UL;
static constexpr uint32_t CAN0_RX_ALT_FUNC  = 5UL;

// =============================================================================
// FlexCAN — CAN controller
//   FlexCAN_0 Base: 0x40308000
//   FlexCAN_1 Base: 0x40310000
//   FlexCAN_2 Base: 0x40318000
//   ... (each instance 0x8000 apart)
// =============================================================================
static constexpr uint32_t FLEXCAN0_BASE     = 0x40308000UL;
static constexpr uint32_t FLEXCAN1_BASE     = 0x40310000UL;
static constexpr uint32_t FLEXCAN2_BASE     = 0x40318000UL;
static constexpr uint32_t FLEXCAN3_BASE     = 0x40320000UL;
static constexpr uint32_t FLEXCAN4_BASE     = 0x40328000UL;
static constexpr uint32_t FLEXCAN5_BASE     = 0x40330000UL;

// ---- FlexCAN register offsets -----------------------------------------------
static constexpr uint32_t FLEXCAN_MCR       = 0x0000UL;  // Module Config
static constexpr uint32_t FLEXCAN_CTRL1     = 0x0004UL;  // Control 1
static constexpr uint32_t FLEXCAN_TIMER     = 0x0008UL;  // Free-running timer
static constexpr uint32_t FLEXCAN_RXMGMASK  = 0x0010UL;  // Rx Mailbox Global Mask
static constexpr uint32_t FLEXCAN_RXFGMASK  = 0x0048UL;  // Rx FIFO Global Mask
static constexpr uint32_t FLEXCAN_ESR1      = 0x0020UL;  // Error & Status 1
static constexpr uint32_t FLEXCAN_IMASK1    = 0x0028UL;  // Interrupt Mask 1
static constexpr uint32_t FLEXCAN_IFLAG1    = 0x0030UL;  // Interrupt Flag 1
static constexpr uint32_t FLEXCAN_CTRL2     = 0x0034UL;  // Control 2
static constexpr uint32_t FLEXCAN_MECR      = 0x0AE0UL;  // Memory Error Control
static constexpr uint32_t FLEXCAN_MB_BASE   = 0x0080UL;  // Message Buffer 0
static constexpr uint32_t FLEXCAN_RXIMR0    = 0x0880UL;  // Rx Individual Mask MB0

// ---- MCR bit fields ---------------------------------------------------------
static constexpr uint32_t FLEXCAN_MCR_MDIS  = (1UL << 31); // Module disable
static constexpr uint32_t FLEXCAN_MCR_FRZ   = (1UL << 30); // Freeze enable
static constexpr uint32_t FLEXCAN_MCR_RFEN  = (1UL << 29); // Rx FIFO enable
static constexpr uint32_t FLEXCAN_MCR_HALT  = (1UL << 28); // Halt (enter freeze)
static constexpr uint32_t FLEXCAN_MCR_NOTRDY= (1UL << 27); // FlexCAN not ready
static constexpr uint32_t FLEXCAN_MCR_SOFTRST=(1UL << 25); // Soft reset
static constexpr uint32_t FLEXCAN_MCR_FRZACK= (1UL << 24); // Freeze acknowledge
static constexpr uint32_t FLEXCAN_MCR_SUPV  = (1UL << 23); // Supervisor mode
static constexpr uint32_t FLEXCAN_MCR_LPMACK= (1UL << 20); // Low-power mode ack
static constexpr uint32_t FLEXCAN_MCR_SRXDIS= (1UL << 17); // Self-reception disable
static constexpr uint32_t FLEXCAN_MCR_IRMQ  = (1UL << 16); // Individual Rx masking
static constexpr uint32_t FLEXCAN_MCR_AEN   = (1UL << 12); // Abort enable
static constexpr uint32_t FLEXCAN_MCR_MAXMB(uint32_t n) { return (n & 0x7FUL); }

// ---- CTRL1 bit fields (bit timing) ------------------------------------------
// Nominal bit timing for 500 kbps with 80 MHz CAN clock:
//   PRESDIV = 7  → Tq clock = 80 MHz / (7+1) = 10 MHz
//   PROPSEG = 5  → prop seg  = 6 Tq
//   PSEG1   = 7  → phase1    = 8 Tq   (PSEG1 = 7)
//   PSEG2   = 3  → phase2    = 4 Tq   (PSEG2 = 3)
//   RJW     = 3  → SJW       = 4 Tq   (RJW   = 3)
//   Total   = 1 (sync) + 6 + 8 + 4 = 19 Tq  → 10 MHz / 20 = 500 kbps
static constexpr uint32_t CTRL1_500KBPS_80MHZ =
    (7UL  << 24) |   // PRESDIV
    (3UL  << 22) |   // RJW
    (5UL  << 16) |   // PSEG1
    (3UL  << 19) |   // PSEG2
    (5UL  <<  0);    // PROPSEG

// ---- Message Buffer CS word CODE field values --------------------------------
static constexpr uint32_t MB_CS_CODE_RX_EMPTY  = (0x4UL << 24); // MB ready to receive
static constexpr uint32_t MB_CS_CODE_TX_DATA   = (0xCUL << 24); // Transmit data frame
static constexpr uint32_t MB_CS_CODE_TX_REMOTE = (0xAUL << 24); // Transmit remote frame
static constexpr uint32_t MB_CS_IDE            = (1UL << 21);   // Extended ID flag
static constexpr uint32_t MB_CS_RTR            = (1UL << 20);   // Remote Tx request
static constexpr uint32_t MB_CS_DLC(uint32_t d){ return ((d & 0xFUL) << 16); }

// ---- Interrupt vector numbers (NVIC IRQ numbers, NOT exception numbers) -----
static constexpr uint32_t IRQ_FLEXCAN0_MB0_15  = 109UL;
static constexpr uint32_t IRQ_FLEXCAN0_MB16_31 = 110UL;
static constexpr uint32_t IRQ_FLEXCAN0_ERR     = 108UL;

// =============================================================================
// NVIC helpers  (ARM Cortex-M standard addresses)
// =============================================================================
static constexpr uint32_t NVIC_ISER_BASE       = 0xE000E100UL;
static constexpr uint32_t NVIC_ICER_BASE       = 0xE000E180UL;
static constexpr uint32_t NVIC_ICPR_BASE       = 0xE000E280UL;
static constexpr uint32_t NVIC_IPR_BASE        = 0xE000E400UL;

inline void NVIC_EnableIRQ(uint32_t irq) {
    S32_REG32(NVIC_ISER_BASE, ((irq >> 5) << 2)) = (1UL << (irq & 0x1FUL));
}
inline void NVIC_SetPriority(uint32_t irq, uint8_t prio) {
    volatile uint8_t* ipr = reinterpret_cast<volatile uint8_t*>(NVIC_IPR_BASE + irq);
    *ipr = static_cast<uint8_t>(prio << 4); // top 4 bits = priority
}
