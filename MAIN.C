#include <stdio.h>
#include <stdlib.h>

#include "DEFS.H"
#include "SBLASTER.H"

int main() {
  printf("Welcome to the game.\n");
  printf("Initializing sound card...\n");
  RESULT result;
  CHECKRESULTP(init_blaster(),
               "Initialization failure. Don't you have a sound card?\n");
  printf("Initialization successful\nPress any key to exit\n");
  getchar();
onreturn:
  release_blaster();
onerror:
  return 0;
}
