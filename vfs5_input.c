#include "vfs5_input.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

uint dma_chan;
dma_channel_config cfg;

int captureChannel = 0;

uint8_t capture_buf[VFS5_BUFFER_SIZE];

/* when powering via 3.3 pin:
  240-243: Bank B
  211-214: Bank A
  171-175: Ch1    
  142-146: Ch2
  97-99:   Ch3
  60-63:   Ch4
*/
bool bankSelected = false;      // false: Bank A, true BankB
int channelSwitchSelected = -1; // Ch1:0, Ch2:1, Ch3:2, Ch4:3
int channelSelected = -1;       // combined channel: [0...7]

bool detectChannel()
{
    int vMax = 0;
    int vMin = 1000;
    for (int i = 0; i < VFS5_BUFFER_SIZE; ++i)
    {
        int c = capture_buf[i];
        vMax = (c > vMax) ? c : vMax;
        vMin = (c < vMin) ? c : vMin;
    }
    //printf("v = %d;%d\n", vMin, vMax);
 
    int switchSelected = -1;
    int eps = 2;
    if (vMin > (240 - eps))
    {
        bankSelected = true;
    }
    else if (vMin > (210 - eps))
    {
        bankSelected = false;
    }
    else if (vMin > (171 - eps)) // Ch1
    {
        switchSelected = 0;
    }
    else if (vMin > (142 - eps)) // Ch2
    {
        switchSelected = 1;
    }
    else if (vMin > (97 - eps)) // Ch3
    {
        switchSelected = 2;
    }
    else if (vMin > (60 - eps)) // Ch4
    {     
        switchSelected = 3;
    }

    // don't trigger channel change on bank selection
    if (switchSelected != -1)
    {
        int channelSelectedBefore = channelSelected;
        channelSwitchSelected = switchSelected;
        channelSelected = channelSwitchSelected + (bankSelected ? 4 : 0);
        return (channelSelected != channelSelectedBefore);
    }
    else 
    {
        return false;
    }

}

void vfs5_init(int captureChannelIn)
{
    captureChannel = captureChannelIn;

    // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
    adc_gpio_init(26 + captureChannel);

    adc_init();
    adc_select_input(captureChannel);
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // We won't see the ERR bit because of 8 bit reads; disable.
        true   // Shift each sample to 8 bits when pushing to FIFO
    );

    // Divisor of 0 -> full speed. Free-running capture with the divider is
    // equivalent to pressing the ADC_CS_START_ONCE button once per `div + 1`
    // cycles (div not necessarily an integer). Each conversion takes 96
    // cycles, so in general you want a divider of 0 (hold down the button
    // continuously) or > 95 (take samples less frequently than 96 cycle
    // intervals). This is all timed by the 48 MHz ADC clock.


    float totalTimeMS = (float)VFS5_INTERVAL_MS;
    float clockRate = 48000000.0f; // clocks / second;
    float clkdiv = (clockRate * totalTimeMS) / (1000.0f * (float)VFS5_BUFFER_SIZE);
    adc_set_clkdiv(clkdiv);

    // sleep_ms(1000);
    // Set up the DMA to start transferring data as soon as it appears in FIFO
    dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);
}

void vfs5_process_start()
{
    dma_channel_configure(dma_chan, &cfg,
                          capture_buf,   // dst
                          &adc_hw->fifo, // src
                          VFS5_BUFFER_SIZE, // transfer count
                          true           // start immediately
    );

    adc_run(true);
}

int vfs5_process_end()
{
    dma_channel_wait_for_finish_blocking(dma_chan);
    adc_run(false);
    adc_fifo_drain();

    if (detectChannel())
    {
    //#ifdef DEBUG
        // printf("footswitch ch=%d (chSel=%d bank=%s)\n", channelSelected, channelSwitchSelected, bankSelected ? "B" : "A");
    //#endif
        return channelSelected;
    }
    else
    {
        return -1;
    }
}