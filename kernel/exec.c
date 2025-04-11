#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

extern void vmprint(pagetable_t pagetable);
int mappages_super(pagetable_t pagetable, uint64 va, uint64 pa, int perm);

static int loadseg(pde_t *, uint64, struct inode *, uint, uint);

int flags2perm(int flags)
{
  int perm = 0;
  if (flags & 0x1)
    perm = PTE_X;
  if (flags & 0x2)
    perm |= PTE_W;
  return perm;
}

// int
// exec(char *path, char **argv)
// {
//   char *s, *last;
//   int i, off;
//   uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
//   struct elfhdr elf;
//   struct inode *ip;
//   struct proghdr ph;
//   pagetable_t pagetable = 0, oldpagetable;
//   struct proc *p = myproc();

//   begin_op();

//   if((ip = namei(path)) == 0){
//     end_op();
//     return -1;
//   }
//   ilock(ip);

//   // Check ELF header
//   if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
//     goto bad;

//   if(elf.magic != ELF_MAGIC)
//     goto bad;

//   if((pagetable = proc_pagetable(p)) == 0)
//     goto bad;

//   // Load program into memory.
//   for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
//     if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
//       goto bad;
//     if(ph.type != ELF_PROG_LOAD)
//       continue;
//     if(ph.memsz < ph.filesz)
//       goto bad;
//     if(ph.vaddr + ph.memsz < ph.vaddr)
//       goto bad;
//     if(ph.vaddr % PGSIZE != 0)
//       goto bad;
//     uint64 sz1;
//     if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
//       goto bad;
//     sz = sz1;
//     if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
//       goto bad;
//   }
//   iunlockput(ip);
//   end_op();
//   ip = 0;

//   p = myproc();
//   uint64 oldsz = p->sz;

//   // Allocate some pages at the next page boundary.
//   // Make the first inaccessible as a stack guard.
//   // Use the rest as the user stack.
//   sz = PGROUNDUP(sz);
//   uint64 sz1;
//   if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W)) == 0)
//     goto bad;
//   sz = sz1;
//   uvmclear(pagetable, sz-(USERSTACK+1)*PGSIZE);
//   sp = sz;
//   stackbase = sp - USERSTACK*PGSIZE;

//   // Push argument strings, prepare rest of stack in ustack.
//   for(argc = 0; argv[argc]; argc++) {
//     if(argc >= MAXARG)
//       goto bad;
//     sp -= strlen(argv[argc]) + 1;
//     sp -= sp % 16; // riscv sp must be 16-byte aligned
//     if(sp < stackbase)
//       goto bad;
//     if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
//       goto bad;
//     ustack[argc] = sp;
//   }
//   ustack[argc] = 0;

//   // push the array of argv[] pointers.
//   sp -= (argc+1) * sizeof(uint64);
//   sp -= sp % 16;
//   if(sp < stackbase)
//     goto bad;
//   if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
//     goto bad;

//   // arguments to user main(argc, argv)
//   // argc is returned via the system call return
//   // value, which goes in a0.
//   p->trapframe->a1 = sp;

//   // Save program name for debugging.
//   for(last=s=path; *s; s++)
//     if(*s == '/')
//       last = s+1;
//   safestrcpy(p->name, last, sizeof(p->name));

//   // Commit to the user image.
//   oldpagetable = p->pagetable;
//   p->pagetable = pagetable;
//   p->sz = sz;
//   p->trapframe->epc = elf.entry;  // initial program counter = main
//   p->trapframe->sp = sp; // initial stack pointer
//   proc_freepagetable(oldpagetable, oldsz);

//   //lab03
//   if(p->pid == 1){
//       vmprint(p->pagetable);
//   }

//   return argc; // this ends up in a0, the first argument to main(argc, argv)

//  bad:
//   if(pagetable)
//     proc_freepagetable(pagetable, sz);
//   if(ip){
//     iunlockput(ip);
//     end_op();
//   }
//   return -1;
// }

int exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  begin_op();

  if ((ip = namei(path)) == 0)
  {
    end_op();
    return -1;
  }
  ilock(ip);

  if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if (elf.magic != ELF_MAGIC)
    goto bad;

  if ((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph))
  {
    if (readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if (ph.vaddr % PGSIZE != 0)
      goto bad;

    int perm = flags2perm(ph.flags);

    // Nếu segment đủ lớn và aligned: dùng superpage
    if ((ph.vaddr % (1 << 21) == 0) && (ph.memsz == (1 << 21)))
    {
      char *mem = kalloc();
      if (mem == 0)
        goto bad;

      if (mappages_super(pagetable, ph.vaddr, (uint64)mem, perm) < 0)
        goto bad;

      if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
        goto bad;

      if (ph.filesz < ph.memsz)
        memset((void *)(ph.vaddr + ph.filesz), 0, ph.memsz - ph.filesz);

      if (ph.vaddr + ph.memsz > sz)
        sz = ph.vaddr + ph.memsz;
    }
    else
    {
      // Thường: dùng uvmalloc + mappages
      uint64 sz1;
      if ((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, perm)) == 0)
        goto bad;
      sz = sz1;
      if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
        goto bad;
    }
  }

  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  sz = PGROUNDUP(sz);
  uint64 sz1;
  if ((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK + 1) * PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz - (USERSTACK + 1) * PGSIZE);
  sp = sz;
  stackbase = sp - USERSTACK * PGSIZE;

  for (argc = 0; argv[argc]; argc++)
  {
    if (argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16;
    if (sp < stackbase)
      goto bad;
    if (copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if (sp < stackbase)
    goto bad;
  if (copyout(pagetable, sp, (char *)ustack, (argc + 1) * sizeof(uint64)) < 0)
    goto bad;

  p->trapframe->a1 = sp;

  for (last = s = path; *s; s++)
    if (*s == '/')
      last = s + 1;
  safestrcpy(p->name, last, sizeof(p->name));

  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;
  p->trapframe->sp = sp;
  proc_freepagetable(oldpagetable, oldsz);

  // In ra page table khi pid = 1 để debug
  if (p->pid == 1)
  {
    vmprint(p->pagetable);
  }

  return argc;

bad:
  if (pagetable)
    proc_freepagetable(pagetable, sz);
  if (ip)
  {
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  for (i = 0; i < sz; i += PGSIZE)
  {
    pa = walkaddr(pagetable, va + i);
    if (pa == 0)
      panic("loadseg: address should exist");
    if (sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if (readi(ip, 0, (uint64)pa, offset + i, n) != n)
      return -1;
  }

  return 0;
}
