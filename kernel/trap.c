#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from, and returns to, trampoline.S
// return value is user satp for trampoline.S to switch to.
//
uint64
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);  //DOC: kernelvec

  struct proc *p = myproc();
  
  // save user program counter.
  uint64 sepc = r_sepc();
  
  // Debug: Check if sepc is suspiciously low BEFORE saving to trapframe
  // This will help catch when sepc is already corrupted
  if(sepc < 0x100 && r_scause() != 8 && p->pid != 1) {
    printf("usertrap(): WARNING: sepc=0x%lx is very low at trap entry (pid=%d, scause=0x%lx, sz=0x%lx)\n", 
           sepc, p->pid, r_scause(), p->sz);
    // Check if this matches the epc in trapframe (might indicate corruption)
    if(p->trapframe->epc == sepc) {
      printf("            NOTE: trapframe->epc already matches sepc - possible corruption\n");
    }
  }
  
  p->trapframe->epc = sepc;
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      kexit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;
    
    // Validate epc after increment - but don't kill, just warn
    // The process might legitimately have low addresses in some cases
    if(p->trapframe->epc < 0x100 && p->sz > 0x10000) {
      printf("usertrap(): WARNING: epc=0x%lx is very low after system call (pid=%d, sepc was 0x%lx, sz=0x%lx)\n",
             p->trapframe->epc, p->pid, sepc, p->sz);
    }
    if(p->trapframe->epc >= p->sz) {
      printf("usertrap(): ERROR: epc=0x%lx >= process size 0x%lx after system call (pid=%d)\n",
             p->trapframe->epc, p->sz, p->pid);
      setkilled(p);
    }

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if(r_scause() == 15) {
    // Store page fault - could be COW, swapped, or lazy allocation
    uint64 va = r_stval();
    pte_t *pte = walk(p->pagetable, va, 0);
    if(pte != 0 && (*pte & PTE_SWAPPED) != 0) {
      // This is a swapped page fault
      memstats_inc_pagefault();
      if(swapin(p->pagetable, va) == 0) {
        setkilled(p);
      }
    } else if(pte != 0 && (*pte & PTE_V) != 0 && (*pte & PTE_COW) != 0) {
      // This is a COW page fault
      memstats_inc_pagefault();
      memstats_inc_cowfault();
      if(cowfault(p->pagetable, va) != 0) {
        setkilled(p);
      }
    } else {
      // Lazy allocation page fault
      uint64 pa = vmfault(p->pagetable, va, 0);
      if(pa == 0) {
        // Failed
        setkilled(p);
      } else {
        // Success
        memstats_inc_pagefault();
        memstats_inc_lazyalloc();
      }
    }
  } else if(r_scause() == 13) {
    // Load page fault - could be swapped, COW, or lazy allocation
    uint64 va = r_stval();
    pte_t *pte = walk(p->pagetable, va, 0);
    if(pte != 0 && (*pte & PTE_SWAPPED) != 0) {
      // This is a swapped page fault
      memstats_inc_pagefault();
      if(swapin(p->pagetable, va) == 0) {
        setkilled(p);
      }
    } else if(pte != 0 && (*pte & PTE_V) != 0 && (*pte & PTE_COW) != 0) {
      // COW pages are read-only and should be readable
      // If we get a load page fault on a COW page, something is wrong
      // This shouldn't happen - COW pages should be readable
      // Just kill the process as this indicates a serious error
      printf("usertrap(): load page fault on COW page va=0x%lx pid=%d\n", va, p->pid);
      setkilled(p);
    } else {
      // Lazy allocation page fault
      uint64 pa = vmfault(p->pagetable, va, 1);
      if(pa == 0) {
        // Failed
        setkilled(p);
      } else {
        // Success
        memstats_inc_pagefault();
        memstats_inc_lazyalloc();
      }
    }
  } else if(r_scause() == 2) {
    // Illegal instruction exception
    // This usually means the instruction pointer is pointing to invalid memory
    // or the instruction at that address is not valid
    uint64 sepc = r_sepc();
    uint64 stval = r_stval();
    printf("usertrap(): illegal instruction pid=%d\n", p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", sepc, stval);
    printf("            instruction pointer may be corrupted or pointing to invalid memory\n");
    
    // Check if the page containing sepc is mapped
    pte_t *pte = walk(p->pagetable, sepc, 0);
    if(pte == 0) {
      printf("            ERROR: Page table entry for sepc=0x%lx does not exist\n", sepc);
    } else if((*pte & PTE_V) == 0) {
      printf("            ERROR: Page at sepc=0x%lx is not valid (PTE_V=0)\n", sepc);
    } else {
      uint64 pa = PTE2PA(*pte);
      uint flags = PTE_FLAGS(*pte);
      printf("            Page is mapped: pa=0x%lx flags=0x%x\n", pa, flags);
      if((flags & PTE_X) == 0) {
        printf("            ERROR: Page is not executable (PTE_X=0)\n");
      } else {
        // Page is executable, but instruction is illegal - check what's at that address
        uint64 offset = sepc % PGSIZE;
        // Read 32-bit instruction (RISC-V instructions are 32 bits)
        uint32 *instr_ptr = (uint32 *)((char *)pa + offset);
        printf("            Instruction at sepc: 0x%x (offset=0x%lx in page)\n", 
               *instr_ptr, offset);
        printf("            Process sz=0x%lx, epc in trapframe=0x%lx\n", 
               p->sz, p->trapframe->epc);
        
        // Check if epc is suspiciously low or outside process size
        // For illegal instruction, epc should point to the instruction that caused the fault
        // If epc is very low, it might indicate corruption
        if(p->trapframe->epc < 0x100 && p->pid != 1) {
          printf("            WARNING: epc=0x%lx is very low (< 256 bytes)\n", p->trapframe->epc);
          printf("            This might indicate instruction pointer corruption\n");
        }
        if(p->trapframe->epc >= p->sz) {
          printf("            ERROR: epc=0x%lx is >= process size 0x%lx\n", 
                 p->trapframe->epc, p->sz);
        }
      }
    }
    
    setkilled(p);
  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    kexit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  prepare_return();

  // the user page table to switch to, for trampoline.S
  uint64 satp = MAKE_SATP(p->pagetable);

  // return to trampoline.S; satp value in a0.
  return satp;
}

//
// set up trapframe and control registers for a return to user space
//
void
prepare_return(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(). because a trap from kernel
  // code to usertrap would be a disaster, turn off interrupts.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  // Validate epc before setting sepc
  // Only kill if epc is clearly invalid (outside process size)
  // Low addresses might be valid for some programs, so just warn
  if(p->trapframe->epc < 0x100 && p->pid != 1 && p->sz > 0x10000) {
    printf("prepare_return(): WARNING: epc=0x%lx is very low before return (pid=%d, sz=0x%lx)\n",
           p->trapframe->epc, p->pid, p->sz);
    // Don't kill - let it try to execute and see what happens
    // The illegal instruction handler will catch it if it's truly invalid
  }
  if(p->trapframe->epc >= p->sz) {
    printf("prepare_return(): ERROR: epc=0x%lx >= process size 0x%lx (pid=%d)\n",
           p->trapframe->epc, p->sz, p->pid);
    setkilled(p);
    return;  // Don't return to invalid address
  }
  w_sepc(p->trapframe->epc);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // interrupt or trap from an unknown source
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // ask for the next timer interrupt. this also clears
  // the interrupt request. 1000000 is about a tenth
  // of a second.
  w_stimecmp(r_time() + 1000000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

