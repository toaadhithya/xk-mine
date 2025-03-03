#include <cdefs.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <spinlock.h>
#include <trap.h>
#include <x86_64.h>


// Interrupt descriptor table (shared by all CPUs).
struct gate_desc idt[256];
extern void *vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

int num_page_faults = 0;

void tvinit(void) {
  int i;

  for (i = 0; i < 256; i++)
    set_gate_desc(&idt[i], 0, SEG_KCODE << 3, vectors[i], KERNEL_PL);
  set_gate_desc(&idt[TRAP_SYSCALL], 1, SEG_KCODE << 3, vectors[TRAP_SYSCALL],
                USER_PL);

  initlock(&tickslock, "time");
}

void idtinit(void) { lidt((void *)idt, sizeof(idt)); }

void trap(struct trap_frame *tf) {
  uint64_t addr;

  if (tf->trapno == TRAP_SYSCALL) {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno) {
  case TRAP_IRQ0 + IRQ_TIMER:
    if (cpunum() == 0) {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case TRAP_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + 7:
  case TRAP_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n", cpunum(), tf->cs, tf->rip);
    lapiceoi();
    break;

  default:
    addr = rcr2();

    if (tf->trapno == TRAP_PF) {
      num_page_faults += 1;

      // LAB3: page fault handling logic here

      // determine if we need to grow the stack or do a cow
      if ((tf->err & 1) == 1 &&  (tf->err & 2) == 2) {
         // potential COW
         if (cowcopy(addr) == 0) {
           break;
         } 
         // check that the cowpage flag is set, then create new stuff
      } else if ((tf->err & 1) == 0 && (tf->err & 4) == 4){ // per the design doc
        // grow the the stack
        if (growstack(addr) == 0) {
          break;
        }
      }

      if (myproc() == 0 || (tf->cs & 3) == 0) {
        // In kernel, it must be our mistake.
        cprintf("unexpected trap %d err %d from cpu %d rip %lx (cr2=0x%x)\n",
                tf->trapno, tf->err, cpunum(), tf->rip, addr);
        panic("trap");
      }

      // for page faults we couldn't handle we will kill the process
      cprintf("pid %d %s: trap %d err %d on cpu %d "
              "rip 0x%lx (cr2=0x%x)--kill proc\n",
              myproc()->pid, myproc()->name, tf->trapno, tf->err, cpunum(),
              tf->rip, addr);
      myproc()->killed = 1;
    } else {
      // for non-page fault traps that reached the default case
      cprintf("pid %d %s: trap %d err %d on cpu %d "
              "rip 0x%lx (cr2=0x%x)--kill proc\n",
              myproc()->pid, myproc()->name, tf->trapno, tf->err, cpunum(),
              tf->rip, addr);
      myproc()->killed = 1;
    }
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == TRAP_IRQ0 + IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}

int growstack(uint64_t addr) {
  struct proc* p = myproc();
  if (p == NULL) {
    return -1;
  }
  addr = PGROUNDDOWN(addr);
  // don't allow stack to grow above stackbase
  if (addr >= p->vspace.regions[VR_USTACK].va_base) {
    return -1;
  }

  // no more than 10 pages of growth
  if (addr < p->vspace.regions[VR_USTACK].va_base - 10 * PGSIZE) {
    return -1;
  }
  
  // stack_bottom = 2147479552
  // addr =         2147450472
  uint64_t stack_bottom = p->vspace.regions[VR_USTACK].va_base - p->vspace.regions[VR_USTACK].size;
  uint64_t sz = stack_bottom - addr;


  if (vregionaddmap(&p->vspace.regions[VR_USTACK], addr, sz, VPI_PRESENT, VPI_WRITABLE) == -1) {
    return -1;
  }
  p->vspace.regions[VR_USTACK].size += sz;
      
  vspaceupdate(&p->vspace);
  return 0;
}


// here is the current sequence, cowcopy is called and executes(semmingly) fine
// wait() is called which triggers a trap error that seems to enter grow stack, 
// growstack fails due to the address being out of range
// we have a trap error
int cowcopy(uint64_t addr) {
  struct proc* p = myproc();
  if (p == NULL) {
    return -1;
  }

  struct vregion * region = va2vregion(&p->vspace, addr);
  struct vpage_info* page = va2vpage_info(region, addr);
  
  if (!page->cowpageflag) {
    return -1;
  } 
  // we have a cowpage

  // kalloc a new frame
  struct vpage_info copy;
  char * data;

  // Note: i actually dont think we need copy here, can just do everything with page
  copy.present = page->present;
  copy.writable = VPI_WRITABLE; 
  copy.cowpageflag = 0;
  copy.used = page->used;
  
  if (!(data = kalloc())){
    return -1;
  }
  memmove(data, P2V(page->ppn << PT_SHIFT), PGSIZE);
  
  copy.ppn = PGNUM(V2P(data));
  //check_refcount(P2V(page->ppn << PT_SHIFT));
  kfree(P2V(page->ppn << PT_SHIFT));
  // check_refcount(P2V(page->ppn << PT_SHIFT));
  
  *page = copy;
  vspaceupdate(&p->vspace);
  vspaceinstall(p);
  
  return 0;
}

