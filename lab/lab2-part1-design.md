# Lab 2 Part 1 Design Doc:

**Names:** Aadhithya Arun, Jacob Young

## Overview

> Explain the motivation and the goal for Lab 2. What do these system calls enable? Why do we need to add synchronization to open files first?

To implement fork(), wait(), and exit() to allow multiple processes to run at the same time, which improves system efficiency and productivity. We need to enable synchronization in the open, close, dup, read, write and stat system calls before all of this to make sure these operations can be done safely while multiple processes are running.

### Major Parts

Synchronization For Open Files

  - Goal: Multiple Processes can interact with open files, and the open file maintains the same state across all of them. The processes should be able to safely read and write to files without corrupting the data or the offsets or any other metadata
  - Challenges: Choosing the lock type like spinlocks or sleep locks and determining the critical sections.


Fork

  - Goal: Allows the parent to create a child process which is a duplicate of itself, which lets the kernel take advantage of multiple processors simultaneously.
  - Challenges: Copying the state properly (virtual address space, trapframe). Keeping track of processor relationships, as well as preventing overuse/corruption of shared system resources by maintaining reference counts.


Exit

  - Goal: Provide a way for child processes to terminate itself and release its resources back to the system. This will need to notify the parent that the child process has exited.
  - Challenges: reclaiming the correct resources from the child process so exit() can completely run, Preventing erasure of shared resources across processes. Need to avoid deadlocks.


Wait

  - Goal: Provide a blocking mechanism so parent processes can ensure the child process has exited and clean up their child’s memory before continuing to run
  - Challenges: Making sure the system does not deadlock on a wait, timing of process exits(). Need to handle cases where the parent exits before the child does.


Interactions

  - Fork with File Synchronization: Fork allows multiple processes to have access to the same open file, the state of the open file must be consistent across all processes with the file descriptor
- Fork with exit: Exit() can only be called by a child process created by fork()
- Fork with wait(): wait can only be called by a parent process that has already called fork()
- exit() with wait(): wait() prevents a parent process from executing any more instructions until the child process has successfully run exit. When a child process exits it sets the state to ZOMBIE


<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

## In-depth Analysis and Implementation

### Synchronization For Open Files

**Functions To Implement & Modify**

This section should describe the behavior of the functions in sufficient detail that
another student/TA would be convinced of its correctness. (include any data structures accessed, 
structs modified and specify the type of locks used and which critical sections you need to guard).
> Example: modify `file_open` to lock around access to the global file info table.

- File_open: add locking to protect access to global file table when checking for available file entries or making new file info structs
- File_close: add locking to the global file table during FD cleanup or when updating the ref count
- File_read and File_write: add locking to file entries during read/write operations to prevent corrupted ref counts or data
- File_dup: add locking to the global file table to increment the ref count safely


**Design Questions**

Why does the global file info table need to be protected? 

- We need to protect the global file table so that none of the data it stores is corrupted. We need to make sure fields like the reference count in file_info structs are reliable, and also that we don't concurrently overwrite entries in the table (i.e. two opens trying to initialize a struct at the same time).

What are the trade-offs of using a single lock for all entries of the global file table vs. per-entry lock?

- If we use a single lock on the entire global file table, we ensure only one inode can be altered at a time, preventing deadlock and improving traceability. However, using a per-entry lock allows for multiple processes to run their file operations without having to wait for another process to finish working on a completely different file.

How long are the critical sections for your lock(s)? What operations (read/write memory, or I/O request) are done while holdings the lock(s)?

- The critical section for these locks is the time it takes for one process to complete its operation on the open file. Critical sections for short operations like updating ref counts or offsets will be short. But they may be longer for operations like read and write.

What are your locking decisions (coarse vs fine-grained, spinlock vs sleeplock) for the global file table and why?

