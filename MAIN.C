#include <crt0.h>
#include <math.h>
#include <stdio.h>

#include "DEFS.H"
// #include "SBLASTER.H"
#include "SB.H"

int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY | _CRT0_FLAG_NONMOVE_SBRK;

void stream_test_sound() {
  // Make up some sound to output with sin waveform
  size_t len;
  static float x = 0.0;
  static float x_step = 0.05;
  unsigned char *stream = StreamBuf(&len);
  if (stream) {
    // We are waiting on more bytes to stream
    for (unsigned char *cursor = stream; cursor != stream + len; cursor++) {
      *cursor = (unsigned char)(sinf(x) * 100 + 128.0);
      x += x_step;
    }
    StreamReady();
  }
}

int main() {
  printf("Welcome to the game.\n");
  printf("Initializing sound card...\n");
  RESULT result;
  if (!sb_init()) {
    printf("Can't init sound card \n");
    return 1;
  }
  StreamStart(22050);
  printf("Initialization successful\nPress any key to exit\n");
  // CHECKRESULTP(init_blaster(),
  //              "Initialization failure. Don't you have a sound card?\n");
  for (;;) {
    stream_test_sound();
    if (kbhit()) {
      break;
    }
  }
  getchar();
  // ints = get_interrupt_counter();
  // printf("Interrupt counter is %d\n", ints);
onreturn:
  StreamStop();
  sb_cleanup();
onerror:
  return 0;
}
