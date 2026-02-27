#pragma once
// =============================================================================
// FILE    : libs/bsp/bspCan/include/can/transceiver/ICanTransceiver.h
// PURPOSE : Hardware-agnostic CAN transceiver interface.  Every platform
//           driver (S32K566 FlexCAN, VDK stub, loopback mock) must implement
//           this interface.  The application layer only ever sees this type.
// =============================================================================

#include "can/canframes/CANFrame.h"
#include "can/transceiver/ICANFrameListener.h"
#include <cstdint>

namespace can {

class ICanTransceiver {
public:
    // -------------------------------------------------------------------------
    // Return codes used by init() and write()
    // -------------------------------------------------------------------------
    enum class ErrorCode : uint8_t {
        CAN_ERR_OK           = 0U,  // Success
        CAN_ERR_TX_BUSY      = 1U,  // Tx message buffer occupied
        CAN_ERR_NOT_INIT     = 2U,  // init() has not been called
        CAN_ERR_INVALID_ARG  = 3U,  // NULL payload or illegal DLC
        CAN_ERR_BUS_OFF      = 4U,  // Controller is in bus-off state
        CAN_ERR_TIMEOUT      = 5U,  // Operation timed out
    };

    virtual ~ICanTransceiver() = default;

    // -------------------------------------------------------------------------
    // Initialise the underlying hardware / virtual CAN controller.
    // Must be called once before write() or addCANFrameListener().
    // -------------------------------------------------------------------------
    virtual ErrorCode init() = 0;

    // -------------------------------------------------------------------------
    // Transmit a CAN frame.
    // Returns CAN_ERR_OK if the frame was placed in the Tx message buffer.
    // The caller does NOT wait for bus arbitration/ACK — that is handled by
    // the hardware.
    // -------------------------------------------------------------------------
    virtual ErrorCode write(CANFrame const& frame) = 0;

    // -------------------------------------------------------------------------
    // Register a listener.  From this point on the transceiver will call
    // listener.canFrameReceived() for every received frame that passes the
    // listener's filter.
    // A maximum of MAX_LISTENERS can be registered simultaneously.
    // -------------------------------------------------------------------------
    virtual void addCANFrameListener(ICANFrameListener& listener) = 0;

    // -------------------------------------------------------------------------
    // Deregister a previously registered listener.
    // -------------------------------------------------------------------------
    virtual void removeCANFrameListener(ICANFrameListener& listener) = 0;

    // -------------------------------------------------------------------------
    // Optional: put the controller into loopback mode (useful for VDK tests)
    // -------------------------------------------------------------------------
    virtual void setLoopback(bool enable) { (void)enable; }

protected:
    static constexpr uint8_t MAX_LISTENERS = 8U;
};

} // namespace can
