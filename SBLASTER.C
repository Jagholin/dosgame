#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <math.h>
#include <pc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/farptr.h>

#include "DEFS.H"

static int base_io = -1, irq = -1, dma8 = -1, dma16 = -1, mixer_io = -1,
           mpu_io = -1;

volatile int interrupt_counter = 0;
const unsigned int sample_rate = 44100;
int realmem_selector = 0;
int realmem_segment = 0;
int realmem_buffer_start = 0;
int realmem_offset = 0;
_go32_dpmi_seginfo old_interrupt_seginfo, my_interrupt_seginfo;

// Interrupt routine when the SB is done with the chunk
void sb_intr() {
  int mixer_io = base_io + 4;
  OUT(mixer_io, 0x82);
  int test_intr = INP(mixer_io + 1);
  if ((test_intr & 2) == 0) // Not our interrupt
    return;

  interrupt_counter++;
  // Acknoledge interrupt
  INP(base_io + 0xf);
  if (irq >= 8)
    OUT(0xA0, 0x20);
  else
    OUT(0x20, 0x20);
  // enable();
  asm("sti");
}

void init_sb_intr() {
  // 1. Allocate 128 kB for the page aligned block of memory in the first MB
  realmem_segment = __dpmi_allocate_dos_memory(8192, &realmem_selector);
  if (realmem_segment == -1) {
    printf("Can't allocate buffer in the first 1MiB of memory\n");
    exit(EXIT_FAILURE);
  }
  // Calculate position of 64kB aligned chunk in the allocated memory
  int linear_address = realmem_segment << 4;
  realmem_buffer_start = ((linear_address & 0xFFFF) == 0)
                             ? linear_address // It's already page aligned
                             : (linear_address & (0xFFFF0000)) + 0x10000;
  realmem_offset = realmem_buffer_start - linear_address;
  printf("Allocated realmem segment is %x \n", realmem_segment);
  printf("Its linear address is %x \n", linear_address);
  printf("64k aligned buffer starts at %x\n", realmem_buffer_start);
  printf("Offset %x\n", realmem_offset);
  // Calculate interrupt vector based on IRQ
  short int_vec = 0x08 + irq;
  if (irq >= 8)
    int_vec = 0x70 + irq - 8;

  // The following is based on https://delorie.com/djgpp/v2faq/faq18_9.html
  int res = _go32_dpmi_get_protected_mode_interrupt_vector(
      int_vec, &old_interrupt_seginfo);
  if (res != 0) {
    printf("Can't get protected mode interrupt vector\n");
    exit(EXIT_FAILURE);
  }
  // This is a recording demo
  my_interrupt_seginfo.pm_offset = (long int)&sb_intr;
  my_interrupt_seginfo.pm_selector = _go32_my_cs();
  res = _go32_dpmi_chain_protected_mode_interrupt_vector(int_vec,
                                                         &my_interrupt_seginfo);

  if (res != 0) {
    printf("Can't allocate iret wrapper for my interrupt routine\n");
    exit(EXIT_FAILURE);
  }

  // TODO: Lock memory that the interrupt uses

  // Unmask interrupt
  short pic_port = irq >= 8 ? 0xA1 : 0x21;
  short mask_bit = irq >= 8 ? irq - 8 : irq;
  short mask = INP(pic_port) & ~(1 << mask_bit);
  OUT(pic_port, mask);
}

void release_sb_intr() {
  // Calculate interrupt vector based on IRQ
  short int_vec = 0x08 + irq;
  if (irq >= 8)
    int_vec = 0x70 + irq - 8;
  _go32_dpmi_set_protected_mode_interrupt_vector(int_vec,
                                                 &old_interrupt_seginfo);
  __dpmi_free_dos_memory(realmem_selector);
}

