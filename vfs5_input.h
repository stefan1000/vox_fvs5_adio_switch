#pragma once
/*
   Detect channel select from a Vox VFS5 footswitch:
   Wired to an analog input and powered via the 3.3V pico output.
   
   VFS5: tip - 3.3V
   VFS5: ring - ADC0/1/2 (see vfs5_init)
   VFS5: shaft - ground

   "Bank" is implemented such that it switches between (Ch0...Ch3)/(Ch4...Ch7), could also be adjusted to 
   do any other switch logic, see detectChannel. 

   See e.g. https://elektrotanya.com/PREVIEWS/63463243/23432455/vox/vox_vsf5_foot_switch_sch.pdf_1.png
*/

// DMA buffer size/interval: doing 1000 samples in a 100ms interval seems to be ok
#define VFS5_BUFFER_SIZE 1000
#define VFS5_INTERVAL_MS  100

// Channel 0 is GPIO26
void vfs5_init(int captureChannel);

// triggers dma input, other work can be done between
void vfs5_process_start();
// wait for DMA to finish, returns the channel selected (0...7) or -1 if no switch was selected.
int vfs5_process_end();