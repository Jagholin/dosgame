
/*
 * Adopted from https://github.com/Franticware/dos-audio-streaming-djgpp
 * Modified for a better fit into the codebase.
 * The original file is licensed CC0
 *
 * Original file header follows
 */

/*
 * Play and record digitized sound sample on soundblaster DAC/ADC using DMA.
 * This source code is in the public domain.
 *
 * Modification History
 *
 *  9-Nov-93    David Baggett       Wrote it based on Sound Blaster
 *              <dmb@ai.mit.edu>    Freedom project and Linux code.
 *
 *  24-Jun-94   Gerhard Kordmann    - modified for recording facilities
 *                                  - added keyboard safety-routines to
 *                                    allow stopping of playing/recording
 *                                  - removed click while buffer switch
 *                                  - added bugfixes by Grzegorz Jablonski
 *                                    and several safety checks
 *                                  - added free dosmem at end of program
 *              <grzegorz@kmm-lx.p.lod.edu.pl>

 *  08-Jul-94   Gerhard Kordmann    - click also removed in recording
 *                                  - changes from dosmem... to memcpy
 *  03-Sep-94   Gerhard Kordmann    - added Highspeed DMA for frequencies
 *                                    above 37000 Hz (also in sb.h)
 *                                  - removed memcpy due to new dpmi-style
 *                                    of djgpp
 *              <kordmann@ldv01.Uni-Trier.de>
 *  27-Dec-2023 Vojtech Salajka     - added streaming playback
 *                                  - removed fixed playback and recording
 */

#include "sb.h"
#include "defs.h"
#include <dos.h>
#include <pc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define sb_writedac(x)                                                         \
  {                                                                            \
    while (inportb(sb_ioaddr + SB_DSP_WRITE_STATUS) & 0x80)                    \
      ;                                                                        \
    outportb(sb_ioaddr + SB_DSP_WRITE_DATA, (x));                              \
  }

#define sb_writemixer(x, y)                                                    \
  {                                                                            \
    outportb(sb_ioaddr + SB_MIXER_ADDRESS, (x));                               \
    outportb(sb_ioaddr + SB_MIXER_DATA, (y));                                  \
  }

/*  GO32 DPMI structs for accessing DOS memory. */
static _go32_dpmi_seginfo dosmem; /* DOS (conventional) memory buffer */

static _go32_dpmi_seginfo oldirq_rm; /* original real mode IRQ */
static _go32_dpmi_registers rm_regs;
static _go32_dpmi_seginfo rm_si; /* real mode interrupt segment info */

static _go32_dpmi_seginfo oldirq_pm; /* original prot-mode IRQ */
static _go32_dpmi_seginfo pm_si;     /* prot-mode interrupt segment info */

/*  Card parameters  */
int sb_ioaddr;
int sb_irq;
int sb_dmachan;
int sb_dmachan16;

/* Is a sound currently playing or recorded ? */
volatile int sb_dma_active = 0;

/* Conventional memory buffers for DMA. */
static volatile int sb_bufnum = 0;
static char *sb_buf[2];
static unsigned int sb_buflen[2];

/* Info about current sample */
static int HIGHSPEED; /* flag for normal/highspeed DMA */

/* DMA chunk size, in bytes.
 * This parameter determines how big our DMA buffers are.  We play
 * the sample by piecing together chunks that are this big.  This
 * means that we don't have to copy the entire sample down into
 * conventional memory before playing it.  (A nice side effect of
 * this is that we can play samples that are longer than 64K.)
 *
 * Setting this is tricky.  If it's too small, we'll get lots
 * of interrupts, and slower machines might not be able to keep
 * up.  Furthermore, the smaller this is, the more grainy the
 * sound will come out.
 *
 * On the other hand, if we make it too big there will be a noticeable
 * delay between a call to sb_play and when the sound actually starts
 * playing, which is unacceptable for things like games where sound
 * effects should be "instantaneous".
 *
 */
#define DMA_CHUNK (2048)

static char sb_stream_buf[DMA_CHUNK];
static char sb_stream_silence[DMA_CHUNK];
static volatile char sb_stream_ready = 0;

