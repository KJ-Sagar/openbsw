#!/usr/bin/env python3
# =============================================================================
# FILE    : vdk/can_stimulus.py
# PURPOSE : Python stimulus script for the Synopsys VDK CAN bus model.
#
#           Connects to the virtual FlexCAN_0 bus on TCP port 9000 and:
#             1. Sends a START command frame  (ID 0x200, data=[0x01])
#             2. Reads back the heartbeat reply from the ECU (ID 0x100)
#             3. Sends a STATUS request       (ID 0x200, data=[0x04])
#             4. Reads back the response frame (ID 0x101)
#             5. Sends a RESET command        (ID 0x200, data=[0x03])
#             6. Reports all received frames with timestamps
#
# Wire protocol: 16-byte struct matching VdkCanWireFrame in VdkCanTransceiver.cpp
#   Bytes 0-3 : CAN ID (uint32 little-endian)
#   Byte  4   : DLC  (0-8)
#   Byte  5   : Flags (bit0=FD, bit1=BRS, bit2=ExtID)
#   Bytes 6-7 : Padding (0x00)
#   Bytes 8-15: Data payload
#
# Usage:
#   # Make sure VDK is already running with s32k566_params.yaml
#   python3 vdk/can_stimulus.py [--host 127.0.0.1] [--port 9000]
# =============================================================================

import socket
import struct
import time
import argparse
import sys
from dataclasses import dataclass
from typing import Optional

# =============================================================================
# Frame structure
# =============================================================================
WIRE_FMT = "<IBB2s8s"   # id(4) + dlc(1) + flags(1) + pad(2) + data(8)
WIRE_SIZE = struct.calcsize(WIRE_FMT)  # Should be 16

@dataclass
class CanFrame:
    id:     int
    dlc:    int
    data:   bytes
    is_fd:  bool = False
    is_ext: bool = False

    def __str__(self) -> str:
        data_hex = self.data[:self.dlc].hex(" ").upper()
        return f"ID=0x{self.id:03X}  DLC={self.dlc}  Data=[{data_hex}]"


def pack_frame(frame: CanFrame) -> bytes:
    flags = 0
    if frame.is_fd:  flags |= 0x01
    if frame.is_ext: flags |= 0x04
    data = frame.data.ljust(8, b'\x00')[:8]
    return struct.pack(WIRE_FMT, frame.id, frame.dlc, flags, b'\x00\x00', data)


def unpack_frame(raw: bytes) -> CanFrame:
    id_, dlc, flags, _pad, data = struct.unpack(WIRE_FMT, raw)
    return CanFrame(
        id=id_, dlc=dlc, data=data,
        is_fd=(flags & 0x01) != 0,
        is_ext=(flags & 0x04) != 0,
    )


# =============================================================================
# VDK CAN bus client
# =============================================================================
class VdkCanBus:
    def __init__(self, host: str, port: int):
        self._host = host
        self._port = port
        self._sock: Optional[socket.socket] = None

    def connect(self) -> None:
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect((self._host, self._port))
        self._sock.settimeout(2.0)
        print(f"[VdkCanBus] Connected to {self._host}:{self._port}")

    def disconnect(self) -> None:
        if self._sock:
            self._sock.close()
            self._sock = None
        print("[VdkCanBus] Disconnected")

    def send(self, frame: CanFrame) -> None:
        assert self._sock is not None, "Not connected"
        raw = pack_frame(frame)
        self._sock.sendall(raw)
        print(f"  TX → {frame}")

    def receive(self, timeout_s: float = 1.0) -> Optional[CanFrame]:
        assert self._sock is not None, "Not connected"
        self._sock.settimeout(timeout_s)
        try:
            raw = self._sock.recv(WIRE_SIZE)
            if len(raw) == WIRE_SIZE:
                frame = unpack_frame(raw)
                print(f"  RX ← {frame}")
                return frame
        except socket.timeout:
            print("  RX ← (timeout — no frame received)")
        return None

    def receive_all(self, count: int, timeout_s: float = 2.0) -> list:
        frames = []
        deadline = time.monotonic() + timeout_s
        while len(frames) < count and time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            self._sock.settimeout(max(0.1, remaining))
            try:
                raw = self._sock.recv(WIRE_SIZE)
                if len(raw) == WIRE_SIZE:
                    frame = unpack_frame(raw)
                    print(f"  RX ← {frame}")
                    frames.append(frame)
            except socket.timeout:
                break
        return frames


