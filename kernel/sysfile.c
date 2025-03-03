//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <cdefs.h>
#include <defs.h>
#include <fcntl.h>
#include <file.h>
#include <fs.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <stat.h>


int sys_dup(void) {
  int fd;
    
  if (argfd(0, &fd) == - 1){
    return -1;
  }

  return file_dup(fd);
}

int sys_read(void) {
  int fd;
  char * buf;
  int n;
  
  if (argfd(0, &fd) == - 1){
    return -1;
  }

  if (argstr(1, &buf) == -1) {
    return -1;
  }

  if (argint(2,&n) == - 1){
    return -1;
  }

  return file_read(fd, buf, n);
}

int sys_write(void) {
  // you have to change the code in this function.
  // Currently it supports printing one character to the screen.

  int fd;
  char * buf;
  int n;
  
  if (argfd(0, &fd) == - 1){
    return -1;
  }

  if (argstr(1, &buf) == -1) {
    return -1;
  }

  if (argint(2,&n) == - 1){
    return -1;
  }

  return file_write(fd, buf, n);
}

int sys_close(void) {
  int fd;

  if (argfd(0, &fd) == - 1){
    return -1;
  }

  return file_close(fd); 
}

int sys_fstat(void) {
  int fd;
  struct stat * statptr;

  if (argfd(0, &fd) == - 1){
    return -1;
  }

  if (argptr(1, (char **)&statptr, sizeof(struct stat)) == -1) { // Unfinished, how to use argptr?
    return -1;
  }

  return file_stat(fd, statptr);
}
 

int sys_open(void) {
  char * path;
  int mode;

  if (argstr(0, &path) == -1) {
    return -1;
  }

  if (argint(1, &mode) == -1) {
    return -1;
  }

  return file_open(path, mode);
}

int sys_exec(void) {
  // LAB2
  char * path;
  char ** argv;

  if (argstr(0, &path) == -1) {
    return -1;
  }
  
   if (argptr(1, (char **)&argv, sizeof(char*)) == -1) {
    return -1;
  }
  int arg_counter = 0;
  while(1) {
    // when up here 0xf00df00d is changed in argptr
    if (argv[arg_counter] == NULL) {
      break;
    }
    int64_t addr;
    if (fetchint64_t( (uint64_t) (argv + arg_counter), &addr) == -1) { // failing here
      return -1;
    }
    
    // validate string pointer
    if (fetchstr((uint64_t)argv[arg_counter], &argv[arg_counter]) == -1) {
      return -1;
    }
    
    arg_counter++;
  }

  return exec(path, argv, arg_counter);
}

int sys_pipe(void) {
  // not called
  // LAB2
  // check that the address space of the array is valid using argptr
  // 
  int* args; 

  if (argptr(0, (void*)&args, sizeof(int)*2) == -1) {
    return -1;
  }
  // use argfd to check the file descriptor values

  // if (pipevalidfd(args[0]) == -1) {
  //   return -1;
  // }

  // if (pipevalidfd(args[1]) == -1) {
  //   return -1;
  // }

  // revisit

  return pipe_open(args);
}

int sys_unlink(void) {
  // LAB 4
  return -1;
}