void sb_intr_play(_go32_dpmi_registers *reg) {
  register unsigned n = sb_bufnum; /* buffer we just played	*/

  inportb(sb_ioaddr + SB_DSP_DATA_AVAIL); /* Acknowledge soundblaster */

  sb_play_buffer(1 - n); /* Start next buffer player */

  sb_fill_buffer(n); /* Fill this buffer for next time around */

  outportb(0x20, 0x20); /* Acknowledge the interrupt */

  enable();
}

/* Fill buffer n with the next data. */
void sb_fill_buffer(register unsigned n) {
  if (sb_stream_ready) {
    sb_buflen[n] = DMA_CHUNK;
    dosmemput(sb_stream_buf, DMA_CHUNK, (unsigned long)sb_buf[n]);
    sb_stream_ready = 0;
  } else {
    sb_buflen[n] = DMA_CHUNK;
    dosmemput(sb_stream_silence, DMA_CHUNK, (unsigned long)sb_buf[n]);
  }
}

void sb_play_buffer(register unsigned n) {
  int t;
  unsigned char im, tm;
  if (sb_buflen[n] <= 0) { /* See if we're already done */
    sb_dma_active = 0;
    return;
  }
  int interrupt_state = disable();

  im = inportb(0x21); /* Enable interrupts on PIC */
  tm = ~(1 << sb_irq);
  outportb(0x21, im & tm);

  outportb(SB_DMA_MASK, 5); /* Set DMA mode 'play' */
  outportb(SB_DMA_FF, 0);
  outportb(SB_DMA_MODE, 0x49);

  sb_bufnum = n; /* Set transfer address */
  t = (int)((unsigned long)sb_buf[n] >> 16);
  outportb(SB_DMAPAGE + 3, t);
  t = (int)((unsigned long)sb_buf[n] & 0xFFFF);
  outportb(SB_DMA + 2 * sb_dmachan, t & 0xFF);
  outportb(SB_DMA + 2 * sb_dmachan, t >> 8);
  /* Set transfer length byte count */
  outportb(SB_DMA + 2 * sb_dmachan + 1, (sb_buflen[n] - 1) & 0xFF);
  outportb(SB_DMA + 2 * sb_dmachan + 1, (sb_buflen[n] - 1) >> 8);

  outportb(SB_DMA_MASK, sb_dmachan); /* Unmask DMA channel */

  // We don't want to enable interrupts early
  if (interrupt_state)
    enable();

  if (HIGHSPEED) {
    sb_writedac(SB_SET_BLOCKSIZE); /* prepare block programming */
  } else {
    sb_writedac(SB_DMA_8_BIT_DAC); /* command byte for DMA DAC transfer */
  }

  sb_writedac((sb_buflen[n] - 1) & 0xFF); /* sb_write length */
  sb_writedac((sb_buflen[n] - 1) >> 8);

  if (HIGHSPEED)
    sb_writedac(SB_HIGH_DMA_8_BIT_DAC); /* command byte for high speed DMA DAC
                                           transfer */

  sb_dma_active = 1; /* A sound is playing now. */
}

/* Set sampling/playback rate.
 * Parameter is rate in Hz (samples per second).
 */
void sb_set_sample_rate(unsigned int rate) {
  unsigned char tc;

  if (rate > 37000)
    HIGHSPEED = 1;
  else
    HIGHSPEED = 0;
  if (HIGHSPEED)
    tc = (unsigned char)((65536 - 256000000 / rate) >> 8);
  else
    tc = (unsigned char)(256 - 1000000 / rate);

  sb_writedac(SB_TIME_CONSTANT); /* Command byte for sample rate */
  sb_writedac(tc);               /* Sample rate time constant */
}

void sb_voice(int state) {
  sb_writedac(state ? SB_SPEAKER_ON : SB_SPEAKER_OFF);
}

int sb_initcard() {
  // just do the reset
  sb_reset();
  return sb_ioaddr;
}

