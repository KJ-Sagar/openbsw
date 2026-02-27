#pragma once
// =============================================================================
// FILE    : libs/bsp/bspCan/include/can/canframes/CANFrame.h
// PURPOSE : Hardware-agnostic CAN frame data structure used across all layers
//           of the OpenBSW CAN stack.
// =============================================================================

#include <cstdint>
#include <cstring>

namespace can {

class CANFrame {
public:
    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    static constexpr uint8_t  MAX_FRAME_LENGTH    = 8U;   // Classic CAN
    static constexpr uint8_t  MAX_FD_FRAME_LENGTH = 64U;  // CAN FD
    static constexpr uint32_t INVALID_ID          = 0xFFFFFFFFU;

    // -------------------------------------------------------------------------
    // Filter base class — subclasses decide which frames a listener accepts
    // -------------------------------------------------------------------------
    class AbstractCANFrameFilter {
    public:
        virtual ~AbstractCANFrameFilter() = default;
        virtual bool acceptFrame(CANFrame const& frame) const = 0;
    };

    // Accepts every incoming frame — default for most listeners
    class AcceptAllFilter : public AbstractCANFrameFilter {
    public:
        bool acceptFrame(CANFrame const& /* frame */) const override {
            return true;
        }
    };

    // Accepts only frames whose ID matches exactly
    class ExactIdFilter : public AbstractCANFrameFilter {
    public:
        explicit ExactIdFilter(uint32_t id) : _id(id) {}
        bool acceptFrame(CANFrame const& frame) const override {
            return frame.getId() == _id;
        }
    private:
        uint32_t _id;
    };

    // Accepts frames whose ID falls within [minId, maxId]
    class RangeFilter : public AbstractCANFrameFilter {
    public:
        RangeFilter(uint32_t minId, uint32_t maxId)
            : _min(minId), _max(maxId) {}
        bool acceptFrame(CANFrame const& frame) const override {
            uint32_t const id = frame.getId();
            return (id >= _min) && (id <= _max);
        }
    private:
        uint32_t _min;
        uint32_t _max;
    };

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------
    CANFrame()
        : _id(INVALID_ID)
        , _payloadLength(0U)
        , _isFD(false)
        , _isBRS(false)
        , _isExtended(false) {
        (void)::memset(_payload, 0, sizeof(_payload));
    }

    // -------------------------------------------------------------------------
    // ID
    // -------------------------------------------------------------------------
    void     setId(uint32_t id)       { _id = id; }
    uint32_t getId()            const { return _id; }

    // -------------------------------------------------------------------------
    // Payload
    // -------------------------------------------------------------------------
    void set(uint8_t const* data, uint8_t length) {
        uint8_t const maxLen = _isFD ? MAX_FD_FRAME_LENGTH : MAX_FRAME_LENGTH;
        _payloadLength = (length > maxLen) ? maxLen : length;
        (void)::memcpy(_payload, data, _payloadLength);
    }

    uint8_t const* getPayload()       const { return _payload; }
    uint8_t        getPayloadLength() const { return _payloadLength; }

    // -------------------------------------------------------------------------
    // Frame type flags
    // -------------------------------------------------------------------------
    void setFD(bool fd)               { _isFD  = fd; }
    void setBRS(bool brs)             { _isBRS = brs; }
    void setExtended(bool ext)        { _isExtended = ext; }

    bool isFD()       const { return _isFD; }
    bool isBRS()      const { return _isBRS; }
    bool isExtended() const { return _isExtended; }

private:
    uint32_t _id;
    uint8_t  _payload[MAX_FD_FRAME_LENGTH];
    uint8_t  _payloadLength;
    bool     _isFD;
    bool     _isBRS;
    bool     _isExtended;
};

} // namespace can
