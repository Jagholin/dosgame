/* Some useful preprocessor directives and definitions */
#ifndef DEFS_H
#define DEFS_H

#include <pc.h>
#include <stdio.h>
#include <unistd.h>

#define INP(port) inportb(port)
#define OUT(port, data) outportb(port, data)

#define LOWBYTE(x) ((x) & 0xFF)
#define HIBYTE(x) (((x) & 0xFF00) >> 8)

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;
typedef int int32;

typedef short int RESULT;

#define CHECKRESULT(...) CHECKERR(last_error < 0, __VA_ARGS__)
#define PROPAGATEERR()                                                         \
  if (last_error < 0) {                                                        \
    printf("Caught error %d on line %d in file " __FILE__ "\n", last_error,    \
           __LINE__);                                                          \
    goto onpropagate;                                                          \
  }
extern const RESULT RES_OK;
extern const RESULT RES_ERR;
extern const RESULT RES_BUFFER_TOO_SMALL;
extern const RESULT RES_BAD_INPUT;
extern RESULT last_error;

static const char oom_message[] = "Out of memory\n\r";
#define OOMERROR(ptr)                                                          \
  if (ptr == NULL) {                                                           \
    write(STDERR_FILENO, oom_message, sizeof(oom_message));                    \
    goto onoom;                                                                \
  }
#define PACKED_STRUCT struct __attribute__((__packed__))
#define CHECKERR(cond, ...)                                                    \
  if (cond) {                                                                  \
    printf(__VA_ARGS__);                                                       \
    if (last_error == RES_OK)                                                  \
      last_error = RES_ERR;                                                    \
    goto onerror;                                                              \
  }
#define CHECKEXPR(expr, cond, errstr, ...)                                     \
  if ((expr)cond) {                                                            \
    printf(errstr "\n", __VA_ARGS__);                                          \
    if (last_error == RES_OK)                                                  \
      last_error = RES_ERR;                                                    \
    goto onerror;                                                              \
  }

#endif // DEFS_H