// General procedures for DMA transfer:
// 1. set up interrupt routine
// 2. program DMA controller
// 3. program DSP sampling rate
// 4. program DSP with DMA transfer mode and length to start the transfer
// 5. service interrupts
// 6. restore interrupt routine
//
// 16 bit autoinit transfer:
// 1. Allocate DMA buffer at 64kB page boundary
// 2. Setup interrupt service
// 3. program DMA for 16bit autoinit transfer
// 4. set DSP transfer sample rate
//   . out(0xc, 0x41)
//   . out(0xc, HIGHBYTE(samplingrate))
//   . out(0xc, LOWBYTE(samplingrate))
// 5. send IO command, followed by transfer mode, and DSP block transfer size
//   . out(0xc, 0xb6) //16 bit output
//   . out(0xc, 0x10) //16 bit mono signed PCM
//   . out(0xc, HIGHBYTE(blksize))
//   . out(0xc, LOWBYTE(blksize))

// SB DMA routines
void init_sb_dma() {
  int ints_were_enabled = disable();

  const unsigned short start_ports[] = {0x00, 0x02, 0x04, 0x06,
                                        0xC0, 0xC4, 0xC8, 0xCC};
  const unsigned short count_ports[] = {0x01, 0x03, 0x05, 0x07,
                                        0xC2, 0xC6, 0xCA, 0xCE};
  const unsigned short mask_port[] = {0x0A, 0x0A, 0x0A, 0x0A,
                                      0xD4, 0xD4, 0xD4, 0xD4};
  const unsigned short fflop_port[] = {0x0C, 0x0c, 0x0c, 0x0c,
                                       0xd8, 0xd8, 0xd8, 0xd8};
  const unsigned short pageaddr_port[] = {0x87, 0x83, 0x81, 0x82,
                                          0x8F, 0x8B, 0x89, 0x8A};
  const unsigned short mode_port[] = {0x0B, 0x0B, 0x0B, 0x0B,
                                      0xD6, 0xD6, 0xD6, 0xD6};

  const uint8 mask = (dma16 & 0x3);
  // Mask DMA channel
  OUT(mask_port[dma16], mask | 0x04);
  // Reset flipflop
  OUT(fflop_port[dma16], 0xFF);
  // Send lowbyte then high byte of address buffer
  printf("sending lowbyte of address to dma: %x\n",
         LOWBYTE(realmem_buffer_start));
  OUT(start_ports[dma16], LOWBYTE(realmem_buffer_start));
  //  OUT(start_ports[dma16], 0xff);
  printf("sending hybyte of address to dma: %x\n",
         HIBYTE(realmem_buffer_start));
  OUT(start_ports[dma16], HIBYTE(realmem_buffer_start));
  //  send length of the buffer to the count registers
  OUT(count_ports[dma16], 0xFF);
  OUT(count_ports[dma16], 0xFF);
  // send page number to the pageaddr port
  printf("sending page buffer to dma: %x\n",
         (realmem_buffer_start & 0xFF0000) >> 16);
  OUT(pageaddr_port[dma16], ((realmem_buffer_start & 0xFF0000) >> 16));
  // Set DMA mode(read)
  // single DMA transfer, auto, read.
  OUT(mode_port[dma16], 0b01011000 | (dma16 & 0x3));
  // Unmask DMA channel
  OUT(mask_port[dma16], mask);

  if (ints_were_enabled) {
    enable();
  }
}

void dsp_write(unsigned short port_offset, uint8 data) {
  // first read status to check if DSP accepts data
  unsigned short port = port_offset + base_io;
  while (INP(port) & 0x80)
    ;
  OUT(port, data);
}

uint8 dsp_read(unsigned short port_offset) {
  // first read status to check if DSP accepts data
  unsigned short port = port_offset + base_io;
  while ((INP(port) & 0x80) == 0)
    ;
  return INP(port);
}

void release_sb_dma() {}

void write_test_data() {
  // Let's write some test data that will output
  float freq = 1000;
  float amplitude = 30000;
  float step_size = 6.28 * freq / sample_rate;
  int sample_count = 32 * 1024;
  float x = 0;
  unsigned short old_selector = _fargetsel();
  _farsetsel(realmem_selector);
  //_farsetsel(_dos_ds);
  printf("Testdata realmem offset is %x\n", realmem_offset);
  for (int i = 0; i < sample_count * 2; ++i) {
    float next_sample = amplitude * sinf(x);
    x += step_size;
    int16 my_sample = (int16)next_sample;
    // uint16 s = *(uint16 *)((void *)&my_sample);
    _farnspokew(realmem_offset + i * 2, my_sample);
    //_farnspokew(0x10000 + i * 2, my_sample);
  }
  _farsetsel(old_selector);
}

