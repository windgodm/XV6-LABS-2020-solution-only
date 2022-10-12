#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sysinfo.h"
#include "proc.h"

uint64
sys_sysinfo(void)
{
  uint64 uinfo;
  struct sysinfo info;
  struct proc *p;

  if(argaddr(0, &uinfo) < 0)
    return -1;

  info.freemem = kfreemembytes();
  info.nproc = procnum();

  p = myproc();
  if(copyout(p->pagetable, uinfo, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}
