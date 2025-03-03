# Lab 4: File System

Everything is turned in on **Gradescope**. Remember to answer the lab questions in Gradescope as you work through the lab. One submission per assignment per group, make sure to add your group members on Gradescope!

- [Introduction](#introduction)
- [Configuration](#configuration)
- [Part A: Writable File System](#part-a-writable-file-system)
	- [Disk Layout](#disk-layout)
	- [Inode Cache](#inode-cache)
	- [File Writes](#file-writes)
	- [File Creation](#file-creation)
	- [File Deletion](#file-deletion)
- [Part B: Support Concurrent Filesys Operations](#part-b-support-concurrent-filesys-operations)
	- [Synchronization in inode layer](#synchronization-in-inode-layer)
- [Part C: Crash Safety](#part-c-crash-safety)
	- [Why Might the File System Be Inconsistent?](#why-might-the-file-system-be-inconsistent)
	- [Crash Safety Through Journaling](#crash-safety-through-journaling)
	- [Overall Lab Tips](#overall-lab-tips)
	- [Files to Look At](#files-to-look-at)
- [Hand-in](#hand-in)

## Introduction

The current xk file system has two major restrictions:
1. Files cannot be created, written to, or deleted.
2. There is only one file directory (`root` or `/`).

In this lab, we are only going to address restriction number 1.
After 1 is addressed, 2 will be straightforward to support (though you aren't required to
support it in your implementation).

Supporting modification to the file system is difficult:
writing to a file may require update to multiple disk blocks, 
failure to update any necessary block will leave the system in an inconsistent state.
And even when all necessary blocks are identified and in the process of writing to disk,
the computer may crash mid-update, causing inconsistency in the file system.
To prevent inconsistency, you must also implement crash safe mechanisms so that file system operations 
can be performed in a crash-safe manner.

Please read through the rest of the lab 4 specification and then write your design doc using
the provided template [lab4-design.md](lab4-design.md). When finished with your design document, submit it on **Gradescope** as a pdf. 

Lab 4 code is divided into 3 parts (3 assignments on gradescope) but all share the same deadline. 

> **Warning:** When your code writes to the QEMU simulated disk, QEMU will store the contents
> of the disk in a file on your host system. The next time you run QEMU (i.e.,
> run `make qemu` without `make clean`), any changes made previously will persist.
> Sometimes this will be what you want -- e.g., to test for
> crash safety. At other times, e.g., if you have a bug that leaves the disk in
> a corrupted state, you will want to regenerate the disk to ensure
> you start with the disk in a known state. To regenerate the disk run
> `make clean` to delete the disk and then run `make` to generate a new disk
> image. You are not required to write a file system checker ([`fsck`](https://linux.die.net/man/8/fsck)),
> but you may find it useful to add some code to the xk initialization that
> checks and warns the user if the disk is original or modified.

---

### Configuration

To get lab4 updates, run the following:
```bash
# Switch to the branch you want to merge upstream changes into, assuming `main`
git switch main
git pull upstream main
```

If there are conflicts, you can merge them via `git merge upstream/main`.

---

## Part A: Writable File System

### Disk Layout

To write files you need to understand how they are laid out on disk.

In `xk`'s baseline implementation, all files are laid out on the filesystem disk\* sequentially.
Your first task is to change the disk layout to enable files to be extended
from their initial size.

> \*In `xk` we assume the computer has two storage devices. Device 0 is the boot
> disk which contains the bootloader and `xk` itself, and device 1 is the filesystem root device (ROOTDEV). 

Take a look at `mkfs.c`. `mkfs.c` is a utility program which generates the initial
filesystem disk when you run `make`.
Currently, `xk`'s filesystem disk layout is like this:

	+------------------+  <- number of blocks in the hard drive
	|                  |
	|      Unused      |
	|                  |
	+------------------+  <- block 2 + nbitmap + size of inodes + allocated extents
	|                  |
	|                  |
	|      Extents     |
	|                  |
	|                  |
	+------------------+  <- block 2 + nbitmap + size of inodes
	|                  |
	|    Inode Table   |
	|                  |
	+------------------+  <- block 2 + nbitmap
	|      Bitmap      |
	+------------------+  <- block 2
	|   Super Block    |
	+------------------+  <- block 1
	|    Boot Block    |
	+------------------+  <- block 0

**The boot block** would be used by the bootloader if the filesystem disk was used as our boot disk (it's not).

**The superblock** describes how the disk is formatted.

**The bitmap** is a bit array with 1/0 for whether a particular disk block is used/free.

**The inode table** is a file which stores all file metadata. Every file in the
filesystem has an inode in the inodetable which stores the file's metadata. The
inode table's file content is simply an array of `struct inode`'s (i.e.: `struct
inode` after `struct inode` until EOF). Every inode is uniquely identified by
its index within the inode table. Currently, each inode is 64 bytes and
describes where to find the disk blocks for the file, along with some additional
information about the file (e.g.: file permissions). 

Making the inode table a file leads to some elegant patterns. As the inode table
is itself a file, its metadata is also stored in an inode in the table. The
inode table's metadata is tracked by the first inode in the table
(`inodetable[INODETABLEINO]`). Furthermore, since it's just a plain old file
allocating more inodes is equivalent to writing/appending to the inode table
(i.e.: by implementing file appends, you'll have also implemented inode table
growth).

Directories are also files, and thus have inodes. The root directory's metadata
is tracked by the second inode in the inode table (`inodetable[ROOTINO]`). 

<!-- The super block tracks the range of inode table (start and size), allowing
`iinit` to bootstrap the file system by reading the super block. -->

<!-- Each directory entry (`struct dirent`) holds the file name and inode number -- the index
into the inode table. Note that we have no sub-directories, so you won't have
to implement sub-directories. Also note however that you will have to update the root
directory when adding a new file. -->

**The extents region** stores the data for each file. `mkfs.c`
writes the initial disk image by allocating a single extent large enough for the entire data for each file.
When you support file extension, you will use the disk region beyond the end of the pre-allocated extents.
You can allocate new disk blocks through the provided `balloc` function (`balloc` uses the bitmap sectors to find a free block).

To support file extension, you will likely want to add fields to the on disk inode (`struct dinode`), e.g., to support multiple extents.
You can update the struct to contain an array of extents, or you can use an indirection block to track an array of extents and 
only store a pointer (sector #) to the indirection block in your actual on disk inode. 
There are simply 2 constraints on modification to the disk inode:
1. `struct dinode` must fit within a single sector (<= 512 bytes)
2. `struct dinode`'s size must be a power of 2 (so no inodes will span across multiple sectors).

You are free to choose your own on-disk format. We only require you to maintain
compatibility with the file system call interface; you can change the file
layout however you choose. When you modify `struct dinode` or super block, you must change `mkfs.c` to work with your modification.
Please make sure that your kernel can still pass all the previous tests after your update to `struct dinode`
before making further changes.

> **NOTE:** Be aware that modifications to `struct dinode` must be reflected in `locki` and `struct inode`.

---

### Inode Cache

Since accessing disk is slow, xk keeps cached copies of disk blocks and persistent structures in memory
to achieve better performance. It uses the block cache (`bio.c`) for reading and writing disk blocks (`bread`, `bwrite`),
and maintains an inode cache (`fs.c:icache`). The inode cache is able to cache a maximum of `NINODE` in memory (`icache.inodes`).
Intially all entries of the inode cache is empty (refcount of 0), as inodes are being opened (`iopen`), the inode
cache gets populated, and as inodes are being closed `irelease`, the cache gets free slots back. 
The inode cache also always keeps a cached copy of the inode table's inode (`icache.inodetable`) in memory.

Naturally, as a cache, one would think that the cached inode (`struct inode`) should look exactly the same as the on disk inode (`struct dinode`). 
However, cached inodes need to track additional metadata that are only relevant in memory: in memory refcount, lock for concurrent access, whether the cached 
copy is in sync with the on disk copy (`inode->valid`) and so on. When you update the disk inode with a new layout, 
you must also change the in memory inode to be consistent with the new layout. 

With a cache, any update to the inode must be reflected both in memory (for immediate visibility) and on disk (for persistenting changes).

---

### File Writes

To support write to a file, you want to first handle the case where writing does not change the size of the file ("overwrite").
This does not require new data blocks to be allocated, but only finding the correct existing data block and the offset to write.

Once you can write to a file, the next step is to support writes that extend the file ("append"). Any write with a final offset
beyond the current end of the file results in an append. You will need to allocate enough new blocks to handle the addition
of the new data. We recommend organizing your write logic into two parts: 1) allocate new blocks if needed, and 2) write data to allocated blocks.
But you can also choose to allocate as you write. You may also find it helpful to create a helper function that takes in an inode and 
a file offset and returns the sector containing the offset.

Remove the no write restriction for open, and modify `writei` to support to support file writes. 

---

### File Creation

In addition to writing and appending to files, you will also need to support
creation of files in the root directory. New files are created through the
`O_CREATE` flag with the corresponding R/W permission flags
(`O_RDONLY`, `O_WRONLY` or `O_RDWR`) to the `open` system call. `O_CREATE`
is passed by bitwise OR'ing it with any of the other file modes.
**Take a look at the create test to see how the flags are composed.**
If the file to be created already exist, `open` should just simply open the file
with the given permission instead of creating and overwriting the file.

File creation always results in the allocation of an on disk inode.
It also requires changes to the parent directory's data blocks to add a directory entry
reflecting the new file. And of course, when the parent directory's data is updated,
don't forget to update its metadata (inode) as well! 

<!-- If a new file's name is too long, you should truncate it to fit within the directory entry. -->

Remove the no creation restriction for open, and modify `iopen` to support file creation. 

---

### File Deletion

Finally, you will need to support deleting files from the root directory
with the `unlink` system call. If no processes have a reference (open inode) to the 
given filename, then the file should be deleted and the space it was using should
be made available for reuse. If there is an open reference to the file or
the given filename does not exist in the file system, `unlink` should return
an error.

Note, supporting file deletion means that the inode table may become fragmented.
Your file creation should be able to fill holes from deleted files in the inodetable (as opposed
to always appending to the end) and in the root directory's directory entries.

Implement new syscall `sys_unlink` to support deletion.


```c
/*
 * arg0: char * [path to the file]
 * 
 * Given a pathname for a file, if no process has an open reference to the
 * file, sys_unlink() removes the file from the file system.
 *
 * On success, returns 0. On error, returns -1.
 *
 * Errors:
 * arg0 points to an invalid or unmapped address
 * there is an invalid address before the end of the string
 * the file does not exist
 * the path represents a directory or device
 * the file currently has an open reference
 */
int sys_unlink(void);
```

---

### Part A Conclusion and Testing

After completing the part A exercises, you should be able to pass all tests from `lab4test_a`.

Run the tests by running the `lab4test_a` binary from the shell. **To load the symbols for `lab4test_a` in GDB** use the command `lab4test_a` in GDB.

Expected Output:
```
$ lab4test_a
(lab4.a) > all 
overwrite_test -> passed 
append_test -> passed 
create_small -> passed 
create_large -> passed 
one_file_test -> passed 
four_files_test -> passed 
unlink_basic -> passed 
unlink_bad_case -> passed 
unlink_open -> passed 
unlink_inum_reuse -> passed 
unlink_data_cleanup -> passed 
passed lab4 part a tests
(lab4.a) > 
```

---

## Part B: Support Concurrent Filesystem Operations

### Synchronizing the `inode` Layer

Since `xk` supports multiprocessing, our writable file system must handle concurrency safely. 
If you designed and implemented your Part A with synchronization in mind, you may be able to pass the Part B tests without further modification. 
If not, revisit your Part A code and add synchronization. 

Think about when you would have concurrency issues. What happens when multiple processes trying to create the same file? ...

#### Notes and Hints, Hints and Notes...

*Hint*: Use `locki(ip)` and `unlocki(ip)` when appropriate.

*Note*: You can't acquire a `sleeplock` when you are holding a `spinlock`(read the
code in `sleep()` in `proc.c` and think about why). However, you can acquire a
`sleeplock` or a `spinlock` when you are holding a `sleeplock`. Be extra
careful when you want to hold multiple locks. You might run into a deadlock.

*Hint*: If you must hold multiple locks(there's a high chance you will), order
your lock acquires consistently to avoid deadlock. For example, a valid rule
would be that you always aquire the inode lock(`locki()`) before the block lock
(acquired by calling `bread()`) whenever you need to hold both locks.

*Note*: The delete stress test will take a significant amount of time to
complete (the TA solution takes approximately 5-7 minute) - this is expected. Consider commenting it out while working on concurrency.

---

### Part B Conclusion and Testing

After adding synchronization to part A, all tests in `lab4test_b.c` should pass
when run from the shell.

Run the tests by entering `lab4test_b` in your shell. **To load the symbols for
`lab4test_b` in GDB**, use the `lab4test_b` command.

Expected Output:
```
$ lab4test_b
(lab4.b) > all
concurrent_dup -> passed 
concurrent_create -> passed 
concurrent_write -> passed 
concurrent_read -> passed 
concurrent_delete -> passed 
delete_stress_test -> 
starting delete stress test (this should take around 5-10 min)...
[################################################################################################### ]passed 
passed lab4 part b tests
(lab4.b) > 
```

---

## Part C: Crash Safety

Since file system updates often involve updates to multiple blocks, 
crashing mid-update can leave the file system in an inconsistent state.
The main challenge in file system design is to make it crash-safe: 
the file system metadata (bitmaps and inodes) should always be consistent 
no matter when the computer crashes.

Your goal in Part C is to make the xk file system crash safe. Specifically, our tests will test whether a file creation is crash-safe.
**Note**: You will not *need* to support file deletion to be crash-safe. (Although you are free to do this as well)

### Why Might the File System Be Inconsistent?

Suppose you use the system call `write` to append a block of data to
a file. Appending to a file (may) require changing the bitmap to allocate a new
data block, changing the inode to hold the new file length, and writing the
actual data. The underlying disk system, however, writes a single block
(actually, an individual sector!) at a time. Without crash-safety, the file
could end up with inconsistent data: e.g., the bitmap having allocated the
block but the file doesn't use it, or vice versa.  Or the file length changed,
but the data not written so that a read to the end of the file returns garbage.
A crash-safe filesystem ensures that the file system is either entirely the new
data and entirely old data. (As you can see, previously when the filesystem
was readonly this was always trivially true as it was *always* the old data).

There are several ways to ensure crash-safety. We will talk about several in
lecture. The most common technique (and the one we'll recommend) is to add
journaling. The main idea is that for each multi-block operation (e.g. modifying
a single block as well as the necessary blocks of file system data structures
such as inodes), write each block of the overall operation to a separate area of
the disk (the log) before any of the changes are written to their actual locations.
Once all parts of the operation are in the log, you can safely write the changes 
back to their actual locations in the file system. 

If a failure occurs, on kernel startup, you can read the log to see if
there were any completed operations (meaning all blocks of the multi-block
operation were written to the log before the crash); if so, copy those changes
back to the disk before continuing. In other words, the contents of the log are
idempotent -- able to be applied multiple times without changing the outcome.

---

### Crash Safety Through Journaling

If you choose to implement journaling (which we recommend), here are some general suggestions on how to approach it.

**Disk Layout** We recommend reserving the log region anywhere alongside the metadata. You can add it
in between super block and the bitmap, or between the bitmap and the inode table, or after the inode table.
Check out `param.h` for required size for your log region. The log must be large enough to hold 
all updates (`MAXOPBLOCKS`) done in a single atomic file operation (append, create, etc.). 
You may log a single operation as one transaction for simplicity, or batch multiple operations within a single transaction.

**Log Format** Next, you need to design how your log header should look like. The log header should track metadata of the log: 
the commit status of the current transaction, the size of the log (how many blocks are logged), 
and the actual data location of each logged block. The log header should fit into a single sector.

**API Design.** A good place to put the logging layer is between the `inode` layer and block cache layer (`bio.c`). 
i.e.: `inode` operations would call into your journalling API, and your journalling functions would call into
block cache layer APIs (like `bread` and `bwrite`).

We suggest your journalling layer provide the following API:
- `begin_tx()`: To begin a transaction. Should always have a matching `commit_tx()` call at the end.
- `commit_tx()`: To finish a transaction. Indicates to the journalling layer that all necessary operations have been noted and can be flushed to disk. If this function completes then client can be assured that operation will be reflected on disk.
- `apply_tx()`: To apply a committed transaction, useful in both `commit_tx` and upon start up at recovery time.
- `log_write()`: To indicate a buffer write which should occur atomically as part of the current transaction. Logs the write to the log. **Must** be called while in a transaction.  

With this API, any `xk` code that formerly looked like:
```cpp
// disk operation 1
buf = bread(dev, disk_addr);
memmove(buf->data, addr, BSIZE);
bwrite(buf);
brelse(buf);
// more disk operations that need to be done atomically with
// disk operation 1
// ....
```

Would then become:
```cpp
begin_tx()
// disk operation 1
buf = bread(dev, disk_addr);
memmove(buf->data, addr, BSIZE);
log_write(buf);
brelse(buf);
// more disk operations that need to be done atomically with
// disk operation 1
// ....
commit_tx()
```

**More Details.** The difference between `log_write` and `bwrite` is that `log_write` 
does not write to the actual disk location. `log_write` will write to the log region instead.
`commit_tx()` will  update the log header on disk, then go through the 
log region and write all of the updated blocks from the log region to their 
intended location on disk.

If the machine crashes before the log header is modified,
the system behaves as if the multi-block transaction had not happened. If the 
machine crashes after the log header is written, then after the machine reboots,
`xk` has full knowledge of what the multi-block transaction is in the log so that
`xk` can ensure the transaction is written to disk.

**Buffer Cache Dirty Bit Trick.** Note that whenever a `struct buf`'s `refcnt`
reaches 0, it can be re-allocated at any time to track a different disk sector.
Depending on how you implement your transactions you may have `struct buf`s with
`refcnt == 0` which you don't want the `bcache` to reclaim until they're fully
committed to disk. This can be prevented by setting the dirty bit in the flags
of said `struct buf` with `b->flags |= B_DIRTY`. Once this is done, it will
remain in the cache until the flags are updated to not be dirty. (**Note**: it's
perfectly fine if your implementation doesn't seem to require this trick).

---

### Part C Conclusion and Testing

After finishing all crash-safety implementation, you're ready to test! (Congrats!!)

The part C tests are a bit more complex (and thus are run differently from other tests). 
In your file system, there is a test file called `user/lab4test_c.c`.  
The test code calls a helper system call `crashn` which causes the system to reboot the OS after `n` disk operations.
The test attempts to create a file for different values of `n`. 

To test the crash-safety of your filesystem, run `python3 crash_safety_test.py` from the host machine (*not* within QEMU). 
On success, it should print out `file system is crash-safe`. Otherwise, you can find the test output in `output.txt`.
You can look at `crash_safety_test.py` to see how it works, but in sum it simply runs `make qemu`, then runs `lab4test_c {i}`, with $i \in [1,30]$ in ascending order. 
So you can simulate this manually by doing `make qemu` then doing `lab4test_c {i}` yourself (replacing `{i}` with the desired value).

**To load `lab4test_c` symbols in GDB use the `lab4test_c` command**.

Expected output:
```
clean finished.
make finished.
Running lab4test_c 30 times. test output=output.txt
finished i=1
finished i=2
finished i=3
finished i=4
finished i=5
finished i=6
finished i=7
finished i=8
finished i=9
finished i=10
finished i=11
finished i=12
finished i=13
finished i=14
finished i=15
finished i=16
finished i=17
finished i=18
finished i=19
finished i=20
finished i=21
finished i=22
finished i=23
finished i=24
finished i=25
finished i=26
finished i=27
finished i=28
finished i=29
finished i=30
killing qemu
qemu-system-x86_64: terminating on signal 15 from pid XXXXX (make)
file system is crash-safe
```

---

### Overall Lab Tips

* All previous labs are relevant here: observed bugs may be a product of lab 4 code, but could potentially be from lab 1, 2, or 3, as well.
* Double check that Copy-On-Write traps are allowed in Kernel mode. This will be necessary for these lab tests.
* Crash safety tests relies on output from `lab4test_c`, so just note that modifying the output in this file may affect the crash safety tests.
* Use the `hexdump` command in linux to manually observe the `fs.img` after terminating QEMU. (Alternatively most code editors have extensions that enable you to view hex files).
    * You can use the `blockno` times `BSIZE` to index into the hexdump output (Be sure to convert to hex).
    * For `onefile.txt` you can ensure the inode was written by looking at block `inum * sizeof(struct dinode) + 27 [the inodeno, this may change based on the log section] * BSIZE`.
    * `xk` uses little endian. Read the lines left to right, but read individual bytes right to left.

### Files to Look At

Here are some files you'll probably need to look at/modify for this lab:
* `file.c`: your file layer. You probably need to change `file_open()` to allow
creation, and `file_read()`, `file_write()`, `file_stat()` to ensure disk synchronization.
* `fs.c`: inode layer. You need to implement create file, write file, and append to file here.
* `bio.c`: buffered I/O layer. `bread()`, `bwrite()` and `brelse()` are implemented here (**you should not modify this file!**).
* `file.h`: definition of inode struct.
* `fs.h`: definition of dinode struct. If you change dinode struct, make sure the change is reflected on inode struct as well.
* `extent.h`: definition of extent struct.
* `mfks.c`: this is the program that sets up your disk image. If you make changes
to dinode struct, you need to make sure the use of dinode in this file is still correct.

Note: `mkfs.c` is a standalone linux program which builds the initial file system image (disk image). Its functions are not available from within xk.

---

## Hand-in

### Submission Guide

When you are done, you should be able to pass all tests from `lab4test_a` and `lab4test_b` (run in the xk shell) and `crash_safety_test.py` (run in the host OS shell with `python3 crash_safety_test.py`).

Please submit your answers to the lab questions and a zip of your code to **Gradescope**.

**If you have a partner, make sure to add them to your submissions on Gradescope!**

