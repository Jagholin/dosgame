/* Some useful preprocessor directives and definitions */

#define INP(port) inportb(port)
#define OUT(port, data) outportb(port, data)

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
#define CHECKRESULTP(expr, ...)                                                 \
  result = expr;                                                               \
  if (ISERR(result)) {                                                         \
    printf(__VA_ARGS__);                                                       \
    goto onerror;                                                              \
  }
