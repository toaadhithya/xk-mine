//
// File descriptors
//

#include <cdefs.h>
#include <defs.h>
#include <file.h>
#include <fs.h>
#include <param.h>
#include <proc.h>
#include <sleeplock.h>
#include <spinlock.h>

#include<fcntl.h>

struct devsw devsw[NDEV];

static struct file_info global_files[NFILE];

// with open we add a new file_info to the global array, and the fd for the
// process array and return a fd for the file (iopen), (irealease if any error)
int file_open(char* path, int mode) {
  // cprintf("in file open\n");
  struct file_info entry;

  // no O_CREATE
  if (mode == O_CREATE) {
    return -1;
  }

  // no write mode unless its to console
  if (mode != O_RDONLY && strncmp(path, "console", 7) != 0) {
    return -1;
  }

  // find the inode pointer
  struct inode* in = iopen(path);
  if (in ==NULL) {  // iopen returns NULL if not found
    return -1;
  }

  struct proc* process = myproc();
  if (process == NULL) {
    irelease(in);
    return -1;
  }

  entry.in = in;
  entry.mode = mode;
  entry.offset = 0;
  entry.ref_count = 1;  // init to 1 for this user
  entry.p = NULL;

  int i;  // declare outside so we can get ptr to file_info in the array

  for (i = 0; i < NFILE; i++) {
    acquiresleep(&global_files[i].lock);
    entry.lock = global_files[i].lock;
    // locked -> 1, and the locks pid = process pid
    if (global_files[i].ref_count == 0) {  // this way we don't have to do any cleanup
      global_files[i] = entry;
      releasesleep(&global_files[i].lock);
      break;
    }
    releasesleep(&global_files[i].lock);
  }

  if (i >= NFILE) {
    irelease(in);
    return -1;  // no space
  }

  struct file_info* file =
      &global_files[i];  // location of the struct in the array

  int fd = 0;
  while (fd < NOFILE) {
    if (process->fds[fd] == NULL) {
      process->fds[fd] = file;
      break;
    }
    fd++;
  }

  if (fd >= NOFILE) {
    irelease(in);
    return -1;  // no space
  }

  return fd;
}

// dup: adds a new fd pointer to the same location in the file_info array,
// increment ref count on file_info
int file_dup(int fd) {
  // cprintf("in file dup\n");
  struct proc* process = myproc();
  if (process == NULL) {
    return -1;
  }

  int fd_index = 0;
  while (fd_index < NOFILE) {
    if (process->fds[fd_index] == NULL) {
      process->fds[fd_index] = process->fds[fd];
      break;
    }
    fd_index++;
  }

  if (fd_index > NOFILE) {
    return -1;  // no space
  }

  acquiresleep(&process->fds[fd_index]->lock);
  process->fds[fd_index]->ref_count++;  // update the reference count
  releasesleep(&process->fds[fd_index]->lock);

  return fd_index;
}

// close: subtract from refcount, if 0 remove from global array, we remove the
// fd, (irelease) Can we be lazy with this? i.e. leave it in the array with a
// refcount of 0 and in of NULL?
int file_close(int fd) {
  // cprintf("in file close\n");
  struct proc* process = myproc();
  if (process == NULL) {
    return -1;
  }

  acquiresleep(&process->fds[fd]->lock);

  // if pipe call pipe_close
  if (process->fds[fd]->p != NULL && process->fds[fd]->p != 0x0) {
    // releasesleep(&process->fds[fd]->lock);
    return pipe_close(process, fd);
  }

  process->fds[fd]->ref_count--;

  // in irelease i noticed the inode also has a reference count, should we be
  // updating that?
  if (process->fds[fd]->ref_count == 0) {
    irelease(process->fds[fd]->in);
    process->fds[fd]->in = NULL;
  }
  releasesleep(&process->fds[fd]->lock);

  
  process->fds[fd] = NULL;

  return 0;
}

// file  read: check the mode of the file_info, update offset as we read, call
// inode read. concurrent_readi
int file_read(int fd, char* buf, int n) {
  // cprintf("in file read\n");
  if (n < 0) {
    return -1;
  }
  struct proc* process = myproc();
  if (process == NULL) {
    return -1;
  }

  struct file_info* fi = process->fds[fd];
  // read is only possible if in RO or WRO mode
  if (fi->mode != O_RDONLY && fi->mode != O_RDWR) {
    return -1;
  }

  // trigger pipe_read
  if (fi->p != NULL) {
    return pipe_read(fd, buf, n,process);
  }

  acquiresleep(&fi->lock);
  int bytes_read = concurrent_readi(fi->in, buf, fi->offset, n);
  fi->offset += bytes_read;  // add the offset
  releasesleep(&fi->lock);
  return bytes_read;
}