RESULT sb_getparams() {
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
      selected_num = &sb_ioaddr;
      break;
    case 'I':
    case 'i':
      selected_num = &sb_irq;
      break;
    case 'D':
    case 'd':
      selected_num = &sb_dmachan;
      break;
    case 'H':
    case 'h':
      selected_num = &sb_dmachan16;
      break;
    case 'M':
    case 'm':
      // selected_num = &mixer_io;
      break;
    case 'P':
    case 'p':
      // selected_num = &mpu_io;
      break;
    }
    if (selected_num != NULL) {
      if (env_tok[0] == 'A' || env_tok[0] == 'a')
        sscanf(&env_tok[1], "%x", &sb_ioaddr);
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

void sb_reset() {
  // TODO: return res
  RESULT res = RESULT_ERR;
  unsigned int i;
  OUT(sb_ioaddr + 0x6, 1);
  // wait for some number of cycles
  for (i = 0; i < 100; ++i) {
  }
  OUT(sb_ioaddr + SB_DSP_RESET, 0);
  for (i = 0; i < 1000; ++i) {
    unsigned int data = INP(sb_ioaddr + SB_DSP_DATA_AVAIL);
    if (data & 0x80) {
      data = INP(sb_ioaddr + SB_DSP_READ_DATA);
      if (data == 0xaa) {
        printf("Successful init after %d loops\n", i);
        res = RESULT_OK;
        break;
      }
    }
  }
  // return res;
}

unsigned char sb_read_dac() {
  for (;;) {
    if (inportb(sb_ioaddr + SB_DSP_DATA_AVAIL) & 0x080)
      break;
  }
  return (inportb(sb_ioaddr + SB_DSP_READ_DATA));
}

void sb_install_interrupts(void (*sb_intr)(_go32_dpmi_registers *)) {
  sb_install_rm_interrupt(sb_intr);
  sb_install_pm_interrupt(sb_intr);
}

/*
 * Install our interrupt as the real mode interrupt handler for
 * the IRQ the soundblaster is on.
 *
 * We accomplish this by have GO32 allocate a real mode callback for us.
 * The callback packages our protected mode code up in a real mode wrapper.
 */
void sb_install_rm_interrupt(void (*sb_intr)(_go32_dpmi_registers *)) {
  int ret;

  rm_si.pm_offset = (int)sb_intr;
  ret = _go32_dpmi_allocate_real_mode_callback_iret(&rm_si, &rm_regs);
  if (ret != 0) {
    printf("cannot allocate real mode callback, error=%04x\n", ret);
    exit(1);
  }

  disable();
  _go32_dpmi_get_real_mode_interrupt_vector(8 + sb_irq, &oldirq_rm);
  _go32_dpmi_set_real_mode_interrupt_vector(8 + sb_irq, &rm_si);
  enable();
}

/* Remove our real mode interrupt handler. */
void sb_cleanup_rm_interrupt() {
  disable();
  _go32_dpmi_set_real_mode_interrupt_vector(8 + sb_irq, &oldirq_rm);
  /* gk : added safety check */
  if (rm_si.size != -1)
    _go32_dpmi_free_real_mode_callback(&rm_si);
  rm_si.size = -1;
  enable();
}

/* Install our interrupt as the protected mode interrupt handler for
 * the IRQ the soundblaster is on. */
void sb_install_pm_interrupt(void (*sb_intr)(_go32_dpmi_registers *)) {
  int ret;
  disable();

  pm_si.pm_offset = (int)sb_intr;
  /* changes to wrap by grzegorz */
  ret = _go32_dpmi_allocate_iret_wrapper(&pm_si);
  if (ret != 0) {
    printf("cannot allocate protected mode wrapper, error=%04x\n", ret);
    exit(1);
  }

  pm_si.pm_selector = _go32_my_cs();
  _go32_dpmi_get_protected_mode_interrupt_vector(8 + sb_irq, &oldirq_pm);
  _go32_dpmi_set_protected_mode_interrupt_vector(8 + sb_irq, &pm_si);
  enable();
}

/* Remove our protected mode interrupt handler. */
void sb_cleanup_pm_interrupt() {
  disable();
  /* changes to wrap by grzegorz, safety chek by gk */
  if (pm_si.size != -1)
    _go32_dpmi_free_iret_wrapper(&pm_si);
  pm_si.size = -1;

  _go32_dpmi_set_protected_mode_interrupt_vector(8 + sb_irq, &oldirq_pm);
  enable();
}

/* Allocate conventional memory for our DMA buffers.
 * Each DMA buffer must be aligned on a 64K boundary in physical memory. */
int sb_init_buffers() {
  dosmem.size = 65536 * 3 / 16;
  if (_go32_dpmi_allocate_dos_memory(&dosmem)) {
    printf("Unable to allocate dos memory - max size is %lu\n", dosmem.size);
    dosmem.size = -1;
    return (0);
  }

  unsigned long sb_buf_aux[2];

  sb_buf_aux[0] = dosmem.rm_segment * 16;
  sb_buf_aux[0] += 0x0FFFFL;
  sb_buf_aux[0] &= 0xFFFF0000L;
  sb_buf_aux[1] = sb_buf_aux[0] + 0x10000;
  memcpy(sb_buf, sb_buf_aux, sizeof(sb_buf_aux));
  return (1);
}
/* Initliaze our internal buffers and the card itself to prepare
 * for sample playing.
 *
 * Call this once per program, not once per sample. */
int sb_init() {
  memset(&rm_regs, 0, sizeof(_go32_dpmi_registers));
  /* undefined registers cause trouble */
  rm_si.size = -1; /* to allow safety check before free */
  pm_si.size = -1;
  dosmem.size = -1;

  sb_getparams(); /* Card card params and initialize card. */
  sb_initcard();

  if (sb_ioaddr)
    /* Allocate buffers in conventional memory for double-buffering */
    if (!sb_init_buffers())
      return 0;
  return (sb_ioaddr);
}

/* Remove interrupt handlers */
void sb_cleanup_ints() {
  /*  Remove our interrupt handlers */
  sb_cleanup_rm_interrupt();
  sb_cleanup_pm_interrupt();
}

/* leave no traces on exiting module
 * Call this at end of program or if you won't need sb functions anymore
 */
int sb_cleanup() {
  if (dosmem.size == -1) /* There is nothing to free */
    return (1);
  if (_go32_dpmi_free_dos_memory(&dosmem)) {
    printf("Unable to free dos memory\n");
    return (1);
  }
  return (0);
}

int sb_read_counter(void)
/* tells you how many bytes DMA play/recording have still to be done */
{
  outportb(SB_DMA_FF, 0);
  return (inportb(SB_DMA + 2 * sb_dmachan + 1) +
          256 * inportb(SB_DMA + 2 * sb_dmachan + 1));
}

void sb_dsp_version(short *major, short *minor) {
  sb_writedac(SB_DSP_VER);
  *major = sb_read_dac();
  *minor = sb_read_dac();
}
/* Jagholin: I just copied this function as-is because it's some DOS black magic
 * This doesn't have anything to do with sound card.
 * What it does is clearing DOS keyboard buffer.
 * It is hard to find good documentation about this in 2025, so I will explain
 * this as best as I can. 0x41a is a locatioon of a pointer(2 bytes long)
 * pointing to the start of the queue, and 0x41c points to the end of the queue
 * (which is organized as a ring buffer) Setting head=tail is a way to "clear"
 * the queue
 *
 * read https://www.infania.net/misc/kbarchive/kb/060/Q60140/index.html for more
 * on this
 * (I archived it in the internet archive archive.org if the link goes bad)
 */
void kbclear(void) {
  short buffer;

  dosmemget(0x41a, sizeof(short), &buffer);
  dosmemput(&buffer, 2, 0x41c);
}

void StreamStart(int Rate) {
  for (int i = 0; i != DMA_CHUNK; ++i) {
    sb_stream_silence[i] = 128;
  }

  sb_voice(1);
  sb_set_sample_rate(Rate);

  // TODO: do we need to do this all the time? Just put it into init func or
  // something
  sb_install_interrupts(sb_intr_play); /* Install our interrupt handlers */

  sb_fill_buffer(0);
  sb_fill_buffer(1);

  sb_play_buffer(0); /* Start the first buffer playing.	*/
}

unsigned char *StreamBuf(size_t *len) {
  if (sb_stream_ready) {
    return 0;
  }
  *len = DMA_CHUNK;
  return (unsigned char *)sb_stream_buf;
}

void StreamReady() { sb_stream_ready = 1; }

void StreamStop() {
  if (HIGHSPEED)
    sb_reset(); /* writedac blocked in HS mode */
  else
    sb_writedac(SB_HALT_DMA);

  // TODO: see comment in StreamStart function above
  sb_cleanup_ints(); /* remove interrupts */
  sb_voice(0);
}
