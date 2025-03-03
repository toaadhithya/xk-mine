// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// Some functions take a `dev` argument. This specifies which storage device
// to read from. For how we run `xk` there are two disks, the boot disk
// and the filesystem root disk (ROOTDEV). If you're doing filesystem operations
// you'll want to use ROOTDEV.
//
// The implementation uses two state flags internally:
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include <cdefs.h>
#include <defs.h>
#include <fs.h>
#include <param.h>
#include <sleeplock.h>
#include <spinlock.h>

#include <buf.h>

int crashn_enable = 0;
int crashn = 0;

int num_disk_reads = 0;

// In-memory buffer cache. See `struct buf` for more
// details on what `buf`'s represent.
struct {
  // Lock which protects the `refcount` field of all buf's
  // in `buf` array, as well as the linked list of buffers.
  struct spinlock lock;
  struct buf buf[NBUF];

  // Dummy node used to access the MRU doubly linked list of bufs.
  //
  // The bufs in `this->buf` form a doubly linked list for MRU access. (If you
  // include `head`, all bufs form a doubly linked loop). head.next is
  // most recently used, head.prev is least recently used.
  // `brelse`'ing a buf moves it to the head of the linked list.
  //
  // We use this data structure to take advantage of temporal locality. We
  // assume that recently `brelse`'d buffers are more likely to be `bget`'ed in
  // the near future.
  struct buf head;
} bcache;

void binit(void) {
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for (b = bcache.head.next; b != &bcache.head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached; recycle some unused buffer and clean buffer
  // "clean" because B_DIRTY and not locked means log.c
  // hasn't yet committed the changes to the buffer.
  for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
    if (b->refcnt == 0 && (b->flags & B_DIRTY) == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->flags = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  num_disk_reads += 1;
  struct buf *b;

  b = bget(dev, blockno);
  if (!(b->flags & B_VALID)) {
    iderw(b);
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (crashn_enable) {
    crashn--;
    if (crashn < 0)
      reboot();
  }
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  b->flags |= B_DIRTY;
  iderw(b);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }

  release(&bcache.lock);
}

void enable_crashn(int n) {
  crashn_enable = 1;
  crashn = n;
}

// Print the data at the given block.
// Format: block_no, byte index, data
// Note: Data stored in blocks on disk are in little endian.
void print_data_at_block(uint block) {
  cprintf("Printing data at block=%d\n", block);
  uint64_t data[BSIZE / 8];
  struct buf* b = bread(ROOTDEV, block);
  memmove(data, b->data, BSIZE);
  brelse(b);
  for (int i = 0; i < BSIZE/8; ++i) {
    cprintf("block=0x%x index=%d: %lx\n", block, i, data[i]);
  }
}
