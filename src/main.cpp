/*
  roland_pico_msx_mouse
  - USB HID mouse (TinyUSB host) -> MSX (Roland MU-1 compatible) nibble emitter
  - Target: Raspberry Pi Pico (RP2040)
  - Minimal external hardware: BSS138 4-channel bidirectional level-shifter
    (HV side -> Roland DE-9 pins, LV side -> Pico GPIO), pull-ups on HV side to +5V.
  
  Verhalten:
  - TinyUSB liest USB mouse reports (typ. 3 bytes: buttons, dx, dy).
  - Bewegungen akkumulieren, skalieren und in signed 8-bit X/Y Delta umwandeln.
  - Bei Takt/Pin8-Wechsel vom Sampler: Pico liefert nacheinander die Nibbles
    (ID nibble + optional padding + XH XL YH YL), über Pins 1-4.
  - MSX-pins sind open-drain style: um 0 zu senden -> GPIO output low;
    um 1 (release) -> GPIO input (high-Z) so HV pullup pulls to +5V.
*/

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"
#include "pico/critical_section.h"

// ---------------------- Configuration: pins & parameters --------------------

// Map Pico GPIOs (LV side) to Roland DE-9 / MSX pins (HV side).
// Use a BSS138-style bidirectional level shifter between HV and LV.
// Data bits (pins 1..4 on Roland) -> we map bit0..bit3
static const uint DATA_PINS[4] = {2, 3, 4, 5}; // LV side Pico pins -> change as you wish

// Buttons (map to Roland pin6/pin7 if you want):
static const uint BUTTON1_PIN = 6; // left button emulation (connect to Roland pin6 via shifter)
static const uint BUTTON2_PIN = 7; // right button emulation (connect to Roland pin7 via shifter)

// Strobe/Clock from Roland (pin8): input to Pico (HV->LV via same shifter)
static const uint STROBE_PIN = 8; // LV side Pico pin for reading Roland's strobe (choose un-used GPIO)

// Other hardware pins
// If you need to power Pico from Roland +5V, wire to VSYS on Pico board (not handled in SW).

// Behavior tuning
static const float MOVE_SCALE = 1.0f;   // multiply raw mouse delta (tweak for comfortable cursor speed)
static const int16_t DELTA_SAT = 127;   // saturate delta to signed 8-bit range

// -------------------- Internal state (shared between IRQ and main) -----------
static volatile int32_t acc_x = 0;
static volatile int32_t acc_y = 0;
static volatile bool has_new_motion = false;

// nibble sequence buffer and state
static uint8_t nibble_seq[8];
static int nibble_len = 0;
static volatile int nibble_pos = 0;
static volatile bool sequence_active = false;

// critical section to protect preparing sequence (main thread) vs ISR reading
static critical_section_t seq_cs;

// -------------------- Utility: MSX nibble output primitives -----------------

// Drive a single data bit: follow open-drain convention:
// - To output bit=0: set GPIO as output and drive low
// - To output bit=1: set GPIO as input (hi-Z) so the external 5V pullup on Roland side pulls it high
static inline void set_data_bit(uint bit_index, bool bit_value) {
    uint gp = DATA_PINS[bit_index];
    if (bit_value) {
        // release line
        gpio_set_dir(gp, GPIO_IN);
        gpio_disable_pulls(gp);
    } else {
        // drive low
        gpio_set_dir(gp, GPIO_OUT);
        gpio_put(gp, 0);
    }
}

// Set 4-bit nibble on data pins (LSB -> DATA_PINS[0])
static void set_data_nibble(uint8_t nibble) {
    for (int i = 0; i < 4; ++i) {
        bool bit = (nibble >> i) & 1;
        set_data_bit(i, bit);
    }
}

// Release all data lines (hi-Z)
static void release_data_lines(void) {
    for (int i = 0; i < 4; ++i) {
        gpio_set_dir(DATA_PINS[i], GPIO_IN);
        gpio_disable_pulls(DATA_PINS[i]);
    }
}

// -------------------- TinyUSB HID host callbacks ----------------------------

// Called when a HID interface is mounted
extern "C" void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_report; (void)desc_len;
    // Start receiving reports for this interface
    tuh_hid_receive_report(dev_addr, instance);
    // We could check protocol here; for simplicity: accept HID mice reports
}

// Called when HID device is unmounted
extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr; (void)instance;
    // Reset accumulators
    acc_x = acc_y = 0;
    has_new_motion = false;
}

// Called when a HID report is received
extern "C" void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)dev_addr; (void)instance;
    // Typical mouse report: [buttons, dx, dy]  (signed 8-bit dx/dy)
    if (len >= 3) {
        int8_t dx = (int8_t)report[1];
        int8_t dy = (int8_t)report[2];
        // accumulate scaled
        int32_t sx = (int32_t)roundf((float)dx * MOVE_SCALE);
        int32_t sy = (int32_t)roundf((float)dy * MOVE_SCALE);

        // critical write (atomicish: volatile ints)
        acc_x += sx;
        acc_y += sy;
        has_new_motion = true;
    }
    // schedule next report (continuous streaming)
    tuh_hid_receive_report(dev_addr, instance);
}