// file write: chase the pointer to the file's inode pass in the params to
// concurrent_writei
int file_write(int fd, char* buf, int n) {
  // cprintf("in file write\n");
  struct proc* process = myproc();
  if (process == NULL) {
    return -1;
  }

  struct file_info* fi = process->fds[fd];
  // read is only possible if in WO or WRO mode
  if (fi->mode != O_WRONLY && fi->mode != O_RDWR) {
    return -1;
  }

  // trigger pipe_write
  if (fi->p != NULL) {
    return pipe_write(fd, buf, n, process);
  }

  acquiresleep(&fi->lock);
  int bytes_written = concurrent_writei(fi->in, buf, fi->offset, n);
  fi->offset += bytes_written;  // add the offset
  releasesleep(&fi->lock);
  return bytes_written;
}

// stat: chase the pointers, then call concurrentstati on the inode
int file_stat(int fd, struct stat* s) {
  // cprintf("in filestat\n");
  
  struct proc* process = myproc();
  if (process == NULL) {
    return -1;
  }

  // Cant call file_stat on pipe
  if (process->fds[fd]->p != NULL) {
    return -1;
  }

  acquiresleep(&process->fds[fd]->lock);
  concurrent_stati(process->fds[fd]->in, s);
  releasesleep(&process->fds[fd]->lock);
  return 0;
}

int pipe_open(int fds[]) {
  // cprintf("in pipe open\n");
  struct proc* process = myproc();
  if (process == NULL) {
    return -1;
  }

  // check that we have two available fds
  int fd_read = -1;
  int fd_write = -1;
  int idx = 0;
  while (idx < NOFILE) {
    if (process->fds[idx] == NULL) {
      fd_write = fd_read;
      fd_read = idx;
      if (fd_read != -1 && fd_write != -1) break;
    }
    idx++;
  }
  if (fd_read == -1 || fd_write == -1) {
    return -1;
  }

  struct file_info read_file;
  read_file.mode = O_RDONLY;
  read_file.offset = 0;
  read_file.ref_count = 1;  

  struct file_info write_file;
  write_file.mode = O_WRONLY;
  write_file.offset = 0;
  write_file.ref_count = 1; 

  // check for two available file_info structs
  int r_idx = -1;
  int w_idx = -1;
  for (int i = 0; i < NFILE; i++) {
    acquiresleep(&global_files[i].lock);
    // locked -> 1, and the locks pid = process pid
    if (global_files[i].ref_count == 0) {  // this way we don't have to do any cleanup
      if (r_idx == -1) {
        read_file.lock = global_files[i].lock;
        global_files[i] = read_file;
        r_idx = i;
      } else if (w_idx == -1) {
        write_file.lock = global_files[i].lock;
        global_files[i] = write_file;
        w_idx = i;
      }

      if (w_idx != -1 && r_idx != -1) {
        releasesleep(&global_files[i].lock);
        break;
      }
    }
    releasesleep(&global_files[i].lock);
  }

  if (r_idx == -1 && w_idx == -1) {
    return -1;
  } else if (w_idx == -1) {
    acquiresleep(&global_files[r_idx].lock);
    global_files[r_idx].ref_count = 0;
    releasesleep(&global_files[w_idx].lock);
    return -1;
  }

  // inv: we have two location in the gloabl file array



  struct pipe* p = (struct pipe *) kalloc();
  if (p == NULL) {
    acquiresleep(&global_files[r_idx].lock);
    global_files[r_idx].ref_count = 0;
    releasesleep(&global_files[r_idx].lock);

    acquiresleep(&global_files[w_idx].lock);
    global_files[w_idx].ref_count = 0;
    releasesleep(&global_files[w_idx].lock);

    return -1;
  }

  p->buffer = (char*)((char *)p + sizeof(struct pipe));
  p->available_bytes = 0;
  p->read_open = true;
  p->write_open = true;
  p->read_offset = 0;
  p->write_offset = 0;
  p->active_writer = (void*)p;
  p->active_readers = (void*)(p + 1);

  initlock(&p->pipe_lock, "pipe lock");


  // set the pipe field in the new structs
  acquiresleep(&global_files[r_idx].lock);
  global_files[r_idx].p = p;
  releasesleep(&global_files[r_idx].lock);

  acquiresleep(&global_files[w_idx].lock);
  global_files[w_idx].p = p;
  releasesleep(&global_files[w_idx].lock);


  process->fds[fd_read] = &global_files[r_idx];
  process->fds[fd_write] = &global_files[w_idx];

  fds[0] = fd_read;
  fds[1] = fd_write;

  return 0;
}

