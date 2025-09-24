/* Using modified code from
 * https://www.delorie.com/djgpp/doc/ug/interrupts/hwirqs.html */
#include <dpmi.h>
#include <memory.h>
#include <sys/segments.h>

#include "IRQFUNCS.H"

int bInitIRQ = 0;
#define IRQ_STACKS 8
const int STACK_SIZE = 8192;
typedef int (*IRQHandler_t)(void);
extern void *IRQWrappers[16];
extern IRQHandler_t IRQHandlers[16];
__dpmi_paddr OldIRQVectors[16];
void *IRQStacks[IRQ_STACKS];

extern void IRQWrap();
extern void IRQWrap_End();

int LockData(void *a, long size) {
  unsigned long baseaddr;
  __dpmi_meminfo region;

  if (__dpmi_get_segment_base_address(_my_ds(), &baseaddr) == -1)
    return (-1);

  region.handle = 0;
  region.size = size;
  region.address = baseaddr + (unsigned long)a;

  if (__dpmi_lock_linear_region(&region) == -1)
    return (-1);

  return (0);
}

int LockCode(void *a, long size) {
  unsigned long baseaddr;
  __dpmi_meminfo region;

  if (__dpmi_get_segment_base_address(_my_cs(), &baseaddr) == -1)
    return (-1);

  region.handle = 0;
  region.size = size;
  region.address = baseaddr + (unsigned long)a;

  if (__dpmi_lock_linear_region(&region) == -1)
    return (-1);

  return (0);
}

static int InitIRQ(void) {
  int i;

  /*
  Lock IRQWrapers[], IRQHandlers[] and IRWrap0()-IRQWrap15().
  */
  if (LOCK_VARIABLE(IRQWrappers) == -1)
    return 0;
  if (LOCK_VARIABLE(IRQHandlers) == -1)
    return 0;
  if (LOCK_VARIABLE(OldIRQVectors) == -1)
    return 0;
  if (LOCK_FUNCTION(IRQWrap) == -1)
    return 0;

  for (i = 0; i < IRQ_STACKS; ++i) {
    if ((IRQStacks[i] = malloc(STACK_SIZE)) == NULL)
      ;
    //...
    LockData(IRQStacks[i], STACK_SIZE);
    //...
    IRQStacks[i] = (char *)IRQStacks[i] +
                   (STACK_SIZE - 16); /* Stack is incremented downward */
  }
  bInitIRQ = 1;
  return 1;
}

static void ShutDownIRQ(void) {
  int i;
  char *p;

  for (i = 0; i < IRQ_STACKS; ++i) {
    p = (char *)IRQStacks[i] - (STACK_SIZE - 16);
    free(p);
  }
  bInitIRQ = 0;
}

int InstallIRQ(int nIRQ, int (*IRQHandler)(void)) {
  int nIRQVect;
  __dpmi_paddr IRQWrapAddr;

  if (!bInitIRQ)
    if (!InitIRQ())
      return 0;

  if (nIRQ > 7)
    nIRQVect = 0x70 + (nIRQ - 8);
  else
    nIRQVect = 0x8 + nIRQ;

  IRQWrapAddr.selector = _my_cs();
  IRQWrapAddr.offset32 = (int)IRQWrappers[nIRQ];
  __dpmi_get_protected_mode_interrupt_vector(nIRQVect, &OldIRQVectors[nIRQ]);
  IRQHandlers[nIRQ] = IRQHandler; /* IRQWrapper will call IRQHandler */

  __dpmi_set_protected_mode_interrupt_vector(nIRQVect, &IRQWrapAddr);
  return 1;
}

void UninstallIRQ(int nIRQ) {
  int nIRQVect;
  int i;

  if (nIRQ > 7)
    nIRQVect = 0x70 + (nIRQ - 8);
  else
    nIRQVect = 0x8 + nIRQ;

  __dpmi_set_protected_mode_interrupt_vector(nIRQVect, &OldIRQVectors[nIRQ]);
  IRQHandlers[nIRQ] = NULL;

  /*
  Check whether all the IRQs are uninstalled and call ShutDownIRQ().
  */
  for (i = 0; i < 16; ++i)
    if (IRQHandlers[i] != NULL)
      return; /* Still remains a handler */
  ShutDownIRQ();
}
