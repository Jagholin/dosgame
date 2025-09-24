#ifndef IRQFUNCS_H
#define IRQFUNCS_H

int LockData(void *, long size);
int LockCode(void *, long size);

#define END_OF_FUNCTION(x)                                                     \
  static void x##_End() {}

#define LOCK_VARIABLE(x) LockData((void *)&x, sizeof(x))
#define LOCK_FUNCTION(x) LockCode(x, (long)x##_End - (long)x)

#endif // IRQFUNCS_H
