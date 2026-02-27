// =============================================================================
// FILE    : bsp/s32k566/bspCan/S32K5FlexCanTransceiver.cpp
// PURPOSE : Full FlexCAN_0 register-level driver for the NXP S32K566.
//
// HARDWARE ASSUMPTIONS (S32K566 EVB default):
//   CAN0_TX  → PTC7  (MSCR[55] = ALT5)
//   CAN0_RX  → PTC6  (MSCR[54] = ALT5, IMCR[512] = 2)
//   CAN clock source: SPLLDIV2 = 80 MHz (PLL configured by startup code)
//   Baud rate: 500 kbps
// =============================================================================

#include "S32K5FlexCanTransceiver.h"
#include "S32K566_registers.h"
#include <cstring>

// =============================================================================
// Global singleton pointer — used by the ISR wrapper (extern "C")
// =============================================================================
static S32K5FlexCanTransceiver* s_instance = nullptr;

// =============================================================================
// Constructor
// =============================================================================
S32K5FlexCanTransceiver::S32K5FlexCanTransceiver() {
    s_instance = this;
    (void)::memset(_listeners, 0, sizeof(_listeners));
}

// =============================================================================
// Convenience: get pointer to a FlexCAN register by offset
// =============================================================================
volatile uint32_t* S32K5FlexCanTransceiver::reg(uint32_t offset) const {
    return reinterpret_cast<volatile uint32_t*>(FLEXCAN0_BASE + offset);
}

// =============================================================================
// init() — configure clocks, pins, bit timing, and message buffers
// =============================================================================
::can::ICanTransceiver::ErrorCode S32K5FlexCanTransceiver::init() {
    if (_initialised) {
        return ErrorCode::CAN_ERR_OK;
    }

    configureClocks();
    configurePins();

    // ------------------------------------------------------------------
    // 1. Wake up module (clear MDIS) and enter freeze mode
    // ------------------------------------------------------------------
    *reg(FLEXCAN_MCR) &= ~FLEXCAN_MCR_MDIS;
    // Wait for clock to stabilise (NOTRDY clears)
    while (*reg(FLEXCAN_MCR) & FLEXCAN_MCR_LPMACK) {}

    // Soft reset
    *reg(FLEXCAN_MCR) |= FLEXCAN_MCR_SOFTRST;
    while (*reg(FLEXCAN_MCR) & FLEXCAN_MCR_SOFTRST) {}

    enterFreezeMode();

    // ------------------------------------------------------------------
    // 2. Configure MCR
    //      IRMQ  = 1  : individual Rx masking per MB
    //      SRXDIS= 1  : disable self-reception (loopback overrides this)
    //      SUPV  = 1  : supervisor mode
    //      MAXMB = 15 : use MBs 0..15
    // ------------------------------------------------------------------
    uint32_t mcr = *reg(FLEXCAN_MCR);
    mcr |=  FLEXCAN_MCR_IRMQ;
    mcr |=  FLEXCAN_MCR_SRXDIS;
    mcr |=  FLEXCAN_MCR_SUPV;
    mcr  = (mcr & ~0x7FUL) | FLEXCAN_MCR_MAXMB(15UL);
    if (_loopback) {
        mcr &= ~FLEXCAN_MCR_SRXDIS; // allow self-reception in loopback
    }
    *reg(FLEXCAN_MCR) = mcr;

    // ------------------------------------------------------------------
    // 3. Bit timing — 500 kbps with 80 MHz CAN clock
    // ------------------------------------------------------------------
    uint32_t ctrl1 = CTRL1_500KBPS_80MHZ;
    if (_loopback) {
        ctrl1 |= (1UL << 12); // LPB bit — internal loopback mode
    }
    *reg(FLEXCAN_CTRL1) = ctrl1;

    // ------------------------------------------------------------------
    // 4. Disable all interrupts and clear all flags
    // ------------------------------------------------------------------
    *reg(FLEXCAN_IMASK1) = 0x00000000UL;
    *reg(FLEXCAN_IFLAG1) = 0xFFFFFFFFUL;  // W1C

    // ------------------------------------------------------------------
    // 5. Invalidate all MBs (set CODE to 0 = inactive)
    // ------------------------------------------------------------------
    for (uint32_t i = 0; i < 16U; ++i) {
        volatile uint32_t* mbBase = reinterpret_cast<volatile uint32_t*>(
            FLEXCAN0_BASE + FLEXCAN_MB_BASE + i * 16U);
        mbBase[0] = 0x00000000UL;  // CS word: CODE=0 (inactive)
        mbBase[1] = 0x00000000UL;  // ID word
        mbBase[2] = 0x00000000UL;  // Data 0-3
        mbBase[3] = 0x00000000UL;  // Data 4-7
    }

    // ------------------------------------------------------------------
    // 6. Configure Tx MB (MB0) and Rx MB (MB4)
    // ------------------------------------------------------------------
    configureMsgBuffer_Tx(TX_MB_IDX);
    // Accept frame ID 0x200 and 0x201 on RX MB4
    // mask = 0x7FC means "match bits 10:2" → accepts 0x200 and 0x201
    configureMsgBuffer_Rx(RX_MB_IDX, 0x200U, 0x7FCUL);

    // ------------------------------------------------------------------
    // 7. Enable Rx interrupt for MB4
    // ------------------------------------------------------------------
    *reg(FLEXCAN_IMASK1) |= (1UL << RX_MB_IDX);

    // ------------------------------------------------------------------
    // 8. Configure NVIC — priority 5, enable
    // ------------------------------------------------------------------
    NVIC_SetPriority(IRQ_FLEXCAN0_MB0_15, 5U);
    NVIC_EnableIRQ(IRQ_FLEXCAN0_MB0_15);

    // ------------------------------------------------------------------
    // 9. Exit freeze mode → go active
    // ------------------------------------------------------------------
    exitFreezeMode();

    _initialised = true;
    return ErrorCode::CAN_ERR_OK;
}

