// =============================================================================
// FILE    : bsp/vdk/VdkCanTransceiver.cpp
// PURPOSE : VDK stub — connects to the Synopsys VDK CAN bus model via a
//           POSIX TCP socket.  Frames are exchanged as a simple binary
//           struct:  [ uint32_t id | uint8_t dlc | uint8_t pad[3] | uint8_t data[8] ]
//
//           The VDK CAN bus model listens on TCP port 9000 (configurable in
//           s32k566_params.yaml).  Run the VDK before starting the application.
//
//           If VDK_LOOPBACK is defined the socket is bypassed and every
//           transmitted frame is immediately delivered back to all listeners
//           (useful for unit tests without a running VDK instance).
// =============================================================================

#include "VdkCanTransceiver.h"

#include <cstring>
#include <cstdio>

// Platform socket headers (Linux / macOS)
#ifndef VDK_LOOPBACK
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
#endif

// =============================================================================
// Wire protocol — 16-byte frame struct shared with VDK CAN bus model
// =============================================================================
struct VdkCanWireFrame {
    uint32_t id;        // CAN ID (standard 11-bit in lower bits)
    uint8_t  dlc;       // Data length (0-8)
    uint8_t  flags;     // bit 0 = FD, bit 1 = BRS, bit 2 = extended ID
    uint8_t  pad[2];    // Padding to align data
    uint8_t  data[8];   // Payload bytes
};                      // Total: 16 bytes

static_assert(sizeof(VdkCanWireFrame) == 16, "VdkCanWireFrame size mismatch");

// =============================================================================
// Constructor / Destructor
// =============================================================================
VdkCanTransceiver::VdkCanTransceiver() {
    (void)::memset(_listeners, 0, sizeof(_listeners));
}

VdkCanTransceiver::~VdkCanTransceiver() {
#ifndef VDK_LOOPBACK
    if (_sockFd >= 0) {
        ::close(_sockFd);
    }
#endif
}

// =============================================================================
// init() — connect to VDK CAN bus model socket
// =============================================================================
::can::ICanTransceiver::ErrorCode VdkCanTransceiver::init() {
    if (_initialised) { return ErrorCode::CAN_ERR_OK; }

#ifdef VDK_LOOPBACK
    // No socket needed — loopback mode
    _initialised = true;
    return ErrorCode::CAN_ERR_OK;
#else
    _sockFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_sockFd < 0) {
        ::perror("[VdkCanTransceiver] socket()");
        return ErrorCode::CAN_ERR_NOT_INIT;
    }

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(VDK_CAN_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (::connect(_sockFd, reinterpret_cast<struct sockaddr*>(&addr),
                  sizeof(addr)) < 0) {
        ::perror("[VdkCanTransceiver] connect() — is VDK running?");
        ::close(_sockFd);
        _sockFd = -1;
        return ErrorCode::CAN_ERR_NOT_INIT;
    }

    // Set socket to non-blocking for the poll() receive path
    int flags = ::fcntl(_sockFd, F_GETFL, 0);
    ::fcntl(_sockFd, F_SETFL, flags | O_NONBLOCK);

    ::printf("[VdkCanTransceiver] Connected to VDK CAN bus on port %u\n",
             VDK_CAN_PORT);
    _initialised = true;
    return ErrorCode::CAN_ERR_OK;
#endif
}

// =============================================================================
// write() — serialize frame and send to VDK
// =============================================================================
::can::ICanTransceiver::ErrorCode
VdkCanTransceiver::write(::can::CANFrame const& frame) {
    if (!_initialised) { return ErrorCode::CAN_ERR_NOT_INIT; }

    VdkCanWireFrame wf = {};
    wf.id  = frame.getId();
    wf.dlc = frame.getPayloadLength();
    wf.flags = (frame.isExtended() ? 0x04U : 0x00U) |
               (frame.isFD()       ? 0x01U : 0x00U) |
               (frame.isBRS()      ? 0x02U : 0x00U);
    (void)::memcpy(wf.data, frame.getPayload(), wf.dlc);

#ifdef VDK_LOOPBACK
    // Loopback: deliver frame immediately to all listeners
    if (_loopback) {
        dispatchToListeners(frame);
    }
    return ErrorCode::CAN_ERR_OK;
#else
    ssize_t sent = ::send(_sockFd,
                          reinterpret_cast<const char*>(&wf),
                          sizeof(wf), 0);
    if (sent != static_cast<ssize_t>(sizeof(wf))) {
        ::perror("[VdkCanTransceiver] send()");
        return ErrorCode::CAN_ERR_TX_BUSY;
    }
    return ErrorCode::CAN_ERR_OK;
#endif
}

// =============================================================================
// poll() — receive pending frames from VDK socket and dispatch to listeners
// =============================================================================
void VdkCanTransceiver::poll() {
    if (!_initialised) { return; }

#ifdef VDK_LOOPBACK
    // Nothing to poll in pure loopback mode
    return;
#else
    VdkCanWireFrame wf = {};
    ssize_t n = ::recv(_sockFd,
                       reinterpret_cast<char*>(&wf),
                       sizeof(wf), 0);

    while (n == static_cast<ssize_t>(sizeof(wf))) {
        ::can::CANFrame frame;
        frame.setId(wf.id);
        frame.setExtended((wf.flags & 0x04U) != 0U);
        frame.setFD((wf.flags & 0x01U) != 0U);
        frame.setBRS((wf.flags & 0x02U) != 0U);
        frame.set(wf.data, wf.dlc);

        dispatchToListeners(frame);

        n = ::recv(_sockFd,
                   reinterpret_cast<char*>(&wf),
                   sizeof(wf), 0);
    }
    // n == -1 with EAGAIN/EWOULDBLOCK means no more frames — normal
#endif
}

// =============================================================================
// Listener management
// =============================================================================
void VdkCanTransceiver::addCANFrameListener(::can::ICANFrameListener& l) {
    if (_listenerCount >= MAX_LISTENERS) { return; }
    for (uint8_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == &l) { return; }
    }
    _listeners[_listenerCount++] = &l;
}

void VdkCanTransceiver::removeCANFrameListener(::can::ICANFrameListener& l) {
    for (uint8_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] == &l) {
            _listeners[i] = _listeners[--_listenerCount];
            _listeners[_listenerCount] = nullptr;
            return;
        }
    }
}

void VdkCanTransceiver::setLoopback(bool enable) {
    _loopback = enable;
}

void VdkCanTransceiver::dispatchToListeners(::can::CANFrame const& frame) {
    for (uint8_t i = 0; i < _listenerCount; ++i) {
        if (_listeners[i] != nullptr &&
            _listeners[i]->getFilter().acceptFrame(frame)) {
            _listeners[i]->canFrameReceived(frame);
        }
    }
}
