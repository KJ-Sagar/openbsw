#pragma once
// =============================================================================
// FILE    : libs/bsp/bspCan/include/can/transceiver/ICANFrameListener.h
// PURPOSE : Pure-virtual interface that any class wishing to receive CAN frames
//           must implement.  The transceiver calls canFrameReceived() from its
//           interrupt context (or deferred task) for every matching frame.
// =============================================================================

#include "can/canframes/CANFrame.h"

namespace can {

class ICANFrameListener {
public:
    virtual ~ICANFrameListener() = default;

    // -------------------------------------------------------------------------
    // Called by the transceiver when a frame passes the listener's filter.
    // NOTE: May be called from interrupt context — keep implementation short
    //       or defer heavy processing to a task via a queue.
    // -------------------------------------------------------------------------
    virtual void canFrameReceived(CANFrame const& frame) = 0;

    // -------------------------------------------------------------------------
    // Returns the filter that determines which frames reach this listener.
    // The transceiver evaluates filter.acceptFrame(frame) before calling
    // canFrameReceived().
    // -------------------------------------------------------------------------
    virtual CANFrame::AbstractCANFrameFilter const& getFilter() = 0;
};

} // namespace can
