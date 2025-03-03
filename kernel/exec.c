#include <cdefs.h>
#include <defs.h>
#include <elf.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <trap.h>
#include <x86_64.h>


int exec(char *path, char **argv, int arg_counter) {
  // your code here
  struct vspace new_vspace;
  uint64_t entry_point;
  uint64_t sp = SZ_2G;
  uint64_t argv_addrs[MAXARG];

  struct proc * curr_proc = myproc();
  struct vspace old_vspace = curr_proc->vspace;
  

  if (vspaceinit(&new_vspace) == -1) {
    return -1;
  }

  if (vspaceloadcode(&new_vspace, path, &entry_point) == 0) {
    vspacefree(&new_vspace);
    return -1;
  }

  if (vspaceinitstack(&new_vspace, sp) == -1) {
    vspacefree(&new_vspace);
    return -1;
  }
  vspaceupdate(&new_vspace);

  // copy strings from the new stack bottom up
  for (int i = 0; i < arg_counter; i++) {
    int len = strlen(argv[i]) + 1;
    sp -= len;
    if (vspacewritetova(&new_vspace, sp, (char*)argv[i], len) == -1) {
      vspacefree(&new_vspace);
      return -1;
    }
    argv_addrs[i] = sp;
  }

  sp = sp - (sp % 8);  // 8 is size of char poinyter
  sp -= 8 * (arg_counter + 1);  // Space for pointers and the NULL

  // write argv array
  for (int i = 0; i < arg_counter; i++) {
    if (vspacewritetova(&new_vspace, sp + i * 8, (char*) &argv_addrs[i], 8) == -1) {
      vspacefree(&new_vspace);
      return -1;
    }
  }

  // wite NULL
  uint64_t null_ptr = 0;
  if (vspacewritetova(&new_vspace, sp + arg_counter * 8, (char*)&null_ptr, 8) == -1) {
    vspacefree(&new_vspace);
    return -1;
  }

  curr_proc->tf->rip = entry_point;
  curr_proc->tf->rsp = sp - 8;
  curr_proc->tf->rdi = arg_counter;
  curr_proc->tf->rsi = sp;
  
  curr_proc->vspace = new_vspace;

  vspaceinstall(curr_proc);
  vspacefree(&old_vspace);


  return 0;
}
