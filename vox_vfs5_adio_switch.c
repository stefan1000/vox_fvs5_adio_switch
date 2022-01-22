/*

   Use pico to drive an Vox Adio Air GT via Vox VFS5 footswitch using MIDI-USB

   Requires the tinyhost midi usb host support (at the time of writing not in rasp sdk), see e.g.
   https://github.com/rppicomidi/midi2usbhost
   https://github.com/rppicomidi/tinyusb

   Can easily be adopted to e.g. send prg change from VFS5 to any classical/USB-midi device.
*/
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "bsp/board.h"
#include "tusb.h"
#include "class/midi/midi_host.h"
#include "vfs5_input.h"

const uint LED_GPIO = 25;
static uint8_t midi_dev_addr = 0;

static void poll_usb_rx(bool connected)
{
    // device must be attached and have at least one endpoint ready to receive a message
    if (!connected || tuh_midih_get_num_rx_cables(midi_dev_addr) < 1)
    {
        return;
    }
    tuh_midi_read_poll(midi_dev_addr);
}

int main()
{
    bi_decl(bi_program_description("Use Vox VFS5 footswitch to control Adio Air GT"));

    board_init();
    printf("VoxAdio AIR GT V5FS switcher\r\n");
    tusb_init();

    // Map the pins to functions
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);
    gpio_put(LED_GPIO, true);
    sleep_ms(250);
    gpio_put(LED_GPIO, false);
    vfs5_init(2);

    while (1)
    {
        vfs5_process_start(); // begin DMA transfer

        tuh_task();

        bool connected = midi_dev_addr != 0 && tuh_midi_configured(midi_dev_addr);

        gpio_put(LED_GPIO, connected);
        int channelSelected = vfs5_process_end(); // wait for DMA transfer (ADC input) finished.
        if (channelSelected != -1)
        {
            if (connected)
            {
                gpio_put(LED_GPIO, false); // small LED flash when fs is triggered...

                // send sysex for mode change:
                // Adio GT/BS, Adio Air GT/BS MIDI Implementation    Revision 1.00 (17 May 2017)
                /*
                (14) MODE CHANGE                                                       R , T
                +-------------------+-------------------------------------------------------+
                |      Byte         |                   Description                         |
                +-------------------+-------------------------------------------------------+
                | F0,42,30,00,01,41 | Exclusive Header                                      |
                | 4E                | Function Code                                         |
                | 0000 00mm         | User(0)/Manual(2)                                     |
                | 0000 0ppp         | Program No.                                           |
                | F7                | End of Exclusive                                      |
                +-------------------+-------------------------------------------------------+
                */
                uint8_t buffer[] = {0xF0, 0x42, 0x30, 0x00, 0x01, 0x41, 0x4e, 0x00, (uint8_t)(channelSelected), 0xF7};
                uint8_t written = tuh_midi_stream_write(midi_dev_addr, 0, buffer, sizeof(buffer));
                printf("channelSelected %d, sending change (written:%d)\n", channelSelected, written);
            }
        }

        if (connected)
            tuh_midi_stream_flush(midi_dev_addr);

        poll_usb_rx(connected);
    }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
    printf("MIDI device address = %u, IN endpoint %u has %u cables, OUT endpoint %u has %u cables\r\n",
           dev_addr, in_ep & 0xf, num_cables_rx, out_ep & 0xf, num_cables_tx);

    midi_dev_addr = dev_addr;
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    midi_dev_addr = 0;
    printf("MIDI device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}
