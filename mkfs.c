/**
 * @file mkfs.c
 * 
 * `mkfs` is a helper program which runs natively on the host machine
 * (*outside* of the qemu VM) in order to produce the initial filesystem
 * image.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>

typedef unsigned long  ulong;
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

#define stat xk_stat  // avoid clash with host struct stat
#include <inc/fs.h>
#include <inc/stat.h>
#include <inc/param.h>

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

// "Inodes Per Block" (IPB).
#define IPB (BSIZE / sizeof(struct dinode))
// The console device ID.
#define CONSOLE 1

// Disk layout:
// [ boot block | sb block | free bit map | inode file start | data blocks ]

// Number of blocks the bitmap will need (needs to have enough space to
// have a bit for every block in the filesystem disk).
int nbitmap = FSSIZE/(BSIZE*8) + 1;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
// Global counter for next free inode index. Incremented by `ialloc()`.
uint freeinode = 0;
uint freeblock;


void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iallocblocks(uint inum, int start, int numblks);
void iappend(uint inum, void *p, int n);

// Returns a ushort which is the little-endian representation
// of x. On a little-endian architecture the return value
// is the exact same as `x`.
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

// Returns a uint which is the little-endian representation
// of x. On a little-endian architecture the return value is
// the exact same as `x`.
uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

// Entrypoint for `mkfs`. Command invocation:
//   `mkfs output_filename [input_files...]`
// 
// Arguments:
//   - output_filename: First positional arg must be a file to write the output
//     to.
//   - input_files...: Zero or more filepaths to files which should be included
//       in the filesystem image. Currently we assert that all input_files are
//       located under the `out/user` directory.
//
// e.g.: `mkfs fs.img out/user/small.txt out/user/_echo`
int
main(int argc, char *argv[])
{
  int i, cc, fd;
  uint inodetableino, inodetableblkn;
  uint rootino, rootdir_size, rootdir_blocks;
  uint inum, off;
  uint inum_count;
  struct dirent de;
  // Sector-sized scratch space to use for writing values to disk.
  char buf[BSIZE];
  struct dinode din;
  struct dinode *root;


  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if(argc < 2){
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  // Open/create the file which we'll use as our filesystem disk for our
  // qemu VM.
  fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }

  // ================================
  // Superblock setup
  // ================================

  // 1 fs block = 1 disk sector (i.e.: filesystem blocks are disk sectors)

  // `nmeta` is the total number of metadata blocks which will always be allocated.
  // For now it's just: bootblock(1) + superblock(1) + nbitmap.
  // NOTE: if you add crash safety datastructures which need static block
  // allocations that should be included under nmeta.
  nmeta = 2 + nbitmap;

  // `nblocks` tracks the remaining number of unallocated blocks as we're building
  // the filesystem image.
  nblocks = FSSIZE - nmeta;

  // Setup the superblock.
  // NOTE: if you modify the `struct superblock` definition make sure to initialize
  // fields how you'd expect here (good for tracking things like journal size and
  // start block).
  sb.size = xint(FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.bmapstart = xint(2);
  sb.inodestart = xint(2+nbitmap);

  printf("nmeta %d (boot, super, bitmap blocks %u) blocks %d total %d\n",
       nmeta, nbitmap, nblocks, FSSIZE);

  // Index of the first free block we can use. We'll update it as we allocate
  // blocks in the filesystem.
  freeblock = nmeta;

  // Default 0 initialize the entire filesystem disk.
  for(i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  // Write the superblock to diskblock 1 (which by contract of our `xk` filesystem
  // is where superblock must always live).
  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  // ================================
  // Adding inodetable
  // ================================

  // Besides first two arguments (name of command and output file), all
  // arguments are files to include in the initial filesystem image.
  //
  // Additionally we need to include an inode for:
  // - The inodetable
  // - The rootdir
  // - The console
  //
  // Thus `inum_count = argc - 2 files + 1 inode file + 1 root dir + console = argc + 1`
  inum_count = argc + 1;
  printf("inum_count %d\n", inum_count);

  // Create the inodetable dinode.
  inodetableino = ialloc(T_FILE);
  assert(inodetableino == INODETABLEINO);

  // Properly setup the inodetable's dinode (primarily the size and extent).
  rinode(inodetableino, &din);
  din.data.startblkno = sb.inodestart;
  // Calculate how many blocks long will the inodetable need to be.
  inodetableblkn = inum_count/IPB;
  // Round up (since above does integer division).
  if (inodetableblkn == 0 || (inum_count * sizeof(struct dinode) % BSIZE))
    inodetableblkn++;
  din.data.nblocks = xint(inodetableblkn);
  din.size = xint(inum_count * sizeof(struct dinode));
  winode(inodetableino, &din);

  // Update freeblock cursor by the number of blocks we allocated to inodetable.
  freeblock += inodetableblkn;

  // ================================
  // Adding Root Directory
  // ================================

  // Now add the root directory.
  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);

  // Similar math for `inum_count`, argc - 2 files from command line, as
  // well as one for ".", one for "..", and one for console, that all
  // need to go into root directory. So total of `argc+1`.
  // argc - 2 directory entries + 2 for '.' and '..' + 1 for console
  rootdir_size = ((argc + 1) * sizeof(struct dirent));
  rootdir_blocks = rootdir_size / BSIZE;
	if (rootdir_size % BSIZE)
		rootdir_blocks += 1;
  iallocblocks(rootino, freeblock, rootdir_blocks);
  freeblock += rootdir_blocks;

  // Add dinode for the "." directory.
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  // Add dinode for the ".." directory.
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  // Setting up the console inode and adding to directory
  inum = ialloc(T_DEV);
  rinode(inum, &din);
  din.devid = xshort(CONSOLE);
  winode(inum, &din);

  bzero(&de, sizeof(de));
  de.inum = xshort(inum);
  strncpy(de.name, "console", DIRSIZ);
  iappend(rootino, &de, sizeof(de));

  // ================================
  // Adding Files Passed from Commandline
  // ================================
  for(i = 2; i < argc; i++){
    char *name = argv[i];

    // Assert that the path to file starts with `out/user/`.
    assert (!strncmp(name, "out/user/", 9));
    name += 9;

    // `index` returns pointer to first occurrence of character in string,
    // this check asserts that there are no more subdirectories in the
    // path after `out/user/`.
    assert(index(name, '/') == 0);

    if((fd = open(argv[i], 0)) < 0){
      perror(argv[i]);
      exit(1);
    }

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    if(name[0] == '_')
      ++name;

    inum = ialloc(T_FILE);

    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, name, DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    rinode(inum, &din);
    din.data.startblkno = xint(freeblock);
		winode(inum, &din);

    while((cc = read(fd, buf, sizeof(buf))) > 0)
      iappend(inum, buf, cc);

    rinode(inum, &din);
    din.data.nblocks = xint(xint(din.size) / BSIZE + (xint(din.size) % BSIZE == 0 ? 0 : 1));
    freeblock += xint(din.data.nblocks);
    winode(inum, &din);

		printf("inum: %d name: %s size %d start: %d nblocks: %d\n",
        inum, name, xint(din.size), xint(din.data.startblkno), xint(din.data.nblocks));
    close(fd);
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  din.size = xint(rootdir_size);
  winode(rootino, &din);

  rinode(inum, &din);
  printf("inum: %d size %d start: %d nblocks: %d\n",
      inum,xint(din.size), xint(din.data.startblkno), xint(din.data.nblocks));

  balloc(freeblock);

  exit(0);
}

// Write the provided buffer (which must be at least size BSIZE) into the
// corresponding "sector" `sec` on the filesystem disk file.
void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(write(fsfd, buf, BSIZE) != BSIZE){
    perror("write");
    exit(1);
  }
}

// Write the contents of the dinode pointed to by `ip` to where the dinode
// with inum `inum` is located on disk.
//
// (NOTE: this function assumes that all inodes are placed contiguously starting
// at `sb.inodestart`, so block `xint(sb.inodestart) + (INODEOFF(inum) / BSIZE)`
// must be the `(INODEOFF(inum) / BSIZE)`'th block of the inodetable. More
// simply: make sure all dinodes are `ialloc`'ed before allocating blocks for
// other stuff like file contents).
void
winode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = xint(sb.inodestart) + (INODEOFF(inum) / BSIZE);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

// Read the inode with corresponding inum from disk.
//
// NOTE: we assume that inodetable is contiguous starting from `sb.inodestart`
// and that `inum` is within `inodetable`'s size.
void
rinode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint off, bn;
  struct dinode *dip;

  bn = xint(sb.inodestart) + (INODEOFF(inum) / BSIZE);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

// Read the corresponding "sector" from the filesystem disk file into
// the provided buffer (which must be at least size BSIZE).
void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, BSIZE) != BSIZE){
    perror("read");
    exit(1);
  }
}

// Create a dinode with the provided type with the next free inum,
// and write it to the corresponding location on disk.
//
// In `mkfs` all dinodes are allocated and initialized this way, so if you
// modify `struct dinode` you should make sure to update this function to
// appropriately initialize your new field.
//
// NOTE: this function relies on global `freeinode` counter, so order in which
// it is called matters (e.g.: since inodetable must have `inum=0`, it must be
// first `ialloc` call).
uint
ialloc(ushort type)
{
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.type = xshort(type);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

#define min(a, b) ((a) < (b) ? (a) : (b))

// Marks the first `used` bits in the bitmap as allocated.
// (This function is expected to just be called once at the end of
// `mkfs` once the total number of blocks to allocate is determined).
void
balloc(int used)
{
  uchar buf[BSIZE];
  int nbuf = 0;
  int i;
  int remaining = used;

  printf("balloc: first %d blocks have been allocated\n", used);

  while (remaining > 0) {
    bzero(buf, BSIZE);
    for(i = 0; i < min(remaining, BSIZE*8); i++){
      buf[i/8] = buf[i/8] | (0x1 << (i%8));
    }
    printf("balloc: write bitmap block at sector %d\n", sb.bmapstart + nbuf);
    wsect(sb.bmapstart + nbuf, buf);
    nbuf ++;
    remaining -= BSIZE * 8;
  }
}

// Update the extent for the dinode with the given `inum` to have
// the provided start block and number of blocks.
//
// NOTE: this function assumes that inum is contained within the
// size of the inodetable.
void
iallocblocks(uint inum, int start, int numblks) {
  struct dinode din;
  rinode(inum, &din);
  din.data.startblkno = xint(start);
  din.data.nblocks = xint(numblks);
  winode(inum, &din);
}

// Append n bytes pointed to by `xp` to the file identified by `inum`.
// NOTE: this function just continues to append to wherever
// file.startblkno + file.size would be. Must make sure that space
// is available and unallocated.
void
iappend(uint inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];

  rinode(inum, &din);
  off = xint(din.size);
  printf("append inum %d at off %d sz %d\n", inum, off, n);
  while(n > 0){
    fbn = off / BSIZE;
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(xint(din.data.startblkno) + fbn, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(xint(din.data.startblkno) + fbn, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