// 4. set DSP transfer sample rate
//   . out(0xc, 0x41)
//   . out(0xc, HIGHBYTE(samplingrate))
//   . out(0xc, LOWBYTE(samplingrate))
// 5. send IO command, followed by transfer mode, and DSP block transfer size
//   . out(0xc, 0xb6) //16 bit output
//   . out(0xc, 0x10) //16 bit mono signed PCM
//   . out(0xc, HIGHBYTE(blksize))
//   . out(0xc, LOWBYTE(blksize))
void test_sound() {
  write_test_data();
  // return;

  printf("Initializing DMA");
  init_sb_dma();

  printf("Initializing sampling rate 44.1khz. HIBYTE %x , LOWBYTE %x \n",
         HIBYTE(sample_rate), LOWBYTE(sample_rate));
  dsp_write(0x0C, 0x41);
  dsp_write(0x0C, HIBYTE(sample_rate));
  dsp_write(0x0C, LOWBYTE(sample_rate));

  // Start Sound IO operation
  dsp_write(0x0C, 0xB6);
  dsp_write(0x0C, 0x10);
  printf("Sending lowbyte %x, highbyte %x \n", LOWBYTE(32 * 1024 - 1),
         HIBYTE(32 * 1024 - 1));
  dsp_write(0x0C, LOWBYTE(32 * 1024 - 1));
  dsp_write(0x0C, HIBYTE(32 * 1024 - 1));
}

int get_interrupt_counter() { return interrupt_counter; }

// Initializes soundblaster's DSP
RESULT init_dsp() {
  RESULT res = RESULT_ERR;
  unsigned int i;
  OUT(base_io + 0x6, 1);
  // wait for some number of cycles
  for (i = 0; i < 100; ++i) {
  }
  OUT(base_io + 0x6, 0);
  for (i = 0; i < 1000; ++i) {
    unsigned int data = INP(base_io + 0xe);
    if (data & 0x80) {
      data = INP(base_io + 0xa);
      if (data == 0xaa) {
        printf("Successful init after %d loops\n", i);
        res = RESULT_OK;
        break;
      }
    }
  }
  return res;
}

RESULT read_env_string() {
  char *blaster_env = getenv("BLASTER");
  char *blaster_envc = NULL;
  char *env_tok = NULL;
  // int res = 0;
  // bool initres = false;
  CHECKERR(blaster_env == NULL, "BLASTER environment variable isn't defined");

  blaster_envc = (char *)malloc(strlen(blaster_env) + 1);
  OOMERR(blaster_envc);
  strcpy(blaster_envc, blaster_env);

  env_tok = strtok(blaster_envc, " ");
  while (env_tok != NULL) {
    int *selected_num = NULL;
    switch (env_tok[0]) {
    case 'A':
    case 'a':
      selected_num = &base_io;
      break;
    case 'I':
    case 'i':
      selected_num = &irq;
      break;
    case 'D':
    case 'd':
      selected_num = &dma8;
      break;
    case 'H':
    case 'h':
      selected_num = &dma16;
      break;
    case 'M':
    case 'm':
      selected_num = &mixer_io;
      break;
    case 'P':
    case 'p':
      selected_num = &mpu_io;
      break;
    }
    if (selected_num != NULL) {
      if (env_tok[0] == 'A' || env_tok[0] == 'a')
        sscanf(&env_tok[1], "%x", &base_io);
      else
        *selected_num = atoi(&env_tok[1]);
    }
    env_tok = strtok(NULL, " ");
  }
  free(blaster_envc);
  return RESULT_OK;
onerror:
  if (blaster_envc)
    free(blaster_envc);
  return RESULT_ERR;
}

RESULT init_blaster() {
  RESULT result = RESULT_OK;
  CHECKRESULT(read_env_string());
  if (base_io == -1 || irq == -1 || dma8 == -1 || dma16 == -1) {
    result = RESULT_ERR;
    goto onerror;
  }
  CHECKRESULT(init_dsp());
  init_sb_intr();
  return RESULT_OK;
onerror:
  return result;
}

void release_blaster() {
  // reset the DSP
  init_dsp();
}