# =============================================================================
# Test scenario
# =============================================================================
def run_scenario(bus: VdkCanBus) -> bool:
    passed = 0
    failed = 0

    def check(condition: bool, description: str) -> None:
        nonlocal passed, failed
        if condition:
            print(f"  ✓  PASS: {description}")
            passed += 1
        else:
            print(f"  ✗  FAIL: {description}")
            failed += 1

    print("\n" + "="*60)
    print(" STEP 1: Send START command (ID=0x200, data=[0x01])")
    print("="*60)
    bus.send(CanFrame(id=0x200, dlc=1, data=bytes([0x01])))
    time.sleep(0.05)

    # Expect response frame ID=0x101 with data[0]=0x01
    frame = bus.receive(timeout_s=1.0)
    check(frame is not None,           "Response frame received")
    check(frame is not None and frame.id == 0x101, "Response ID = 0x101")
    check(frame is not None and frame.data[0] == 0x01, "Response echoes START cmd")

    print("\n" + "="*60)
    print(" STEP 2: Wait for heartbeat (ID=0x100)")
    print("="*60)
    time.sleep(0.05)
    frame = bus.receive(timeout_s=1.5)
    check(frame is not None,            "Heartbeat received")
    check(frame is not None and frame.id == 0x100, "Heartbeat ID = 0x100")
    check(frame is not None and frame.dlc == 8,    "Heartbeat DLC = 8")
    check(frame is not None and frame.data[6] == 0xBE
          and frame.data[7] == 0xEF, "Magic bytes 0xBEEF in heartbeat")

    print("\n" + "="*60)
    print(" STEP 3: Send STATUS request (ID=0x200, data=[0x04])")
    print("="*60)
    bus.send(CanFrame(id=0x200, dlc=1, data=bytes([0x04])))
    frame = bus.receive(timeout_s=1.0)
    check(frame is not None,                       "Status response received")
    check(frame is not None and frame.id == 0x101, "Status response ID = 0x101")
    check(frame is not None and frame.data[0] == 0x04, "Response echoes STATUS cmd")

    print("\n" + "="*60)
    print(" STEP 4: Send RESET command (ID=0x200, data=[0x03])")
    print("="*60)
    bus.send(CanFrame(id=0x200, dlc=1, data=bytes([0x03])))
    frame = bus.receive(timeout_s=1.0)
    check(frame is not None,                       "Reset ACK received")
    check(frame is not None and frame.id == 0x101, "Reset ACK ID = 0x101")

    print("\n" + "="*60)
    print(f" RESULTS: {passed} passed, {failed} failed")
    print("="*60 + "\n")
    return failed == 0


# =============================================================================
# Entry point
# =============================================================================
def main() -> int:
    parser = argparse.ArgumentParser(
        description="CAN stimulus script for S32K566 VDK simulation")
    parser.add_argument("--host", default="127.0.0.1",
                        help="VDK CAN bus host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=9000,
                        help="VDK CAN bus port (default: 9000)")
    args = parser.parse_args()

    bus = VdkCanBus(args.host, args.port)

    try:
        bus.connect()
        success = run_scenario(bus)
    except ConnectionRefusedError:
        print(f"\n[ERROR] Cannot connect to {args.host}:{args.port}")
        print("  → Make sure the VDK simulation is running:")
        print("    virtualizer_exec --model S32K566_VDK.so \\")
        print("                     --elf build-vdk/s32k566_can_app.elf \\")
        print("                     --params vdk/s32k566_params.yaml")
        return 1
    except Exception as e:
        print(f"[ERROR] {e}")
        return 1
    finally:
        bus.disconnect()

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
