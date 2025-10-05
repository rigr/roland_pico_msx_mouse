#include <stdio.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "class/hid/hid_host.h" // für HID Host-Funktionen

// -----------------------------------------------------------------------------
// Callback: HID-Gerät (z. B. Maus) wurde erkannt
// -----------------------------------------------------------------------------
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const* desc_report, uint16_t desc_len)
{
    (void) desc_report;
    (void) desc_len;

    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    printf("HID device connected: addr=%u, instance=%u, VID=%04x, PID=%04x\n",
           dev_addr, instance, vid, pid);

    // ersten Report anfordern
    tuh_hid_receive_report(dev_addr, instance);
}

// -----------------------------------------------------------------------------
// Callback: HID-Gerät getrennt
// -----------------------------------------------------------------------------
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    printf("HID device disconnected: addr=%u, instance=%u\n", dev_addr, instance);
}

// -----------------------------------------------------------------------------
// Callback: HID-Report empfangen (z. B. Mausbewegung)
// -----------------------------------------------------------------------------
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const* report, uint16_t len)
{
    (void) len;

    // Cast auf vorhandene TinyUSB-Struktur
    hid_mouse_report_t const* mouse = (hid_mouse_report_t const*)report;

    printf("Mouse: buttons=%02x, x=%d, y=%d, wheel=%d\n",
           mouse->buttons, mouse->x, mouse->y, mouse->wheel);

    // Nächsten Report anfordern
    tuh_hid_receive_report(dev_addr, instance);
}

// -----------------------------------------------------------------------------
// Setup + Mainloop
// -----------------------------------------------------------------------------
int main()
{
    stdio_init_all();
    board_init();
    tusb_init();

    printf("TinyUSB HID Host Beispiel gestartet.\n");

    while (true) {
        tuh_task();  // USB Host Aufgaben
        sleep_ms(10);
    }

    return 0;
}
