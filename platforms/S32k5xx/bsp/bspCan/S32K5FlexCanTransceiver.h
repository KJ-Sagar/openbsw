#pragma once
// =============================================================================
// FILE    : bsp/s32k566/bspCan/S32K5FlexCanTransceiver.h
// PURPOSE : S32K566 FlexCAN_0 hardware driver that implements the
//           ICanTransceiver interface.
//
//           Supports:
//             - Classic CAN 2.0B (standard & extended IDs)
//             - Up to MAX_LISTENERS simultaneous frame listeners
//             - 1 dedicated Tx message buffer (MB0)
//             - 1 dedicated Rx message buffer (MB4)
//             - Individual Rx masking (IRMQ)
//             - Loopback mode for self-test / VDK verification
// =============================================================================

#include "can/transceiver/ICanTransceiver.h"
#include "can/canframes/CANFrame.h"
#include "can/transceiver/ICANFrameListener.h"
#include <cstdint>

class S32K5FlexCanTransceiver : public ::can::ICanTransceiver {
public:
    // -------------------------------------------------------------------------
    // Message buffer indices
    // -------------------------------------------------------------------------
    static constexpr uint8_t  TX_MB_IDX  = 0U;  // MB used for transmission
    static constexpr uint8_t  RX_MB_IDX  = 4U;  // MB used for reception

    // -------------------------------------------------------------------------
    // Constructor / Destructor
    // -------------------------------------------------------------------------
    S32K5FlexCanTransceiver();
    ~S32K5FlexCanTransceiver() override = default;

    // -------------------------------------------------------------------------
    // ICanTransceiver overrides
    // -------------------------------------------------------------------------
    ErrorCode init()                                      override;
    ErrorCode write(::can::CANFrame const& frame)         override;
    void      addCANFrameListener(::can::ICANFrameListener& listener)    override;
    void      removeCANFrameListener(::can::ICANFrameListener& listener) override;
    void      setLoopback(bool enable)                    override;

    // -------------------------------------------------------------------------
    // ISR entry point — call from the FlexCAN0_ORed_0_15_MB ISR vector
    // -------------------------------------------------------------------------
    void handleRxIrq();
    void handleErrorIrq();

private:
    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------
    void configureClocks();
    void configurePins();
    void enterFreezeMode();
    void exitFreezeMode();
    void configureMsgBuffer_Tx(uint8_t mbIdx);
    void configureMsgBuffer_Rx(uint8_t mbIdx, uint32_t acceptId, uint32_t mask);
    void dispatchToListeners(::can::CANFrame const& frame);

    volatile uint32_t* reg(uint32_t offset) const;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    bool      _initialised {false};
    bool      _loopback    {false};

    // Listener table
    ::can::ICANFrameListener* _listeners[MAX_LISTENERS] {};
    uint8_t                   _listenerCount {0U};
};
