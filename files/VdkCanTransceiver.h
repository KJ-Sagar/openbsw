#pragma once
// =============================================================================
// FILE    : bsp/vdk/VdkCanTransceiver.h
// PURPOSE : Synopsys VDK stub implementation of ICanTransceiver.
//
//           When the VDK is active, its FlexCAN peripheral model handles the
//           register access.  This file provides a thin C++ wrapper that:
//             - Exposes the same ICanTransceiver interface as the hardware driver
//             - Talks to the VDK CAN bus model via a socket or shared memory
//             - Allows the CanApplication code to run completely unchanged
//
//           For stand-alone (no VDK socket) loopback testing, define
//           VDK_LOOPBACK at compile time.
// =============================================================================

#include "can/transceiver/ICanTransceiver.h"
#include "can/canframes/CANFrame.h"
#include <cstdint>

class VdkCanTransceiver : public ::can::ICanTransceiver {
public:
    // Port on which the VDK CAN bus model listens (matches vdk params.yaml)
    static constexpr uint16_t VDK_CAN_PORT = 9000U;

    VdkCanTransceiver();
    ~VdkCanTransceiver() override;

    ErrorCode init()                                             override;
    ErrorCode write(::can::CANFrame const& frame)                override;
    void      addCANFrameListener(::can::ICANFrameListener& l)   override;
    void      removeCANFrameListener(::can::ICANFrameListener& l)override;
    void      setLoopback(bool enable)                           override;

    // Call from main loop (or a VDK thread) to poll for incoming frames
    void poll();

private:
    void dispatchToListeners(::can::CANFrame const& frame);

    int      _sockFd       {-1};
    bool     _initialised  {false};
    bool     _loopback     {false};

    ::can::ICANFrameListener* _listeners[MAX_LISTENERS] {};
    uint8_t                   _listenerCount {0U};
};
