#include <dpmi.h>
#include <go32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pc.h>

#include "defs.h"

static int base_io = -1, irq = -1, dma8 = -1, dma16 = -1, mixer_io = -1,
           mpu_io = -1;

int realmem_selector = 0;
int realmem_segment = 0;
_go32_dpmi_seginfo old_interrupt_seginfo, my_interrupt_seginfo;

// Interrupt routine when the SB is done with the chunk
void sb_intr() {
}

void init_sb_intr() {
  // 1. Allocate 128 kB for the page aligned block of memory in the first MB
  realmem_segment = __dpmi_allocate_dos_memory(8192, &realmem_selector);
  if (realmem_segment == -1) {
    printf("Can't allocate buffer in the first 1MiB of memory\n");
    exit(EXIT_FAILURE);
  }
  // Calculate interrupt vector based on IRQ
  short int_vec = 0x08 + irq;
  if (irq >= 8) int_vec = 0x70 + irq - 8;

  // The following is based on https://delorie.com/djgpp/v2faq/faq18_9.html
  int res = _go32_dpmi_get_protected_mode_interrupt_vector(int_vec,
                                                           &old_interrupt_seginfo);
  if (res != 0) {
    printf("Can't get protected mode interrupt vector\n");
    exit(EXIT_FAILURE);
  }
  my_interrupt_seginfo.pm_offset = (long int)&sb_intr;
  my_interrupt_seginfo.pm_selector = _go32_my_cs();
  res = _go32_dpmi_chain_protected_mode_interrupt_vector(int_vec,
                                                         &my_interrupt_seginfo);

  if (res != 0) {
    printf("Can't allocate iret wrapper for my interrupt routine\n");
    exit(EXIT_FAILURE);
  }

  //TODO: Lock memory that the interrupt uses
}

void release_sb_intr() {
  // Calculate interrupt vector based on IRQ
  short int_vec = 0x08 + irq;
  if (irq >= 8) int_vec = 0x70 + irq - 8;
  _go32_dpmi_set_protected_mode_interrupt_vector(int_vec,
                                                 &old_interrupt_seginfo);
  __dpmi_free_dos_memory(realmem_selector);
}

// SB DMA routines
void init_sb_dma() {

}

void release_sb_dma() {
}

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
  //int res = 0;
  //bool initres = false;
  CHECKERR(blaster_env == NULL, "BLASTER environment variable isn't defined");

  blaster_envc = malloc(strlen(blaster_env) + 1);
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
}