// =============================================================================
// write() — load a frame into Tx MB0 and kick off transmission
// =============================================================================
::can::ICanTransceiver::ErrorCode
S32K5FlexCanTransceiver::write(::can::CANFrame const& frame) {
    if (!_initialised) {
        return ErrorCode::CAN_ERR_NOT_INIT;
    }

    uint8_t const* d   = frame.getPayload();
    uint8_t const  dlc = frame.getPayloadLength();

    if (d == nullptr || dlc > ::can::CANFrame::MAX_FRAME_LENGTH) {
        return ErrorCode::CAN_ERR_INVALID_ARG;
    }

    // Pointer to MB0 (4 × uint32_t words)
    volatile uint32_t* mb = reinterpret_cast<volatile uint32_t*>(
        FLEXCAN0_BASE + FLEXCAN_MB_BASE + TX_MB_IDX * 16U);

    // Check that previous Tx is done (CS CODE should be 0x8 = Tx_ABORT or 0x4 = empty)
    uint32_t cs = mb[0];
    uint32_t code = (cs >> 24) & 0xFUL;
    if (code == 0xCUL) {
        // Still transmitting
        return ErrorCode::CAN_ERR_TX_BUSY;
    }

    // Write ID (standard 11-bit in bits 28:18)
    mb[1] = (frame.getId() & 0x7FFUL) << 18U;

    // Write data bytes — FlexCAN stores them big-endian within each word
    uint32_t word0 =
        ((uint32_t)d[0] << 24U) | ((uint32_t)d[1] << 16U) |
        ((uint32_t)d[2] <<  8U) | ((uint32_t)d[3]       );
    uint32_t word1 =
        ((uint32_t)d[4] << 24U) | ((uint32_t)d[5] << 16U) |
        ((uint32_t)d[6] <<  8U) | ((uint32_t)d[7]       );
    mb[2] = word0;
    mb[3] = word1;

    // Write CS word last with CODE=0xC to trigger transmission
    mb[0] = MB_CS_CODE_TX_DATA | MB_CS_DLC(dlc);

    return ErrorCode::CAN_ERR_OK;
}

// =============================================================================
// Listener management
// =============================================================================
void S32K5FlexCanTransceiver::addCANFrameListener(
    ::can::ICANFrameListener& listener) {
    if (_listenerCount >= MAX_LISTENERS) { return; }
    for (uint8_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == &listener) { return; } // already registered
    }
    _listeners[_listenerCount++] = &listener;
}

