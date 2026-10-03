// Single translation unit that pulls in the stb_image_write implementation.
// Every other example TU includes "stb_image_write.h" for declarations only;
// the implementation is emitted exactly once, here.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8  // accept UTF-8 paths through MSVC fopen
#include "stb_image_write.h"
