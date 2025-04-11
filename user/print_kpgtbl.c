#include "kernel/types.h"
#include "user/user.h"

int
main()
{
  kpgtbl(); // gọi syscall
  exit(0);
}
