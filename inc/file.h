#pragma once

#include <extent.h>
#include <sleeplock.h>

#include <param.h>

// The in-memory inode structure. Every file within the `xk`
// filesystem is represented by an in-memory inode. All `struct inode`s
// correspond to an on-disk inode structure. For more info see `struct dinode`
// in `fs.h`.
struct inode {
  // Device number of the inode. Each hard drive attached to the computer has a
  // device number. Only meaningful for inodes that represent on-disk files.
  uint dev;
  // The inode number. Every inode has a unique inum. The inum is the index of
  // the inode within the inodetable. Thus INODEOFF(inum) gives the offset into
  // the inodetable at which the on-disk inode's data is located.
  uint inum;
  // The number of in-memory references to this in-memory `struct inode`.
  int ref;
  // Tracks whether or not the inode is valid. 1 if valid, 0 otherwise. A
  // `struct inode` is valid once it has been populated from disk using `locki`.
  int valid;
  // The lock for this inode. Should only be used after initialized by `iinit`.
  struct sleeplock lock;

  // Copy of information from the corresponding disk inode (see `struct dinode`
  // in fs.h for details).
  short type; 
  short devid;
  uint size;
  struct extent data;
};


// create a new one on opne
// dup just points to the same one pointed to by the other fd
struct file_info {
  struct inode * in; // pointer to the underlying inode

  // lock for file
  struct sleeplock lock;
  
  // field for which mode the file is in
  int mode;

  // underlying offset in the file
  int offset;

  // how many file descriptors point to this
  int ref_count;

  struct pipe * p;
};

struct pipe {
  char * buffer;
  int read_offset;
  int write_offset;
  int available_bytes;
  bool read_open;
  bool write_open;
  void* active_writer;
  void* active_readers;
  struct spinlock pipe_lock;
};

// table mapping device ID (devid) to device functions
struct devsw {
  int (*read)(struct inode *, char *, int);
  int (*write)(struct inode *, char *, int);
};

extern struct devsw devsw[];

// static struct file_info global_files[NFILE];


// Device ids
enum {
  CONSOLE = 1,
};