// -------------------- MSX strobe IRQ (called on both edges) -----------------
// Each time STROBE toggles the host is reading the current nibble.
// We must place next nibble quickly so host reads correct data.
// This ISR must be minimal and deterministic.

static void gpio_strobe_callback(uint gpio, uint32_t events) {
    (void)events;
    if (gpio != STROBE_PIN) return;

    // If there's an active sequence, output current nibble; advance pointer.
    // Use critical section or rely on volatile atomic ops.
    if (sequence_active && nibble_len > 0) {
        uint8_t n;
        // load current nibble
        // nibble_pos may be updated by main thread but protected in critical region when prepared.
        n = nibble_seq[nibble_pos];
        set_data_nibble(n);
        // advance (wrap)
        nibble_pos = (nibble_pos + 1) % nibble_len;
    } else {
        // no active sequence -> release lines (mouse idle)
        release_data_lines();
    }
}

// -------------------- Prepare nibble sequence (main thread) -----------------
// We prepare a typical sequence that works with many MSX routines:
// [ ID nibble (0xB) , PAD(0xF), PAD(0xF), X_hi, X_lo, Y_hi, Y_lo ]
// This is conservative and compatible with many MSX BIOS / software checks.
// You can tweak the pads or shorten the sequence if desired.

static void prepare_sequence_for_xy(int8_t x, int8_t y) {
    critical_section_enter_blocking(&seq_cs);

    // convert signed to uint8 for nibble extraction
    uint8_t ux = (uint8_t)x;
    uint8_t uy = (uint8_t)y;

    nibble_seq[0] = 0xB;                // ID nibble = 1011b
    nibble_seq[1] = 0xF;                // pad
    nibble_seq[2] = 0xF;                // pad
    nibble_seq[3] = (ux >> 4) & 0x0F;   // X high nibble
    nibble_seq[4] = (ux) & 0x0F;        // X low nibble
    nibble_seq[5] = (uy >> 4) & 0x0F;   // Y high nibble
    nibble_seq[6] = (uy) & 0x0F;        // Y low nibble

    nibble_len = 7;
    nibble_pos = 0;
    sequence_active = true;

    // After preparing the sequence, set data lines to the first nibble right away
    // (so that if the sampler reads immediately, first nibble is present).
    set_data_nibble(nibble_seq[0]);

    critical_section_exit(&seq_cs);
}

// -------------------- Helper: clamp acc to signed 8-bit --------------------
static inline int8_t clamp_to_int8(int32_t v) {
    if (v > DELTA_SAT) v = DELTA_SAT;
    if (v < -DELTA_SAT) v = -DELTA_SAT;
    return (int8_t)v;
}

// -------------------- Main --------------------------------------------------
int main() {
    stdio_init_all();
    printf("roland_pico_msx_mouse starting...\n");

    // init critical section
    critical_section_init(&seq_cs);

    // initialize data pins as inputs (released)
    for (int i = 0; i < 4; ++i) {
        gpio_init(DATA_PINS[i]);
        gpio_set_dir(DATA_PINS[i], GPIO_IN);
        gpio_disable_pulls(DATA_PINS[i]);
    }

    // buttons (we implement them similarly: drive low for pressed, release for not)
    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_disable_pulls(BUTTON1_PIN);

    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_disable_pulls(BUTTON2_PIN);

    // strobe pin input:
    gpio_init(STROBE_PIN);
    gpio_set_dir(STROBE_PIN, GPIO_IN);
    gpio_disable_pulls(STROBE_PIN);

    // attach IRQ on both edges (fast)
    gpio_set_irq_enabled_with_callback(STROBE_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_strobe_callback);

    // init USB (TinyUSB)
    tusb_init();
    printf("TinyUSB init done. Waiting for mouse...\n");

    // main loop: run TinyUSB task + prepare nibble sequences when there's motion
    while (true) {
        tuh_task(); // TinyUSB host background handler

        // If we have new motion, compute deltas and prepare nibble sequence
        if (has_new_motion) {
            // atomically grab and reset accumulators
            int32_t x = 0, y = 0;
            // no lock but volatiles; small race tolerance acceptable
            x = acc_x;
            y = acc_y;
            acc_x = 0;
            acc_y = 0;
            has_new_motion = false;

            // clamp to int8 range
            int8_t sx = clamp_to_int8(x);
            int8_t sy = clamp_to_int8(y);

            // If no motion (rare), just continue
            if (sx == 0 && sy == 0) {
                // keep sequence active? we can keep last value; do nothing
            } else {
                // prepare nibble sequence for the new motion
                prepare_sequence_for_xy(sx, sy);
                // optionally debug
                printf("Prepared sequence for X=%d Y=%d\n", sx, sy);
            }
        }

        // small sleep to yield CPU (tuh_task already handles USB)
        sleep_ms(2);
    }

    return 0;
}
