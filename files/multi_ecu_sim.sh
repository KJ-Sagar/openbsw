#!/bin/bash
# =============================================================================
# FILE    : vdk/multi_ecu_sim.sh
# PURPOSE : Launch two virtual S32K566 ECUs on the same virtual CAN bus
#           using the Synopsys VDK.
#
#           ECU 1 — Zone Controller: runs the CAN send/receive app (build-vdk/)
#           ECU 2 — Sensor Node    : runs a second firmware image (sensor/)
#
#           A vdk_can_bridge process connects the two virtual buses so that
#           frames from ECU1 reach ECU2 and vice versa.
#
# Prerequisites:
#   - virtualizer_exec   in PATH  (from Synopsys Virtualizer installation)
#   - vdk_can_bridge     in PATH  (part of VDK network utilities)
#   - Both ELF files built
#
# Usage:
#   chmod +x vdk/multi_ecu_sim.sh
#   ./vdk/multi_ecu_sim.sh
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Paths — adjust to your environment
# ---------------------------------------------------------------------------
VDK_MODEL="/opt/synopsys/vdk/s32k566/S32K566_VDK.so"
ECU1_ELF="build-vdk/s32k566_can_app.elf"
ECU2_ELF="build-vdk/sensor_node.elf"         # Build a second image for this
ECU1_PARAMS="vdk/s32k566_params_ecu1.yaml"
ECU2_PARAMS="vdk/s32k566_params_ecu2.yaml"

# ---------------------------------------------------------------------------
# Check files exist
# ---------------------------------------------------------------------------
for f in "$VDK_MODEL" "$ECU1_ELF"; do
    if [[ ! -f "$f" ]]; then
        echo "[ERROR] Required file not found: $f"
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# Create per-ECU param files (port offset ECU2 by +100)
# ---------------------------------------------------------------------------
cat > "$ECU1_PARAMS" <<YAML
platform:
  clocks:
    FIRC_frequency_Hz: 48000000
    FXOSC_frequency_Hz: 16000000
    PLL_frequency_Hz: 200000000
  can:
    FlexCAN_0:
      enable: true
      virtual_bus_port: 9000
      loopback: false
  uart:
    LPUART_0:
      enable: true
      host_port: 3000
  debug:
    gdb_port: 2345
    halt_on_reset: false
YAML

cat > "$ECU2_PARAMS" <<YAML
platform:
  clocks:
    FIRC_frequency_Hz: 48000000
    FXOSC_frequency_Hz: 16000000
    PLL_frequency_Hz: 200000000
  can:
    FlexCAN_0:
      enable: true
      virtual_bus_port: 9001
      loopback: false
  uart:
    LPUART_0:
      enable: true
      host_port: 3001
  debug:
    gdb_port: 2346
    halt_on_reset: false
YAML

# ---------------------------------------------------------------------------
# Trap to kill background processes on Ctrl-C
# ---------------------------------------------------------------------------
cleanup() {
    echo ""
    echo "[multi_ecu_sim] Stopping all processes..."
    kill "$ECU1_PID" "$ECU2_PID" "$BRIDGE_PID" 2>/dev/null || true
    wait 2>/dev/null || true
    echo "[multi_ecu_sim] Done."
}
trap cleanup EXIT INT TERM

# ---------------------------------------------------------------------------
# Launch ECU 1
# ---------------------------------------------------------------------------
echo "[multi_ecu_sim] Starting ECU 1 (Zone Controller)..."
virtualizer_exec \
    --model   "$VDK_MODEL" \
    --elf     "$ECU1_ELF"  \
    --gdb-port 2345        \
    --params  "$ECU1_PARAMS" &
ECU1_PID=$!
echo "  PID=$ECU1_PID   GDB port 2345   UART console: nc localhost 3000"

sleep 1   # Give ECU1 time to open its CAN port

# ---------------------------------------------------------------------------
# Launch ECU 2 (if ELF exists)
# ---------------------------------------------------------------------------
ECU2_PID=0
BRIDGE_PID=0
if [[ -f "$ECU2_ELF" ]]; then
    echo "[multi_ecu_sim] Starting ECU 2 (Sensor Node)..."
    virtualizer_exec \
        --model   "$VDK_MODEL" \
        --elf     "$ECU2_ELF"  \
        --gdb-port 2346        \
        --params  "$ECU2_PARAMS" &
    ECU2_PID=$!
    echo "  PID=$ECU2_PID   GDB port 2346   UART console: nc localhost 3001"

    sleep 1

    # -----------------------------------------------------------------------
    # Bridge: connects ECU1 (port 9000) ↔ ECU2 (port 9001)
    # -----------------------------------------------------------------------
    echo "[multi_ecu_sim] Starting CAN bus bridge (9000 ↔ 9001)..."
    vdk_can_bridge --port-a 9000 --port-b 9001 &
    BRIDGE_PID=$!
    echo "  PID=$BRIDGE_PID"
else
    echo "[multi_ecu_sim] ECU 2 ELF not found — running ECU 1 only."
    echo "                 Build sensor_node and set ECU2_ELF to enable multi-ECU."
fi

# ---------------------------------------------------------------------------
# Usage instructions
# ---------------------------------------------------------------------------
echo ""
echo "================================================================"
echo " Multi-ECU simulation running"
echo "----------------------------------------------------------------"
echo " Debug ECU 1 : arm-none-eabi-gdb $ECU1_ELF"
echo "               (gdb) target remote localhost:2345"
echo " Debug ECU 2 : arm-none-eabi-gdb $ECU2_ELF"
echo "               (gdb) target remote localhost:2346"
echo " CAN stimulus: python3 vdk/can_stimulus.py --port 9000"
echo " UART ECU 1  : nc localhost 3000"
echo " UART ECU 2  : nc localhost 3001"
echo "================================================================"
echo " Press Ctrl-C to stop all processes."
echo "================================================================"

# ---------------------------------------------------------------------------
# Wait indefinitely (cleanup runs on EXIT)
# ---------------------------------------------------------------------------
wait "$ECU1_PID"
