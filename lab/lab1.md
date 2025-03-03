# Lab 1: Interrupts and System Calls

Everything should be turned in on **Gradescope**. 
Only one person from each group needs to submit, but make sure all group members are added to the submission.

## Introduction

Welcome! This is the CSE 451 lab 1 spec. Specs outline what you need to do to complete the labs.
See [`how_to_lab.md`](/docs/development/how_to_lab.md) for an intro to approaching the labs.

Every lab in this course is built on top of a skeleton operating system (xk) designed for 
educating students about the principles and real-world practices of operating systems.
xk is a 64-bit port of [xv6](https://github.com/mit-pdos/xv6-public) with some minor changes.
It's a simple bootable operating system with a set of basic system calls.
As of now, it is capable of running a single user program that prints out "hello world" to the console.
Throughout the quarter, you will add new functionalities to xk to transform it into a more practical and usable system.

In this lab, you will learn how to set up and debug xk, explore the codebase and its designs, 
and most importantly, implement a number of system calls for xk. There are lab questions in a Gradescope assignment that aim to enhance your understanding of xk.
**As a group**, you should answer these questions on the corresponding Gradescope assignment as you read through the spec.

## Getting Started
The lab environment is set up on the [CSE attu](https://www.cs.washington.edu/lab/linux) machines. 
You can log into it via `ssh netid@attu.cs.washington.edu`. 
If you have trouble accessing attu, please contact the course staff as soon as possible.
You can develop on `attu` using a text editor of your choice.
A common choice is VSCode. Checkout [`vscode.md`](../docs/development/vscode.md) for setup instructions.

Once you are logged in to `attu`, you can clone the course repository by running the commands below.

```
$ git clone git@gitlab.cs.washington.edu:xk-public/25wi.git xk
Cloning into xk...
$ cd xk
```

Next, you need to create your own repository on gitlab instead of working directly on our repo.
**Only one person from each group** needs to do the following:
1. Create a new project on your gitlab with the blank template (**do not** initialize your repo with a README)
2. Set the visibility of your newly created repo to **private**, your code should not be visible to other students
3. Add your team member(s) as Maintainer to your repo
4. Add all course staffs as Developer to your repo, make sure to add via emails (shown [here](https://courses.cs.washington.edu/courses/cse451/25wi/index.html#staff))
5. Populate your repo by running the following commands
  ```
  $ git remote rename origin upstream
  $ git remote add origin git@gitlab.cs.washington.edu:<repo_owner's_uwid>/<repo_name>.git
  $ git push -u origin --all
  ```

Once the group repo is set up, other team members should directly pull the newly created repo with the following commands:
```
$ git clone git@gitlab.cs.washington.edu:<repo_owner's_uwid>/<repo_name>.git
$ cd <repo_name>
$ git remote add upstream git@gitlab.cs.washington.edu:xk-public/25wi.git
```

## Part 1: xk Basics

### The Codebase

The xk codebase is organized as follows:

```
xk
├── inc           // all the header files (i.e., .h); includes definition of all the data structures and the system call interfaces
├── kernel        // the kernel source code
│   └── Makefrag  // compilation scripts for kernel (xk.img), QEMU scripts
├── user          // all the source code for user applications
│   └── Makefrag  // compilation scripts for user applications
├── lab           // specifications for labs
├── Makefile      // starting point of compilation
└── sign.pl       // make sure the boot block is below a block size on the disk
```

After compilation, a new folder `out` will appear, which contains the kernel image and all the intermediate compilation outputs (.d, .o, .asm).

---

### Running xk
For this class, we will run the xk kernel on top of a program that faithfully emulates a complete x86_64 machine 
(the code you write for the emulator will boot on a real machine too).
Using an emulator simplifies debugging; you can, for example, set break points inside the emulated x86_64. 
The emulator we use is the [QEMU Emulator](https://wiki.qemu.org/Main_Page). 
It is available on attu and can be accessed by adding the following line to your shell startup file (`~/.bashrc` for bash).

```
export PATH=/cse/courses/cse451/25wi/bin/x86_64-softmmu:$PATH
```
You can then run 
```
source ~/.bashrc
```
to reload your `bashrc` after the change.

Now you are ready to compile and run xk! 
First, type `make` in your project directory to build xk. Next, type `make qemu` to run xk with QEMU.

You should observe the following output:
```
Booting from Hard Disk..xk...
CPU: QEMU Virtual CPU version 2.5+
  fpu de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2
  sse3 cx16 hypervisor
  syscall nx lm
  lahf_lm svm
E820: physical memory map [mem 0x9000-0x908f]
  [mem 0x0-0x9fbff] available
  [mem 0x9fc00-0x9ffff] reserved
  [mem 0xf0000-0xfffff] reserved
  [mem 0x100000-0xfdffff] available
  [mem 0xfe0000-0xffffff] reserved
  [mem 0xfffc0000-0xffffffff] reserved

cpu0: starting xk

free pages: 3750
cpu0: starting
sb: size 50000 nblocks 49985 bmap start 2 inodestart 15
hello world
```

This means that the xk kernel is successfully running on the emulator and started its first 
user process which prints out `hello world`.

> *Fun facts: How does this work?* On `make`, we first compile the xk kernel and a set of user programs, 
> we then write the bootloader and the kernel binary into the kernel image file `out/xk.img`, 
> and write the user programs and file system metatdata into the file system image file `out/fs.img`.
> On `make qemu`, we specify a number of QEMU options (how much memory does this emulated machine have, how many cpus, 
> direct serial port output to the terminal, etc.), 
> and supply these files to QEMU as the content of the emulated machine's virtual hard disk.

You can exit the emulator instance by doing `ctrl-a, x` (press ctrl and a, release, and then press x). 

---

### Debugging xk

The purpose of this section is to introduce you to the kernel bootstrap process and to get you started with QEMU/GDB debugging.
While QEMU's built-in monitor provides only limited debugging support, 
QEMU can act as a remote debugging target for GDB, which we'll use in this lab to step through the early boot process.

> You can find advice on interpreting common xk error messages and more in [debugging.md](../docs/xk/debugging.md)!

<!-- 
The purpose of the first exercise is to introduce you to the kernel bootstrap process and to get you started with QEMU/GDB debugging.
You will not have to write any code for this part of the lab, but you will use GDB and answer questions.
Write down your answers in a .txt file, each group only needs to submit one submission on Gradescope. -->

<!-- 
### Getting started with x86_64 assembly

The definitive reference for x86_64 assembly language programming is Intel’s
instruction set architecture reference is
[Intel 64 and IA-32 Architectures Software Developer’s Manuals](https://software.intel.com/en-us/articles/intel-sdm).
It covers all the features of the most recent processors that we won’t need in class, but
you may be interested in learning about. An equivalent (and often friendlier) set of
manuals is [AMD64 Architecture Programmer’s Manual](http://developer.amd.com/resources/developer-guides-manuals/).
Save the Intel/AMD architecture manuals for later or use them for reference when you want
to look up the definitive explanation of a particular processor feature or instruction.

You don't have to read them now, but you'll almost certainly want
to refer to some of this material when reading and writing x86_64 assembly. -->

To use GDB on xk, you need to create the file `~/.gdbinit` in your home directory and add the following line:
```
add-auto-load-safe-path /absolute/path/to/xk/.gdbinit
```
(where `/absolute/path/to/xk/` is the absolute path to your xk directory, use `pwd -P` to check your absolute path)

To attach GDB to xk, you need to open two separate terminals, both in the xk root directory.
In one terminal, type `make qemu-gdb`. This starts a QEMU instance and wait for GDB to attach. 
In another terminal, type `make gdb`. Now the GDB process is attached to QEMU. 

Note that when you are using `attu` there are different attu servers that you might be connected to.
Make sure both of your terminals are connected to the same physical attu server (e.g., explicitly using `attu2.cs.washington.edu`).

In xk, when the bootloader loads the kernel from disk to memory, the CPU operates in 32-bit mode. 
The starting point of the 32-bit kernel is in `kernel/entry.S`. 
`kernel/entry.S` setups 64-bit virtual memory and enables 64-bit mode.
You don't need to understand `kernel/entry.S` in detail, but take a look at 
[this section](/kernel/entry.S#L126-139) after the switch to 64-bit.
You will see a call to `main` in `kernel/main.c` which is the starting point of the OS.

---

### The First User Process
xk sets up the first user space program (`user/lab1test.c`) by running `userinit` (`kernel/proc.c`). 

> *Fun facts: How does `userinit` work?* It first calls `allocproc` to allocate a process control block (`struct proc` in xk) for the new process.
> `allocproc` sets up some initial process states in `struct proc` to ensure that the 
> new process can be scheduled and that the same struct will not be re-allocated while the process is still running.
> Once a `struct proc` is allocated for the new process, `userinit` then initializes the virtual address space
> of the new process, loads the initial process's code, sets up its trapframe, and updates the process state to runnable.
> The trapframe is set up so that when the process is scheduled, it knows where to start execution in the user space.

After `userinit` finishes, the process will be scheduled at some point, 
output a "hello world" message and hangs.

```c
int main() {
  printf(stdout, "hello world\n");
  while (1);

  if(open("console", O_RDWR) < 0){
    error("lab1test: failed to open the console");
  }
  dup(0);     // stdout
  dup(0);     // stderr

  // test code below
```

This is the first and only user process running on xk. It is also the testing process for lab1.
Since xk is an operating system, tests for xk are all written as user programs testing kernel functionalities via system calls.
As you implement the system calls in the next part, you should comment out the first two lines to run the tests.


## Part 2: Implement Filesys System Calls

Currently xk supports only a few system calls:
- `kill(int pid)`
  + kill a process with a given process id
- `sleep(int n)`
  + sleep for n clock ticks
- `write(int fd, void *buf, int n)`
  + an incomplete implementation of write that outputs buf to the console

Your task is to implement the following list of system calls so that applications 
can use the disk and file system in a managed and safe way.
- `open(char* filename, int mode)`
  + opens a file with the specified mode, returns a handle (file descriptor) for that open file
- `close(int fd)`
  + closes the open file represented by the file descriptor
- `read(int fd, void *buf, int n)`
  + reads n bytes from an open file into buf
- `write(int fd, void *buf, int n)`
  + writes n bytes from buf into an open file
- `dup(int fd)`
  + duplicate a file descriptor, creating another handle for the same open file
- `fstat(int fd, struct stat* stat)`
  + populate stat with information of the open file

--- 

### System Call Specification

Each supported system call in [inc/user.h](/inc/user.h#L8-L30)
has a corresponding system call handler in the kernel (`kernel/sysfile.c` and `kernel/sysproc.c`).
For this lab, all the system calls you need to implement are in `kernel/sysfile.c`.
We recommend that you implement them in the presented order for testing purposes.

Here's the specification for these system calls:

*open*
```c
/*
 * arg0: char * [path to the file]
 * arg1: int [mode for opening the file (see inc/fcntl.h)]
 *
 * Given a pathname for a file, sys_open() returns a file descriptor, a small,
 * nonnegative integer for use in subsequent system calls. The file descriptor
 * returned by a successful call will be the lowest-numbered file descriptor
 * not currently open for the process.
 *
 * Each open file maintains a current position, initially zero.
 *
 * returns -1 on error
 *
 * Errors:
 * arg0 points to an invalid or unmapped address 
 * there is an invalid address before the end of the string 
 * the file does not exist
 * already at max open files
 * there is no available file descriptor 
 * since the file system is read only, any write flags for non console files are invalid (until lab 4)
 * O_CREATE is not permitted (until lab 4)
 */
int
sys_open(void);
```

*dup* (called by `user/lab1test.c` before it runs tests)
```c
/*
 * arg0: int [file descriptor]
 *
 * Duplicate the file descriptor arg0, must use the smallest unused file descriptor.
 * Return a new file descriptor of the duplicated file, -1 otherwise
 *
 * dup is generally used by the shell to configure stdin/stdout between
 * two programs connected by a pipe (lab 2).  For example, "ls | more"
 * creates two programs, ls and more, where the stdout of ls is sent
 * as the stdin of more.  The parent (shell) first creates a pipe 
 * creating two new open file descriptors, and then create the two children. 
 * Child processes inherit file descriptors, so each child process can 
 * use dup to install each end of the pipe as stdin or stdout, and then
 * close the pipe.
 *
 * Error conditions:
 * arg0 is not an open file descriptor
 * there is no available file descriptor
 */
int
sys_dup(void);
```

*close*
```c
/*
 * arg0: int [file descriptor]
 *
 * Close the given file descriptor
 * Return 0 on successful close, -1 otherwise
 *
 * Error conditions:
 * arg0 is not an open file descriptor
 */
int
sys_close(void);
```

*read*
```c
/*
 * arg0: int [file descriptor]
 * arg1: char * [buffer to write read bytes to]
 * arg2: int [number of bytes to read]
 *
 * Read up to arg2 bytes from the current position of the corresponding file of the 
 * arg0 file descriptor, place those bytes into the arg1 buffer.
 * The current position of the open file is then updated with the number of bytes read.
 *
 * Return the number of bytes read, or -1 if there was an error.
 *
 * Fewer than arg2 bytes might be read due to these conditions:
 * If the current position + arg2 is beyond the end of the file.
 * If this is a pipe or console device and fewer than arg2 bytes are available  (lab 2)
 * If this is a pipe and the other end of the pipe has been closed. (lab 2)
 *
 * Error conditions:
 * arg0 is not a file descriptor open for read 
 * some address between [arg1, arg1+arg2) is invalid
 * arg2 is not positive
 */
int
sys_read(void);
```

*write*
```c
/*
 * arg0: int [file descriptor]
 * arg1: char * [buffer of bytes to write to the given fd]
 * arg2: int [number of bytes to write]
 *
 * Write up to arg2 bytes from arg1 to the current position of the corresponding file of
 * the file descriptor. The current position of the file is updated by the number of bytes written.
 *
 * Return the number of bytes written, or -1 if there was an error.
 *
 * If the full write cannot be completed, write as many bytes as possible 
 * before returning with that number of bytes.
 *
 * If writing to a pipe and the other end of the pipe is closed,
 * return -1. (lab 2)
 *
 * Error conditions:
 * arg0 is not a file descriptor open for write
 * some address between [arg1,arg1+arg2-1] is invalid
 * arg2 is not positive
 *
 * note that for lab1, the file system does not support writing past 
 * the end of the file (or at all). Normally this would extend the size of the file
 * allowing the write to complete, to the maximum extent possible 
 * provided there is space on the disk.
 */
int
sys_write(void);
```

*fstat*
```c
/*
 * arg0: int [file descriptor]
 * arg1: struct stat *
 *
 * Populate the struct stat pointer passed in to the function
 *
 * Return 0 on success, -1 otherwise
 *
 * Error conditions: 
 * if arg0 is not a valid file descriptor
 * if any address within the range [arg1, arg1+sizeof(struct stat)] is invalid
 * if the file descriptor is a pipe file descriptor (lab 2)
 */
int
sys_fstat(void);
```
Next, we will walk you through necessary steps needed to implement these functions. 

--- 

### System Call Code Flow
In xk, when a process invokes a system call listed in [user/user.h](/inc/user.h)
it goes to the provided system call stubs in [user/lib/usys.S](/user/lib/usys.S). 
The syscall stub writes the specific system call number (defined in `inc/syscall.h`) to the register `%eax` (line 7) and mode switches into the kernel
through a software interrupt (line 8) with the interrupt number for system calls (`TRAP_SYSCALL` in [inc/trap.h](/inc/trap.h#L30)).

Once in kernel mode, the software interrupt is captured by the registered trap vector ([kernel/vectors.S](/kernel/vectors.S#L316-L319)).
The trap vector saves the interrupt number (`TRAP_SYSCALL`) and the process's registers onto its kernel stack (`alltraps`), 
creating the [trapframe](/inc/trap.h#L41-L66).
After the trap frame is set up, `kernel/trap.c:trap` is called, which invokes `kernel/syscall.c:syscall` due to the `TRAP_SYSCALL` trap number.
The `syscall` function then demux the call to the respective handler in `kernel/sysproc.c` or `kernel/sysfile.c`.

### System Call Arguments
Now that we are in a system call handler, how do we get the arguments? 

When a system call is invoked from user space, the system call arguments are written to registers (`%rdi`, `%rsi`, and so on) following the x86_64 calling convention.
Upon entering the kernel, the value of all registers are saved onto the kernel stack (as part of the trap frame) to ensure that user process's states can be restored when
it returns back to the user space. To fetch syscall arguments, we just need to read the saved registers from the trap frame (see `kernel/syscall.c` for existing helper functions).

---

### File Descriptors 

After parsing and validating system call arguments, it's time to dive into the specifics.
You might notice that many of these syscalls take in a file descriptor (`fd`) as an argument, so what is it?
A file descriptor is a kernel managed identifier (an integer) for an open file upon a successful call to `open`.
A user can request operations like read and write to be performed on an open file by providing its fd.
File descriptors are unique to each process, meaning that the same fd value can represent different open files for different processes.

In this lab, you are responsible for implementing `open` and `close`, which means you need to manage the allocation and deallocation of file descriptors. 
Each process should be able to have `NOFILE` ([inc/param.h](./inc/param.h)) amount of open files.
You need to create a mapping between an fd and an open file upon a successful `open`, retrieve the corresponding open file to operate on for syscalls with a fd,
and remove the mapping when upon a call to `close`. Keep in mind that although the kernel hands out valid file descriptors, user processes may or may not
be faithful in providing a valid file descriptor when requesting system calls. Make sure you perform validation for each fd argument.
Since the file descriptor namespace (allocable integers) is per process, you can extend `struct proc` to contain a mapping of file descriptors to open files.

> *Fun facts: Why use file descriptors?* So far we've been looking at `open`, `close`, and `dup` system calls as file system operations, 
> but in fact they can be done for other forms of I/O (console, socket, etc) as well. 
> Despite the name "file descriptor", it is really an "I/O descriptor".
> This enables applications to interact with various kinds of I/O using the same interface.
> The console is simply a file (file descriptor) from the user application's point of view,
> and reading from keyboard and writing to screen is done through the same `read`/`write` system calls.

### The Open File Abstraction

The existing xk file system (`kernel/fs.c`) already provides us with a notion of a file (as `inode`) and code to operate on a file (really `inode`), 
so why do we need an additional abstraction for files that have been opened? This is because a user process may want to
open the same file on disk with different modes (read only, write only, etc.) and enjoy a higher level interface than 
the underlying file system provides (e.g. syscall read implicitly moves read offset forward so that user doesn't need to
always specify where to start reading). Notice there is a difference between the system call `open(filename, mode)` and its corresponding file system operation `iopen(filename)`.

You are responsible for creating the open file abstraction to support these richer semantics. The richer semantics include:
- allowing the same file to be opened in different modes (`open`)
- implicitly advancing the file offset: the offset starts at 0 and advances by `n` after `n` bytes have been read or written (`read`, `write`)
- adding a reference (file descriptor) to the same open file (`dup`)
- handling allocation and deallocation of an open file (`open`, `close`, `dup`)
  - `open` allocates an open file
  - `close` removes a reference to an open file, deallocates when there are no more references left
  - `dup` adds a reference to an open file

We recommend that you create a `struct file_info` to represent an open file, `inc/file.h` may be a good place to define it.
Your struct should track a pointer to the open file's underlying inode and metadata to support the semantics described above.

Once you define your struct, you need to manage its allocation and deallocation. 
Kernel typically has a hard limit on the total number of open files across all processes, in xk, we require you to support `NFILE` (`inc/param.h`)
amount of total open files. This means that you can allocate a static array of `struct file_info` as your global file info table.
You can then manage the allocation and deallocation of open files. Make sure that you initialize all fields of your struct upon an allocation or deallocation.

We highly recommend that you declare the following functions in `kernel/file.c` to invoke after you have validated the system call
arguments and retrieved the open file (given a file descriptor):

- `file_write`, `file_read`:
  - checks whether the file mode allows for the operation
  - invokes the underlying inode operation
  - advances file offset
- `file_open`:
  - opens the file/inode through the file system (`iopen`)
  - checks whether the requested mode is allowed for the file/inode type
    - only type `T_DEV` can be opened with writable mode (see explanation in the next section)
  - allocates an open file and a file descriptor, creating the mapping
  - upon any error, call `irelease` to "return the inode" back to the filesys
- `file_close`:
  - removes the fd to open file mapping, deallocates the open file if needed
  - upon an open file deallocation, make sure to "return the inode" back to the filesys (`irelease`)
- `file_dup`:
  - allocates and maps a new fd to the open file
    - should have no effect on the underlying inode's reference count (no new inode reference is given out by the filesys)
- `file_stat`:
  - invokes the underlying inode operation 

For any new functions you declare, add the declarations in `inc/defs.h` if you would like them to be used by other files. 

### The xk File System
The existing xk file system is a read-only file system. Since it's read only,
writes are not supported on files, but can be done on the console device.

It supports a number of operations (see `kernel/fs.c`) on inodes, 
which are used to implement files, directories, and the console device.
For example, to open a file, you can call `iopen`, which performs a lookup from the
root directory of the file system. Once the file is found, a `struct inode*`
is returned as a handle so that you can request more file system operations with it.

For this lab, you will be using `iopen`, `irelease`, `concurrent_readi`, `concurrent_writei`, 
`idup`, and `concurrent_stati` to implement the file functions listed above.

> *Fun facts: How are there any files if writes are not supported?* If you recall, when we run xk and QEMU, we give it an initial file system 
> image `out/fs.img` written by [mkfs.c](/mkfs.c).
> `mkfs.c` runs on our normal linux machine and writes a number of user programs plus other files in a format that the xk file system understands.

Now you should have all the information you need to complete this lab :)


## Testing

For this lab, the solution makes changes to `sysfile.c`, `file.c`, `proc.h`, `defs.h`, and `file.h`. You may change more or fewer files.
As you implement the system calls described above, you should comment out the first two lines of `lab1test.c:main` so the tests can run.
Run `make qemu` and you should see what tests you are currently passing.
Once all tests pass, you should see `passed lab1 tests!`.

## Hand-in

### Submission Guide
You need to submit both your answers to the questions and a zip of your repo to **Gradescope**.

> You may find it useful to tag your submission for future reference, but you will only be graded on what you submit to Gradescope. 
To tag the latest commit, use `git tag lab1_final && git push origin main --tags`.

To create the zip file, run `make clean` and then `zip -r submission.zip *` in your project root dir. Or you can just download your repo from GitLab by clicking code -> zip.
Make sure to run `make clean` if you are creating the zip yourself, the autograder fails if the upload is too large (typically happens when the `out` folder is included). 

**If you have a partner, make sure to add them to your submissions on Gradescope!**