void S32K5FlexCanTransceiver::removeCANFrameListener(
    ::can::ICANFrameListener& listener) {
    for (uint8_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == &listener) {
            // Compact the array
            _listeners[i] = _listeners[--_listenerCount];
            _listeners[_listenerCount] = nullptr;
            return;
        }
    }
}

// =============================================================================
// setLoopback() — must be called BEFORE init()
// =============================================================================
void S32K5FlexCanTransceiver::setLoopback(bool enable) {
    _loopback = enable;
}

// =============================================================================
// handleRxIrq() — called from ISR when MB4 receives a frame
// =============================================================================
void S32K5FlexCanTransceiver::handleRxIrq() {
    uint32_t flags = *reg(FLEXCAN_IFLAG1);

    if (!(flags & (1UL << RX_MB_IDX))) { return; }

    volatile uint32_t* mb = reinterpret_cast<volatile uint32_t*>(
        FLEXCAN0_BASE + FLEXCAN_MB_BASE + RX_MB_IDX * 16U);

    // --- Lock MB by reading CS word ---
    uint32_t const cs   = mb[0];
    uint32_t const idw  = mb[1];
    uint32_t const dw0  = mb[2];
    uint32_t const dw1  = mb[3];

    // Unlock MB: read the free-running timer
    (void)*reg(FLEXCAN_TIMER);

    // Extract fields
    uint32_t const id  = (idw >> 18U) & 0x7FFUL;  // standard ID
    uint8_t  const dlc = static_cast<uint8_t>((cs >> 16U) & 0xFUL);

    // Reconstruct byte array from big-endian words
    uint8_t data[8] = {};
    data[0] = static_cast<uint8_t>(dw0 >> 24U);
    data[1] = static_cast<uint8_t>(dw0 >> 16U);
    data[2] = static_cast<uint8_t>(dw0 >>  8U);
    data[3] = static_cast<uint8_t>(dw0       );
    data[4] = static_cast<uint8_t>(dw1 >> 24U);
    data[5] = static_cast<uint8_t>(dw1 >> 16U);
    data[6] = static_cast<uint8_t>(dw1 >>  8U);
    data[7] = static_cast<uint8_t>(dw1       );

    ::can::CANFrame frame;
    frame.setId(id);
    frame.set(data, dlc);

    // Dispatch to all registered listeners
    dispatchToListeners(frame);

    // Clear interrupt flag (W1C)
    *reg(FLEXCAN_IFLAG1) = (1UL << RX_MB_IDX);
}

// =============================================================================
// handleErrorIrq() — called from error ISR
// =============================================================================
void S32K5FlexCanTransceiver::handleErrorIrq() {
    uint32_t esr = *reg(FLEXCAN_ESR1);
    // Clear error flags (W1C)
    *reg(FLEXCAN_ESR1) = esr;
    // TODO: increment error counters, trigger bus-off recovery if needed
}

// =============================================================================
// Private helpers
// =============================================================================

void S32K5FlexCanTransceiver::configureClocks() {
    // Enable SPLLDIV2 as clock source for FlexCAN0 (80 MHz)
    volatile uint32_t* pcc_can0 =
        reinterpret_cast<volatile uint32_t*>(PCC_BASE + PCC_FlexCAN0);
    *pcc_can0 &= ~PCC_CGC_MASK;          // Disable gate before changing source
    *pcc_can0  = PCC_PCS_SPLLDIV2;       // Set source
    *pcc_can0 |= PCC_CGC_MASK;           // Enable gate
}

