#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "fcntl.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

uint64
sys_mmap(void)
{
  uint64 addr; // uint *, ignore
  uint64 length; // uint
  int prot;
  int flags;
  int fd;
  int offset; // uint, ignore
  struct file *f;
  struct proc *p = myproc();

  // argraw(0, &addr);
  length = p->trapframe->a1; // arg1
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  if(fd < 0 || fd >= NOFILE || (f=p->ofile[fd]) == 0)
    return -1;
  argint(5, &offset);

  // check fd
  if(f->type != FD_INODE)
    return -1;
  if(prot & PROT_READ && f->readable == 0)
    return -1;
  if(prot & PROT_WRITE && f->writable == 0 && (flags & MAP_PRIVATE) == 0)
    return -1;

  // find empty area
  if(p->sz + length > MAXVA)
    return -1;
  addr = p->sz;
  p->sz += length;

  // add VMA
  struct VMA *vma = 0;
  for(int i = 0; i < 16; i++){
    if(p->vmas[i].file == 0){
      vma = &p->vmas[i];
      break;
    }
  }
  if(vma == 0)
    return -1;
  vma->addr = addr;
  vma->length = length;
  vma->pteprot = prot << 1; // PTE = PROT << 1
  vma->flags = flags;
  vma->file = filedup(f);
  vma->mapaddrst = addr;
  vma->mapaddred = addr + length;

  return addr;
}

uint64
munmap(struct proc *p, uint64 addr, uint64 length)
{
  struct VMA *vma = 0;

  // get VMA
  for(int i = 0; i < 16; i++){
    if(p->vmas[i].file == 0)
      continue;
    if(addr < p->vmas[i].addr || (p->vmas[i].addr + p->vmas[i].length) <= addr)
      continue;
    vma = &p->vmas[i];
    break;
  }
  if(vma == 0){
    return -1;
  }
  
  // check position
  printf("addr=%x np=%d\n", addr, length/PGSIZE);
  printf("st=%x ed=%x\n", vma->mapaddrst, vma->mapaddred);
  if(addr == vma->mapaddrst){
    vma->mapaddrst += length;
  } else if ((addr + length) == vma->mapaddred){
    vma->mapaddred -= length;
  } else {
    return -1;
  }

  // write back
  if(vma->flags & MAP_SHARED){
    for(uint64 off = 0; off < length; off += PGSIZE){
      uint64 a = addr + off;
      pte_t *ppte = walk(p->pagetable, a, 0);
      if(ppte == 0 || (*ppte & PTE_D) == 0)
        continue;
      begin_op();
      struct file *f = vma->file;
      int r = 0;
      ilock(f->ip);
      r = writei(f->ip, 1, a, a-vma->addr, PGSIZE);
      iunlock(f->ip);
      end_op();
      if(r == 0)
        return -1;
    }
  }

  // unmap
  uvmunmap(p->pagetable, addr, length/PGSIZE, 1);

  // release vma
  if(vma->mapaddrst >= vma->mapaddred){
    fileclose(vma->file);
    vma->file = 0;
  }

  return 0;
}

uint64
sys_munmap(void)
{
  struct proc *p = myproc();
  uint64 addr;
  uint64 length;

  addr = p->trapframe->a0; // arg0
  length = p->trapframe->a1; // arg1

  return munmap(p, addr, length);
}
