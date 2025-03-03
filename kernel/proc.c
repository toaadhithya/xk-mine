#include <cdefs.h>
#include <defs.h>
#include <file.h>
#include <fs.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <spinlock.h>
#include <trap.h>
#include <x86_64.h>
#include <fs.h>
#include <file.h>
#include <vspace.h>

// process table
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// Pointer to the init process set up by `userinit`.
static struct proc *initproc;

// Global PID counter used to assign unique increasing PIDs
// to newly created processes.
int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// to test crash safety in lab5,
// we trigger restarts in the middle of file operations
void reboot(void) {
  uint8_t good = 0x02;
  while (good & 0x02)
    good = inb(0x64);
  outb(0x64, 0xFE);
loop:
  asm volatile("hlt");
  goto loop;
}

void pinit(void) { initlock(&ptable.lock, "ptable"); }

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->killed = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trap_frame *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 8;
  *(uint64_t *)sp = (uint64_t)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->rip = (uint64_t)forkret;

  return p;
}

// Set up first user process.
void userinit(void) {
  struct proc *p;
  extern char _binary_out_initcode_start[], _binary_out_initcode_size[];

  p = allocproc();

  initproc = p;
  assertm(vspaceinit(&p->vspace) == 0, "error initializing process's virtual address descriptor");
  vspaceinitcode(&p->vspace, _binary_out_initcode_start, (int64_t)_binary_out_initcode_size);
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ss = (SEG_UDATA << 3) | DPL_USER;
  p->tf->rflags = FLAGS_IF;
  p->tf->rip = VRBOT(&p->vspace.regions[VR_CODE]);  // beginning of initcode.S
  p->tf->rsp = VRTOP(&p->vspace.regions[VR_USTACK]);

  safestrcpy(p->name, "initcode", sizeof(p->name));

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Create new child process that is copy of current process.
// Sets up child process to return as if from system call.
// Must set state of newly created proc to RUNNABLE.
int fork(void) {
  // your code here
  // do allocproc to allocate new PCB

  // if allocproc fail
    // return -1
  
  // vscpace init

  struct proc *p;

  p = allocproc();
  if (p == 0) {
    return -1;
  }

  if (vspaceinit(&p->vspace) == -1) {
    return -1;
  }

  struct proc* parent_proc = myproc();
  if (parent_proc == NULL) {
    return -1;
  }

  if (vspacecowcopy(&p->vspace, &parent_proc->vspace) == -1) {
    return -1;
  }

  p->parent = parent_proc->pid;

  for (int i = 0; i < NOFILE; i++) {
    if (parent_proc->fds[i] != NULL) {
      acquiresleep(&parent_proc->fds[i]->lock);
      parent_proc->fds[i]->ref_count++;
      p->fds[i] = parent_proc->fds[i];
      releasesleep(&parent_proc->fds[i]->lock);
    }
  }

  p->chan = (void*)(long)p->pid;

  *p->tf = *parent_proc->tf;

  p->tf->rax = 0;

  p->state = RUNNABLE;

  return p->pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
  // your code here
  struct proc* child_proc = myproc(); // this has pid 3
  if (child_proc == NULL) {
    return; // what to do if this fails?
  }

  for (int i = 0; i < NOFILE; i++) {
    if (child_proc->fds[i] != NULL) {
      file_close(i);
    }
  }
  acquire(&ptable.lock);
  for (int i = 0; i < NPROC; i++) {
    if (ptable.proc[i].parent == child_proc->pid) { // change to use pid
      ptable.proc[i].parent = initproc->pid; // change to use pid
    }
  }
  child_proc->state = ZOMBIE;
  wakeup1((void*)(long)child_proc->parent);
  sched();
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
  // get current proc
  // 
  struct proc* parent_proc = myproc();
  if (parent_proc == NULL) {
    return -1;
  }

  // get the ptable lock
  int child_idx = -1;
  acquire(&ptable.lock);
  while(true) {
    bool children_exist = false;
    for (int i = 0; i < NPROC; i++) {
      if (ptable.proc[i].parent == parent_proc->pid) {
        // we have a child
        children_exist = true;
        if (ptable.proc[i].state == ZOMBIE) {
          // clean up
          child_idx = i;
          break;
        }
      }
    }
    if (!children_exist) { // didnt find any children 
      release(&ptable.lock);
      return -1;
    }
    if (child_idx != -1) {
      break;
    }
    //sleep on chan
    sleep((void*)(long)parent_proc->pid, &ptable.lock);
    
  }
  // clean up the proc on ptable[child_idx]
  
  // save childs pid to return
  int child_pid = ptable.proc[child_idx].pid;

  // use vspace free and kfree
  kfree(ptable.proc[child_idx].kstack);
  vspacefree(&ptable.proc[child_idx].vspace);

  // set childs state to UNUSED
  ptable.proc[child_idx].state = UNUSED;
  ptable.proc[child_idx].parent = -1;

  release(&ptable.lock);

  return child_pid;
}

int sbrk(int n) {
  struct proc* p = myproc();
  if (p == NULL) {
    return -1;
  }

  struct vspace* v = &p->vspace;

  uint64_t old_heap =  v->regions[VR_HEAP].va_base + v->regions[VR_HEAP].size;

  if (n > 0) {
      if (vregionaddmap(&v->regions[VR_HEAP], old_heap, n, VPI_PRESENT, VPI_WRITABLE) == -1) {
        return -1;
      }
      v->regions[VR_HEAP].size += n;
      vspaceupdate(v);
      // vspaceinstall(p);
  }

  return old_heap;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
  struct proc *p;

  for (;;) {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      mycpu()->proc = p;
      vspaceinstall(p);
      p->state = RUNNING;
      swtch(&mycpu()->scheduler, p->context);
      vspaceinstallkern();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      mycpu()->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
  int intena;

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1) {
    // Only the ptable lock should be held, thus ncli should be exactly 1, and
    // any other sources of `pushcli` should have gracefully called `popcli`. We
    // enforce that ncli == 1 because otherwise interrupts would be permanently
    // disabled while scheduling the next thread.
    cprintf("pid : %d\n", myproc()->pid);
    cprintf("ncli : %d\n", mycpu()->ncli);
    cprintf("intena : %d\n", mycpu()->intena);

    // If you are getting this output, it most likely means that you're
    // holding a spinlock other than the ptable.lock while trying to
    // enter the scheduler.
    panic("sched locks");
  }
  if (myproc()->state == RUNNING)
    panic("sched running");
  if (readeflags() & FLAGS_IF)
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&myproc()->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
  acquire(&ptable.lock); // DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
  if (myproc() == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock) { // DOC: sleeplock0
    acquire(&ptable.lock);  // DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  myproc()->chan = chan;
  myproc()->state = SLEEPING;
  sched();

  // Tidy up.
  myproc()->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock) { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid) {
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void) {
  static char *states[] = {[UNUSED] = "unused",   [EMBRYO] = "embryo",
                           [SLEEPING] = "sleep ", [RUNNABLE] = "runble",
                           [RUNNING] = "run   ",  [ZOMBIE] = "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint64_t pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state != 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING) {
      getcallerpcs((uint64_t *)p->context->rbp, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

struct proc *findproc(int pid) {
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid)
      return p;
  }
  return 0;
}


// fails on if (!page->cowpageflag) {
// (gdb) 
// 185         return -1;

/*
(gdb) print p->pid
$1 = 2
(gdb) print page
$2 = (struct vpage_info *) 0xffffffff801d7000
(gdb) print *page
$3 = {used = 1, ppn = 472, present = 1, writable = 0, cowpageflag = 1}
*/

/**
(gdb) print p->pid
$4 = 3
(gdb) print page
$5 = (struct vpage_info *) 0xffffffff80186000
(gdb) print *page
$6 = {used = 1, ppn = 472, present = 1, writable = 0, cowpageflag = 1}
*/

/*
(gdb) print p->pid
$7 = 3
(gdb) print page
$8 = (struct vpage_info *) 0xffffffff80185060
(gdb) print *page
$9 = {used = 1, ppn = 470, present = 1, writable = 0, cowpageflag = 1}
*/

/*
(gdb) print p->pid
$10 = 3
(gdb) print page
$11 = (struct vpage_info *) 0xffffffff80186000
(gdb) print *page
$12 = {used = 1, ppn = 478, present = 1, writable = 0, cowpageflag = 1}
*/

/*
(gdb) print p->pid
$13 = 4
(gdb) print page
$14 = (struct vpage_info *) 0xffffffff80210000
(gdb) print *page
$15 = {used = 1, ppn = 478, present = 1, writable = 0, cowpageflag = 1}
*/

/*
(gdb) print p->pid
$16 = 4
(gdb) print page
$17 = (struct vpage_info *) 0xffffffff8020f168
(gdb) print *page
$18 = {used = 1, ppn = 495, present = 1, writable = 0, cowpageflag = 1}
*/

/*
(gdb) print p->pid
$19 = 3
(gdb) print page
$20 = (struct vpage_info *) 0xffffffff80186000
(gdb) print *page
$21 = {used = 1, ppn = 534, present = 1, writable = 0, cowpageflag = 1}
*/

/*
(gdb) print p ->pid
$22 = 5
(gdb) print page
$23 = (struct vpage_info *) 0xffffffff80218000
(gdb) print *page
$24 = {used = 1, ppn = 534, present = 1, writable = 0, cowpageflag = 1}
*/

// (gdb) print p->pid
// $25 = 5
// (gdb) print page
// $26 = (struct vpage_info *) 0xffffffff80210168
// (gdb) print *page
// $27 = {used = 1, ppn = 495, present = 1, writable = 0, cowpageflag = 0}

// 3 is the parent of 5, it seems maybe we double fork issues?
