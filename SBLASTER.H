// Initializes SoundBlaster compatible soundcard
// return EXIT_SUCCESS or EXIT_FAILURE
int init_blaster();

// Deinitialization of the soundcard initialized by init_blaster()
void release_blaster();