- Fine-grained lock answer: We used a fine-grained lock to allow non-conflicting (where a conflict means two operations are writing to the same file_info struct) operations to occur at the same time.
- Sleep lock: The length of file operation can vary depending on the file size, so with operations on very large files in mind, we believe a sleep lock is the best choice. We also know that the underlying inode operations use sleep locks, so if we were to use a spin lock on the table we would not be able to send the wake-up interrupt to the inode sleep lock since spinning locks block interrupts.


Does the per-process file descriptor table need to be protected? Why or why not?

- Currently, the file descriptor table does not need to be protected, as only the owning process can access it and alter its values and we can only run one thread in a process at the same time. This means only one entity can ever change the file descriptor table at one time.
- However once we can have multiple threads running in a process, where each thread can run a system call, then a lock will be needed



**Corner Cases**

Describe any special/edge cases and how your program logic handles these cases.

- Multiple processes pointing with system calls to the same open file want to write/read
  - They will wait for the lock apply their operation then release the lock
- Multiple file_opens/dups/closes at once
  - Locks prevent writing to the same space in the global file table
- Cases where a process tries to access an invalid FD


**Test Plans**

List an order in which you will test the functionality of this component
and give a brief justification as to why your program logic will correctly handle
the listed test cases.

- Run lab1 tests to make sure adding locks maintains correctness
- Run lab2 tests
  - All of these rely on synchronization to be correct to pass
  - Especially care about fork_fd() to make sure the ref counts are expected
  - Also care about fork_wait_exit_stress() as the test tries to open more files than allowed
- Make our own file and test concurrent read and writes on it to make sure offsets and data are updated correctly
- Verify proper cleanup of FDs when exit() is called
- Make sure code is deadlock-proof


<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

### Fork

**Functions To Implement & Modify**

This section should describe the behavior of the functions in sufficient detail that
another student/TA would be convinced of its correctness. (include any data structures accessed, 
structs modified and specify locks used and critical sections you need to guard).

