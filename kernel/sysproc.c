#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Get memory statistics
// Returns physical pages in use, and fills in pointers with stats
uint64
sys_get_mem_stats(void)
{
  uint64 page_faults, swap_ins, swap_outs, cow_faults, lazy_allocs;
  uint64 pages_in_use;
  uint64 addr;
  struct proc *p = myproc();
  
  // Get pointer to stats structure
  argaddr(0, &addr);
  
  // Get statistics
  pages_in_use = memstats_get(&page_faults, &swap_ins, &swap_outs, 
                              &cow_faults, &lazy_allocs);
  
  // Copy stats to user space
  struct {
    uint64 pages_in_use;
    uint64 page_faults;
    uint64 swap_ins;
    uint64 swap_outs;
    uint64 cow_faults;
    uint64 lazy_allocs;
  } stats;
  
  stats.pages_in_use = pages_in_use;
  stats.page_faults = page_faults;
  stats.swap_ins = swap_ins;
  stats.swap_outs = swap_outs;
  stats.cow_faults = cow_faults;
  stats.lazy_allocs = lazy_allocs;
  
  if(copyout(p->pagetable, addr, (char*)&stats, sizeof(stats)) < 0)
    return -1;
  
  return 0;
}

// Reset memory statistics counters
uint64
sys_reset_mem_stats(void)
{
  memstats_reset();
  return 0;
}