// Assumes we have the lock for the file_info struct
// Make sure to set the pipe to null
int pipe_close(struct proc * process, int fd) {
  // cprintf("in pipe close\n");

  acquire(&process->fds[fd]->p->pipe_lock); // acquire the pipes spinlock
  process->fds[fd]->ref_count--;
  if (process->fds[fd]->ref_count == 0) {
    if (process->fds[fd]->mode == O_RDONLY){
      // send a wakeup to active writer
      process->fds[fd]->p->read_open = false;
      wakeup(process->fds[fd]->p->active_writer);
    } else {
      process->fds[fd]->p->write_open = false;
      wakeup(process->fds[fd]->p->active_readers);
    }
    release(&process->fds[fd]->p->pipe_lock); 
    process->fds[fd]->p = NULL; // remove the ref to pipe
    releasesleep(&process->fds[fd]->lock); 
    process->fds[fd] = NULL; // remove from fd table
    return 0;
  }
  release(&process->fds[fd]->p->pipe_lock); 
  releasesleep(&process->fds[fd]->lock);  
  process->fds[fd] = NULL; // remove from fd table
  return 0;
}

int pipe_read(int fd, char* buf, int n, struct proc* process) {
  // cprintf("in pipe read %s\n", process->name);

  struct file_info* fi = process->fds[fd];
  if (fi->mode != O_RDONLY) {
    return -1;
  }

  acquiresleep(&fi->lock); // acquire the lock for read file info struct
  struct pipe* p = fi->p;
  int total_size = 4096 - sizeof(struct pipe);

  acquire(&p->pipe_lock); // acquire the pipe's lock

  while (p->available_bytes == 0 && p->write_open) {
    wakeup(p->active_writer); // wake up any writer that might be waiting
    releasesleep(&fi->lock);
    sleep(p->active_readers, &p->pipe_lock);
    acquiresleep(&fi->lock);
  }

  // if there isnt any data and the write end is closed, return 0
  if (p->available_bytes == 0) {
    release(&p->pipe_lock);
    releasesleep(&fi->lock);
    return 0;
  }

  // else, read as much data as can up to n bytes
  int idx = 0;
  while (idx < n && p->available_bytes > 0) {
    buf[idx] = p->buffer[p->read_offset];
    p->read_offset = (p->read_offset + 1) % total_size;
    idx++;
    p->available_bytes--;
  }

  release(&p->pipe_lock);
  releasesleep(&fi->lock);
  return idx; // number of bytes read
}

int pipe_write(int fd, char* buf, int n, struct proc* process) {
  // cprintf("in pipe read %s\n", process->name);

  struct file_info* fi = process->fds[fd];

  if (fi->mode != O_WRONLY) {
    return -1;
  }

  acquiresleep(&fi->lock); // acquire the lock for read file info struct
  struct pipe* p = fi->p;
  int total_size = 4096 - sizeof(struct pipe);

  acquire(&p->pipe_lock); // acquire the pipe's lock

  int idx = 0;
  while(idx < n) { // outer loop to handle wakeup after sleeping
    if (!p->read_open) { // no read ends are open
      // if we haven't written any bytes, return error (-1)
      if (idx == 0) {
        release(&p->pipe_lock);
        releasesleep(&fi->lock);
        return -1;
      } else {
        // if some bytes were written, break out and return the count
        break;
      }
    }
    
    while(idx < n && p->available_bytes < total_size) {
      p->buffer[p->write_offset] = buf[idx]; // assumes read offset has not been read at start
      p->write_offset = (p->write_offset + 1) % total_size;
      if (p->write_offset < 0) {
        cprintf("write offset was negative\n");
      }
      idx++;
      p->available_bytes++;
    }
    if (idx < n) { // available bytes == 0 
      wakeup(p->active_readers); // wakeup the active writer
      sleep(p->active_writer, &p->pipe_lock);
    }
  }
  release(&p->pipe_lock);
  wakeup(p->active_readers);
  // release(&p->pipe_lock);
  releasesleep(&fi->lock);
  // cprintf("done with pipe_write\n");
  return idx;

}