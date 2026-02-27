// =============================================================================
// FILE    : app/src/can/CanApplication.cpp
// PURPOSE : Implementation of the top-level CAN application.
//           - Registers itself as a CAN listener
//           - Periodically transmits heartbeat and status frames
//           - Dispatches received command and config frames
// =============================================================================

#include "can/CanApplication.h"
#include <cstring>

namespace app {

// =============================================================================
// Constructor
// =============================================================================
CanApplication::CanApplication(::can::ICanTransceiver& transceiver)
    : _transceiver(transceiver) {}

// =============================================================================
// init() — must be called once at startup
// =============================================================================
void CanApplication::init() {
    // Initialise the underlying hardware / VDK CAN controller
    (void)_transceiver.init();

    // Register this object as a frame listener.
    // All received frames will be dispatched to canFrameReceived().
    _transceiver.addCANFrameListener(*this);

    _running = true;
}

// =============================================================================
// Periodic tasks
// =============================================================================

void CanApplication::task10ms() {
    if (!_running) { return; }
    ++_tick10ms;
    sendHeartbeat();
}

void CanApplication::task100ms() {
    if (!_running) { return; }
    // Send a fuller status frame every 100 ms
    sendResponse(0xAA);
}

// =============================================================================
// TX: Heartbeat (CAN ID 0x100, 8 bytes)
//
// Byte 0-1 : TX counter (little-endian)
// Byte 2-3 : RX counter (little-endian)
// Byte 4   : Status flags (bit 0 = running)
// Byte 5   : Tick (lower 8 bits of _tick10ms)
// Byte 6-7 : Fixed magic 0xBEEF for frame validation
// =============================================================================
void CanApplication::sendHeartbeat() {
    ::can::CANFrame frame;
    frame.setId(CanIds::TX_HEARTBEAT);

    uint8_t payload[8] = {};
    payload[0] = static_cast<uint8_t>(_txCount & 0xFFU);
    payload[1] = static_cast<uint8_t>((_txCount >> 8U) & 0xFFU);
    payload[2] = static_cast<uint8_t>(_rxCount & 0xFFU);
    payload[3] = static_cast<uint8_t>((_rxCount >> 8U) & 0xFFU);
    payload[4] = _running ? 0x01U : 0x00U;
    payload[5] = static_cast<uint8_t>(_tick10ms & 0xFFU);
    payload[6] = 0xBEU;
    payload[7] = 0xEFU;

    frame.set(payload, sizeof(payload));

    auto const result = _transceiver.write(frame);
    if (result == ::can::ICanTransceiver::ErrorCode::CAN_ERR_OK) {
        ++_txCount;
    }
}

// =============================================================================
// TX: Response frame (CAN ID 0x101)
//
// Byte 0 : Echo of command byte
// Byte 1 : Status byte passed in
// Byte 2-5 : TX counter (big-endian)
// Byte 6-7 : Reserved 0x00
// =============================================================================
void CanApplication::sendResponse(uint8_t statusByte) {
    ::can::CANFrame frame;
    frame.setId(CanIds::TX_RESPONSE);

    uint8_t payload[8] = {};
    payload[0] = statusByte;
    payload[1] = _running ? 0x01U : 0x00U;
    payload[2] = static_cast<uint8_t>((_txCount >> 24U) & 0xFFU);
    payload[3] = static_cast<uint8_t>((_txCount >> 16U) & 0xFFU);
    payload[4] = static_cast<uint8_t>((_txCount >> 8U)  & 0xFFU);
    payload[5] = static_cast<uint8_t>(_txCount & 0xFFU);
    payload[6] = 0x00U;
    payload[7] = 0x00U;

    frame.set(payload, sizeof(payload));

    auto const result = _transceiver.write(frame);
    if (result == ::can::ICanTransceiver::ErrorCode::CAN_ERR_OK) {
        ++_txCount;
    }
}

// =============================================================================
// RX: Dispatcher — called from transceiver interrupt or deferred context
// =============================================================================
void CanApplication::canFrameReceived(::can::CANFrame const& frame) {
    ++_rxCount;

    switch (frame.getId()) {
        case CanIds::RX_COMMAND:
            handleCommand(frame);
            break;
        case CanIds::RX_CONFIG:
            handleConfig(frame);
            break;
        default:
            // Unknown ID — ignore (filter is AcceptAll, so this is expected)
            break;
    }
}

// =============================================================================
// RX: Command handler (ID 0x200)
//
// Byte 0 : Command byte (see CanCmd namespace)
// =============================================================================
void CanApplication::handleCommand(::can::CANFrame const& frame) {
    if (frame.getPayloadLength() < 1U) { return; }

    uint8_t const cmd = frame.getPayload()[0];

    switch (cmd) {
        case CanCmd::START:
            _running = true;
            sendResponse(CanCmd::START);
            break;

        case CanCmd::STOP:
            _running = false;
            sendResponse(CanCmd::STOP);
            break;

        case CanCmd::RESET:
            _txCount  = 0U;
            _rxCount  = 0U;
            _tick10ms = 0U;
            _running  = true;
            sendResponse(CanCmd::RESET);
            break;

        case CanCmd::STATUS:
            sendResponse(CanCmd::STATUS);
            break;

        default:
            // Unknown command — send NACK (0xFF)
            sendResponse(0xFFU);
            break;
    }
}

// =============================================================================
// RX: Config handler (ID 0x201)
//
// Byte 0 : Config parameter ID
// Byte 1-7 : Parameter value (interpretation depends on parameter ID)
// =============================================================================
void CanApplication::handleConfig(::can::CANFrame const& frame) {
    if (frame.getPayloadLength() < 2U) { return; }

    // Placeholder: extend with actual configuration parameters
    uint8_t const paramId = frame.getPayload()[0];
    (void)paramId;

    sendResponse(0x55U);  // ACK config
}

// =============================================================================
// Filter accessor
// =============================================================================
::can::CANFrame::AbstractCANFrameFilter const& CanApplication::getFilter() {
    return _filter;  // AcceptAllFilter — receives every frame on the bus
}

} // namespace app
