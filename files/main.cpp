// =============================================================================
// FILE    : app/src/main.cpp
// PURPOSE : Application entry point for the S32K566 CAN send/receive demo.
//
//           Instantiates the FlexCAN transceiver and CanApplication, then
//           runs a simple super-loop that calls the periodic tasks.
//
//           For RTOS-based projects: replace the super-loop with OS tasks
//           and call task10ms() / task100ms() from a 10 ms / 100 ms timer.
// =============================================================================

#include "can/CanApplication.h"
#include "S32K5FlexCanTransceiver.h"
#include <cstdint>

// =============================================================================
// Crude busy-wait delay
// Adjust the loop count to match the actual CPU frequency (200 MHz assumed).
// Replace with a hardware timer in production.
// =============================================================================
static void delay_ms(uint32_t ms) {
    // At 200 MHz: ~200 000 cycles per ms; each loop iteration ≈ 4 cycles
    volatile uint32_t count = ms * 50000UL;
    while (count--) {}
}

// =============================================================================
// Tick counters — driven from the super-loop
// =============================================================================
static uint32_t s_tick10ms  = 0U;
static uint32_t s_tick100ms = 0U;

// =============================================================================
// main()
// =============================================================================
int main(void) {
    // -------------------------------------------------------------------------
    // Instantiate hardware transceiver and application
    // -------------------------------------------------------------------------
    static S32K5FlexCanTransceiver transceiver;
    static app::CanApplication     canApp(transceiver);

    // Optional: enable loopback for standalone VDK test (no second ECU needed)
    // transceiver.setLoopback(true);

    // Initialise: configures FlexCAN_0, registers listener, enables IRQ
    canApp.init();

    // -------------------------------------------------------------------------
    // Super-loop
    // -------------------------------------------------------------------------
    for (;;) {
        delay_ms(10U);

        ++s_tick10ms;

        // 10 ms task — sends heartbeat frame (ID 0x100)
        canApp.task10ms();

        // 100 ms task — sends full status frame (ID 0x101)
        if (s_tick10ms % 10U == 0U) {
            ++s_tick100ms;
            canApp.task100ms();
        }
    }
}
