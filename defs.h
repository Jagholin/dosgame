/* Some useful preprocessor directives and definitions */
#ifndef DEFS_H
#define DEFS_H

#include <pc.h>

#define INP(port) inportb(port)
#define OUT(port, data) outportb(port, data)

#define LOWBYTE(x) ((x) & 0xFF)
#define HIBYTE(x) (((x) & 0xFF00) >> 8)

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef short int int16;

#define CHECKERR(cond, ...)                                                    \
  if (cond) {                                                                  \
    printf(__VA_ARGS__);                                                       \
    goto onerror;                                                              \
  }
#define OOMERR(ptr)                                                            \
  if (ptr == NULL)                                                             \
    goto onerror;

typedef int RESULT;
#define RESULT_OK 0
#define RESULT_ERR -1
#define ISERR(result) ((int)(result)) < 0
#define CHECKRESULT(expr)                                                      \
  result = expr;                                                               \
  if (ISERR(result)) {                                                         \
    goto onerror;                                                              \
  }
#define CHECKRESULTP(expr, ...)                                                \
  result = expr;                                                               \
  if (ISERR(result)) {                                                         \
    printf(__VA_ARGS__);                                                       \
    goto onerror;                                                              \
  }

#endif // DEFS_H
