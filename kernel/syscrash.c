#include <cdefs.h>
#include <defs.h>

int sys_crashn(void) {
  int n;
  if (argint(0, &n) < 0)
    return -1;

  enable_crashn(n);

  return 0;
}