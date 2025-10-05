#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"
#include "class/hid/hid_host.h"   // <-- wichtig für tuh_hid_receive_report()
#include <cmath>             // <-- für roundf()

// --- Roland S-750 Mausport Mapping ---
// Roland Maus nutzt serielle Übertragung (ca. 1200–2400 Baud)
// Wir emulieren diese Signale über GPIO (z. B. GP2 = TX, GP3 = CLK)
// Achtung: Roland arbeitet mit 5 V Logik → Levelshifter empfohlen!
#define ROLAND_DATA_PIN 2
#define ROLAND_CLK_PIN 3

// Skalierungsfaktor der Mausbewegung (Empfindlichkeit)
#define MOVE_SCALE 0.5f

// USB-HID Maus Report Struktur
typedef struct TU_ATTR_PACKED
{
    uint8_t buttons;
    int8_t  x;
    int8_t  y;
    int8_t  wheel;
} hid_mouse_report_t;

static hid_mouse_report_t last_report = {0};

// --- Funktion zur Emulation der Roland-Mausübertragung ---
void roland_send(uint8_t data)
{
    // Einfaches serielles Protokoll (Startbit + 8 Datenbits + Stopbit)
    gpio_put(ROLAND_CLK_PIN, 0);
    sleep_us(100);
    gpio_put(ROLAND_DATA_PIN, 0); // Startbit
    sleep_us(200);

    for (int i = 0; i < 8; i++) {
        gpio_put(ROLAND_CLK_PIN, 0);
        gpio_put(ROLAND_DATA_PIN, (data >> i) & 1);
        sleep_us(200);
        gpio_put(ROLAND_CLK_PIN, 1);
        sleep_us(200);
    }

    gpio_put(ROLAND_CLK_PIN, 0);
    gpio_put(ROLAND_DATA_PIN, 1); // Stopbit
    sleep_us(200);
}

// --- HID Host Callback bei Geräteverbindung ---
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, const uint8_t* desc_report, uint16_t desc_len)
{
    (void) desc_report;
    (void) desc_len;
    printf("Maus verbunden (Gerät %d, Instanz %d)\n", dev_addr, instance);
    tuh_hid_receive_report(dev_addr, instance);
}

// --- HID Host Callback bei Geräteentfernung ---
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    printf("Maus getrennt (Gerät %d, Instanz %d)\n", dev_addr, instance);
}

// --- Callback bei empfangenem HID-Maus-Report ---
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len)
{
    (void) dev_addr;
    (void) instance;

    if (len < sizeof(hid_mouse_report_t)) return;

    hid_mouse_report_t const* mouse = (hid_mouse_report_t const*)report;

    int dx = mouse->x;
    int dy = mouse->y;

    // Mausbewegung in Roland-Format konvertieren
    int32_t sx = (int32_t)std::roundf((float)dx * MOVE_SCALE);
    int32_t sy = (int32_t)std::roundf((float)dy * MOVE_SCALE);

    // Hier würdest du das Roland-Mausprotokoll anwenden:
    // Beispielhaft senden wir ein 3-Byte-Paket: [Buttons, X, Y]
    roland_send(mouse->buttons);
    roland_send((uint8_t)sx);
    roland_send((uint8_t)sy);

    last_report = *mouse;

    tuh_hid_receive_report(dev_addr, instance);  // <-- wichtig: nächsten Report anfordern
}

// --- Hauptprogramm ---
int main()
{
    stdio_init_all();
    gpio_init(ROLAND_DATA_PIN);
    gpio_set_dir(ROLAND_DATA_PIN, GPIO_OUT);
    gpio_put(ROLAND_DATA_PIN, 1);

    gpio_init(ROLAND_CLK_PIN);
    gpio_set_dir(ROLAND_CLK_PIN, GPIO_OUT);
    gpio_put(ROLAND_CLK_PIN, 1);

    printf("Roland S-750 Maus-Emulator startet...\n");

    tusb_init();

    while (true) {
        tuh_task();  // TinyUSB Host stack aufrufen
        sleep_ms(5);
    }
}
