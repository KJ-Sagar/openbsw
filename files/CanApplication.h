#pragma once
// =============================================================================
// FILE    : app/src/can/CanApplication.h
// PURPOSE : Top-level application class that owns the CAN send / receive logic.
//           Implements ICANFrameListener so it can register with any transceiver
//           and receive frames directly.
// =============================================================================

#include "can/canframes/CANFrame.h"
#include "can/transceiver/ICanTransceiver.h"
#include "can/transceiver/ICANFrameListener.h"
#include <cstdint>

namespace app {

// =============================================================================
// CAN message ID definitions used by this application
// =============================================================================
namespace CanIds {
    static constexpr uint32_t TX_HEARTBEAT  = 0x100U;  // Periodic status frame
    static constexpr uint32_t TX_RESPONSE   = 0x101U;  // Response to commands
    static constexpr uint32_t RX_COMMAND    = 0x200U;  // Incoming command frame
    static constexpr uint32_t RX_CONFIG     = 0x201U;  // Incoming config frame
} // namespace CanIds

// =============================================================================
// Application command byte definitions (first data byte of RX_COMMAND frame)
// =============================================================================
namespace CanCmd {
    static constexpr uint8_t START  = 0x01U;
    static constexpr uint8_t STOP   = 0x02U;
    static constexpr uint8_t RESET  = 0x03U;
    static constexpr uint8_t STATUS = 0x04U;
} // namespace CanCmd

// =============================================================================
// CanApplication
// =============================================================================
class CanApplication : public ::can::ICANFrameListener {
public:
    // -------------------------------------------------------------------------
    // Constructor — takes a reference to ANY ICanTransceiver (hardware or VDK)
    // -------------------------------------------------------------------------
    explicit CanApplication(::can::ICanTransceiver& transceiver);

    // -------------------------------------------------------------------------
    // ICANFrameListener interface
    // -------------------------------------------------------------------------
    void canFrameReceived(::can::CANFrame const& frame) override;
    ::can::CANFrame::AbstractCANFrameFilter const& getFilter() override;

    // -------------------------------------------------------------------------
    // Application lifecycle
    // -------------------------------------------------------------------------
    // Call once at startup: registers listener and initialises transceiver
    void init();

    // -------------------------------------------------------------------------
    // Periodic tasks — call from your 10 ms / 100 ms scheduler or super-loop
    // -------------------------------------------------------------------------
    void task10ms();    // Sends heartbeat every 10 ms
    void task100ms();   // Sends full status frame every 100 ms

    // -------------------------------------------------------------------------
    // Statistics (useful for VDK verification)
    // -------------------------------------------------------------------------
    uint32_t getTxCount() const { return _txCount; }
    uint32_t getRxCount() const { return _rxCount; }
    bool     isRunning()  const { return _running;  }

private:
    void sendHeartbeat();
    void sendResponse(uint8_t statusByte);
    void handleCommand(::can::CANFrame const& frame);
    void handleConfig(::can::CANFrame const& frame);

    ::can::ICanTransceiver&           _transceiver;
    ::can::CANFrame::AcceptAllFilter  _filter;

    uint32_t _txCount  {0U};
    uint32_t _rxCount  {0U};
    uint32_t _tick10ms {0U};
    bool     _running  {false};
};

} // namespace app
