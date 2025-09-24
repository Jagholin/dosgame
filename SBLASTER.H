// Initializes SoundBlaster compatible soundcard
// return EXIT_SUCCESS or EXIT_FAILURE
int init_blaster();

// Deinitialization of the soundcard initialized by init_blaster()
void release_blaster();

// Makes the soundcard play sinusoid waveform on a loop.
void test_sound();

// returns how many times soundcard issued interrupt
int get_interrupt_counter();