- Fork:
  - allocate a new PCB (struct proc) via allocproc’
    - This initializes state, pid, killed,kstack, context
    - Return -1 if allocproc() fails
  - initialize the child process's virtual address space (vspaceinit)
    - Return -1 if it fails
  - Copy the parent vspace to the child with vspace copy(vspace.c)
  - Copy all of the parent's state, fd table, context values(not the pointer), Tf, state, Parent
    - Init chan to the proc’s address
   - Add child to the process table (scheduler will grab it eventually)
   - Determine if parent or child returns pid of child or 0
      - When copying trap frame from parent to child, have %rax be 0
      - Return child pid (the child won't finish the rest of fork, instead from the scheduler it goes to forkret, trapret, and back to where user called it)

- Sys_fork:
  - Syscall method for fork, check arguments and call above helper function if safe


**Design Questions**

How does a new process become schedulable? Take a look at `scheduler` to see how the scheduler finds candidates to schedule.

- Allocrproc will add the process to the ptable, in order for the scheduler to run the process, we must mark the child as RUNNABLE

At what point in `fork` can a new process be safely set to schedulable? Take a look at `userinit` and see when the first process becomes schedulable.
  
- Once the child process’s resources have been initialized like the trapframe, vspace and kernel stack, and the parents virtual address space has been copied and we have the lock for the ptable, we can set the new process to be schedulable.

Who may access the scheduling state of a process? How does your design protect/synchronize these accesses? 

- Only kernel code such as the scheduler, fork, wait, exec, interrupt handlers, and parent processes may access the state of another process. We enforce the parent process rule by having a field in every process called Parent that is a pointer to a process’s parent PCB. fork will initialize the parent field. We also make sure to use the ptable locks to ensure no process’s information is corrupted

Take a look at `allocproc`, what field(s) of `struct proc` are protected by `ptable.lock` and what are not? Why is this a safe decision?

- The state, pid, and killed are protected by ptable.lock, while fields that are mainly unused memory are not. The pid is locked so in case other procs are being allocated there still is a unique pid for each process, and we maintain that new processes have a larger pid. The state and killed are protected with the lock so that they cannot be preemptively killed or marked to run during initialization.

How do you plan to track the parent child relationship between the calling process and the newly created process?

- The child will be an external proc value, and once its fields have been initialized, it will be sent to the scheduler, and will go through forkret(), then trapret(), then return to the user code.

How does the new process inherit its parent's open files? What needs to be updated as a result?

- The parent process will copy its file descriptor table into the new process’s file descriptor table. The ref_count of the file in the global file table will need to be updated as a result.

**Corner Cases**

- a parent loops calling fork(), tries to create more processes than it should
- Calling open() in a parent should not alter a child’s fd table (close/dup as well)
- Call fork(), have one process write to a shared open file, other process should see the change


**Test Plans**

List an order in which you will test the functionality of this component
and give a brief justification as to why your program logic will correctly handle
the listed test cases.

- Run lab 2 tests
  - fork_wait_exit_basic() will test basic functionality
  - Fork_wait_exit_stress() will stress test our approach, making sure we utilize the concurrency and locks
  - fork_fd_test() will make sure we handle the fd table copying correctly, and that the underlying open file’s changes are reflected in both the parent and child processes


<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

### Exit

**Functions To Implement & Modify**

This section should describe the behavior of the functions in sufficient detail that
another student/TA would be convinced of its correctness. (include any data structures accessed, 
structs modified and specify locks used and critical sections).

- Exit:
  - close all open files
  - Change the procstate to ZOMBIE
  - Send a wakeup to Parent process
    - Parent process is stored as a pointer in the proc
- Sys_exit:
  - System call for exit


**Design Questions**

As a child process, how does the exiting process inform its parent? 

- The child will send a wake-up to the parent's channel,(which will either be waiting and will be woken up by the interrupt or the parent will not have called wait and will eventually clean up the process or pass it to the grandfather initproc process )we can use the parents proc address as the channel
- Or we can access the parent's channel through the parent pointer in the child's proc


As a parent process, how does the exiting process ensure that its children get cleaned up if it exits first?

- The exiting process will go through the page table and for any other proc whose parent pointer value points to the exiting process, it will change the parent pointer to point to the first process (made by user init)
- This should happen even if the child is a ZOMBIE and the parent never called wait(), the parent will still pass the child to initproc


Why can't an exiting process clean up all of its resources?

- In order for exit to completely run it requires some resources like the virtual memory and kernel stack, if the page table is erased while the program is still running the process has no idea where in physical memory to point to.

Is it safe for the child process to access its parent's PCB? What happens if the parent has already exited? 

- If the parent is still alive it is safe for the child process to access the parent’s PCB. However, if the parent exits first, the parent’s PCB will be cleaned up, so the information in the PCB could be for an unrelated process or empty.
- We can avoid this by having each process’s channel be the address of their proc. Thus a child already stores the parent's channel in their parent field of the proc struct, and will not need to access the actual PCB.


What fields of `struct proc` are accessed and/or modified in `exit`? How are these accesses protected?

- The fields exit() accesses is the state to set it to ZOMBIE, the parent field to notify the parent with a wakeup, chan to clear to NULL, the FD array to decrement ref counts, the VSpace is released, the kernel stack is cleaned up as well after the process becomes ZOMBIE. It also accesses the ptable to do this.
- Protection is provided by the ptable lock and because of this, operations like changing the state field to ZOMBIE are also protected. The ref count updates are protected by the global file table locks.



**Corner Cases**

- Parent has already exited before the child. To handle this, we need to reassign the exiting process’s parent to initproc
- Deadlocks between parent and child. There is only one lock (the ptable lock) and both the parent and child will never wait with the lock acquired, the parent will sleep in wait() and reacquire the lock once the child wakes it up, and the child will only update the state and release the lock.
- wakeup



**Test Plans**
List an order in which you will test the functionality of this component
and give a brief justification as to why your program logic will correctly handle
your listed test cases.

- Test for orphaned child. Parents who exit before there child will set the child’s parent values to initcode, which just is infinitely running wait
- fork_wait_exit_cleanup() to make sure we clean up everything needed before setting the child to a ZOMBIE
- fork_wait_exit_stress() to make sure our concurrency is correct and can handle large amounts of data.


<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->

### Wait

**Functions To Implement & Modify**

This section should describe the behavior of the functions in sufficient detail that
another student/TA would be convinced of its correctness. (include any data structures accessed, 
structs modified and specify locks used and critical sections).

- Wait:
  - While a child has not been cleaned up:
    - Acquire ptable lock
    - Find the child process(es) of the parent process in the ptable
    - Check the state of each child
      - If ZOMBIE, break from the loop
      - If no child processes in the table break from the loop and return -1 
    - If children exist but none need to be cleaned up release the lock, sleep on chan
    - Once the parent is woken up go back to the start of the loop
  - Save the child’s pid and free the resources, release the ptable lock
  - Return the child’s pid

- Sys_wait:
  - System call for sys_wait


**Design Questions**

How can a parent process find its children? Does this operation need any synchronization?

- The ptable contains all the processes and the parent field is the link between a child process to the parent
- This part does need synchronization because another process or the scheduler might modify the ptable concurrently. We can use the existing ptable lock to ensure only one process has the ptable at a time.


How do you ensure that a parent process cannot wait on the same child process twice?

- When a parent process finishes the wait on a child, the child’s state is changed to UNUSED. This means that the process slot in the ptable is marked as free and the parent cannot wait on it again


When is it safe to deallocate the child's PCB (`struct proc`)? How do you deallocate a PCB? What fields need to be cleaned up?

- Its safe to do so when the child’s PCB becomes a ZOMBIE and the parent calls wait.
  - We need to clean up the kernel stack, and the virtual memory space


**Corner Cases**

- No children, this means wait should return -1
  - As stated above, wait will return -1 if it doesn't find any processes in the page table that have its address as their parent
- Orphaned process. The children should be “adopted” by initproc
  - wait() will work the same with initproc, however we don’t know if initproc ever calls wait, which means these processes could just take up page table space.


**Test Plans**

List an order in which you will test the functionality of this component
and give a brief justification as to why your program logic will correctly handle
your listed test cases.

- fork_wait_exit_basic() will test the basic functionality of wait()
- fork_wait_exit_tree() will test wait while there are multiple “generations” of processes.
- fork_wait_exit_stress() will really test wait() as it relies on wait to clean up the memory of the numerous child processes it creates.
- We will test the corner cases such as no children and orphaned process
- We can fork multiple children and exit them in random order and test to see if the parent waits on all of them properly


<!-- for formatting, do not remove -->
\newpage
<!-- for formatting, do not remove -->


## Risk Analysis

### Unanswered Questions

- if a parent opens more files after fork, should the child process inherit them too?
- What if the memory allocation fails when calling fork? (Think we return -1)
- Will orphaned Zombie processes ever be cleaned up by initproc?


### Staging of Work

- File Synchronization
  - Add locking to the global file table, and update existing open, dup, close, read, write and state calls to utilize the locks
- Fork
  - Implement functionality to create a child process with copied resources like FDs, vspace, trapframe, etc.
- Exit
  - Implement the exit syscall and clean up all the resources, make sure that it sets process to ZOMBIE and notifies the parent
- Wait
  - Implement functionality to block the parent process until a child process exits
- Testing/Edge cases
  - Now we can run the lab tests. Also, test the edge cases mentioned



### Time Estimation

- File Synchronization (3-4 hours)
- Fork (4-5 hours)
- Exit (4-5 hours)
- Wait (4-5hours)
- Edge cases and Error handling (6-8 hours)

