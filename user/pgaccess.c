#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"

char *testname = "pgaccess_test";

void
err(char *why)
{
  printf("pgaccess_test: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

int
main(int argc, char *argv[])
{
  printf("pgaccess_test starting\n");

  int npages = 16;
  char *buf = malloc(npages * PGSIZE);
  if (buf == 0)
    err("Malloc failed");

  // Gọi lần đầu khi chưa truy cập gì
  uint64 abits = 0;
  if (pgaccess(buf, npages, &abits) < 0)
    err("pgaccess failed (first call)");

  if (abits != 0)
    err("Expected no accessed pages initially");

  // Truy cập một vài trang
  buf[PGSIZE * 0] = 1;
  buf[PGSIZE * 3] = 2;
  buf[PGSIZE * 7] = 3;

  // Gọi lần hai để kiểm tra
  abits = 0;
  if (pgaccess(buf, npages, &abits) < 0)
    err("pgaccess failed (second call)");

  uint64 expected = (1 << 0) | (1 << 3) | (1 << 7);
  if (abits != expected)
    err("Access bitmask incorrect");

  // Gọi lần ba để kiểm tra việc xóa bit sau lần trước
  abits = 0;
  if (pgaccess(buf, npages, &abits) < 0)
    err("pgaccess failed (third call)");

  if (abits != 0)
    err("Access bits not cleared properly");

  free(buf);
  printf("pgaccess_test: OK\n");
  exit(0);
}