void S32K5FlexCanTransceiver::configurePins() {
    // ---- CAN0_TX → PTC7 = MSCR[55]: ALT5, output buffer enable ----
    volatile uint32_t* mscr_tx =
        reinterpret_cast<volatile uint32_t*>(
            SIUL2_BASE + SIUL2_MSCR_BASE + CAN0_TX_MSCR_IDX * 4U);
    *mscr_tx = SIUL2_MSCR_OBE | SIUL2_MSCR_DSE_HIGH | CAN0_TX_ALT_FUNC;

    // ---- CAN0_RX → PTC6 = MSCR[54]: ALT5, input buffer enable, pull-up ----
    volatile uint32_t* mscr_rx =
        reinterpret_cast<volatile uint32_t*>(
            SIUL2_BASE + SIUL2_MSCR_BASE + CAN0_RX_MSCR_IDX * 4U);
    *mscr_rx = SIUL2_MSCR_IBE | SIUL2_MSCR_PUE | SIUL2_MSCR_PUS | CAN0_RX_ALT_FUNC;

    // ---- IMCR[512] = 2 — route CAN0_RX from PTC6 to FlexCAN_0 RX ----
    volatile uint32_t* imcr =
        reinterpret_cast<volatile uint32_t*>(
            SIUL2_BASE + SIUL2_IMCR_BASE + CAN0_RX_IMCR_IDX * 4U);
    *imcr = 2UL;
}

void S32K5FlexCanTransceiver::enterFreezeMode() {
    *reg(FLEXCAN_MCR) |= (FLEXCAN_MCR_FRZ | FLEXCAN_MCR_HALT);
    while (!(*reg(FLEXCAN_MCR) & FLEXCAN_MCR_FRZACK)) {}
}

void S32K5FlexCanTransceiver::exitFreezeMode() {
    *reg(FLEXCAN_MCR) &= ~(FLEXCAN_MCR_FRZ | FLEXCAN_MCR_HALT);
    while (*reg(FLEXCAN_MCR) & FLEXCAN_MCR_FRZACK) {}
}

void S32K5FlexCanTransceiver::configureMsgBuffer_Tx(uint8_t mbIdx) {
    volatile uint32_t* mb = reinterpret_cast<volatile uint32_t*>(
        FLEXCAN0_BASE + FLEXCAN_MB_BASE + mbIdx * 16U);

    // CS = CODE 0x8 (Tx inactive/abort) — ready to transmit when write() sets CODE=0xC
    mb[0] = (0x8UL << 24U);
    mb[1] = 0x00000000UL;
    mb[2] = 0x00000000UL;
    mb[3] = 0x00000000UL;

    // Tx MB does not need an individual mask, but clear it anyway
    volatile uint32_t* rximr = reinterpret_cast<volatile uint32_t*>(
        FLEXCAN0_BASE + FLEXCAN_RXIMR0 + mbIdx * 4U);
    *rximr = 0x00000000UL;
}

void S32K5FlexCanTransceiver::configureMsgBuffer_Rx(
    uint8_t mbIdx, uint32_t acceptId, uint32_t mask) {

    volatile uint32_t* mb = reinterpret_cast<volatile uint32_t*>(
        FLEXCAN0_BASE + FLEXCAN_MB_BASE + mbIdx * 16U);

    // CS = CODE 0x4 — empty, ready to receive
    mb[0] = MB_CS_CODE_RX_EMPTY;
    // ID field: standard ID in bits 28:18
    mb[1] = (acceptId & 0x7FFUL) << 18U;
    mb[2] = 0x00000000UL;
    mb[3] = 0x00000000UL;

    // Individual Rx mask — 1 bit = "must match", 0 bit = "don't care"
    // Standard mask occupies bits 28:18 of RXIMR
    volatile uint32_t* rximr = reinterpret_cast<volatile uint32_t*>(
        FLEXCAN0_BASE + FLEXCAN_RXIMR0 + mbIdx * 4U);
    *rximr = (mask & 0x7FFUL) << 18U;
}

void S32K5FlexCanTransceiver::dispatchToListeners(::can::CANFrame const& frame) {
    for (uint8_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] != nullptr) {
            if (_listeners[i]->getFilter().acceptFrame(frame)) {
                _listeners[i]->canFrameReceived(frame);
            }
        }
    }
}

// =============================================================================
// ISR wrappers — must match the vector table entry names in the startup file
// =============================================================================
extern "C" {

void FlexCAN0_ORed_0_15_MB_IRQHandler(void) {
    if (s_instance != nullptr) {
        s_instance->handleRxIrq();
    }
}

void FlexCAN0_Error_IRQHandler(void) {
    if (s_instance != nullptr) {
        s_instance->handleErrorIrq();
    }
}

} // extern "C"
