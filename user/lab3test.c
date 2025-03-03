#include <cdefs.h>
#include <fcntl.h>
#include <stat.h>
#include <stdarg.h>
#include <sysinfo.h>
#include <user.h>
#include <test.h>

#define STACKBASE SZ_2G

void run_test(char*);
void bad_mem_access(void);
void malloc_test(void);
void sbrk_small(void);
void sbrk_large(void);
void stack_growth_basic(void);
void stack_growth_bad_access(void);
void cow_fork(void);

int main(int argc, char *argv[]) {
  char buf[40];
  // if a command line argument was passed, it will be run directly rather than
  // starting the minishell (exactly the same as if it was entered in the minishell). 
  // Only the first argument will be run. After completing, will terminate.
  if (argc > 1) {
    run_test(argv[1]);
  } else {
    while (true) {
      shell_prompt("lab3");
      memset(buf, 0, sizeof(buf));
      gets(buf, sizeof(buf));
      if (buf[0] == 0) {
        continue;
      }
      run_test(buf);
    }
  }

  exit();
  return 0;
}

void run_test(char *test) {
  // Terminate the string at the first newline (strips newline at end)
  for (char *c = test; *c != 0; c++) {
    if (*c == '\n') {
      *c = '\0';
      break;
    }
  }
  if (strcmp(test, "all") == 0) {
    bad_mem_access();  // sbrk(0) required
    malloc_test();
    sbrk_small();
    sbrk_large();
    stack_growth_basic();
    stack_growth_bad_access();
    cow_fork();
    pass("lab3 tests");
  } else if (strcmp(test, "exit") == 0) {
    exit();
  } else if (strcmp(test, "bad_mem_access") == 0) {
    bad_mem_access();
  } else if (strcmp(test, "malloc_test") == 0) {
    malloc_test();
  } else if (strcmp(test, "sbrk_small") == 0) {
    sbrk_small();
  } else if (strcmp(test, "sbrk_large") == 0) {
    sbrk_large();
  } else if (strcmp(test, "stack_growth_basic") == 0) {
    stack_growth_basic();
  } else if (strcmp(test, "stack_growth_bad_access") == 0) {
    stack_growth_bad_access();
  } else if (strcmp(test, "cow_fork") == 0) {
    cow_fork();
  } else {
    printf(stderr, "input matches no test: %s", test);
  }
}

// try to access memory out of allowed ranges
void bad_mem_access(void) {
  test("bad_mem_access");

  int pid;
  char *a;
  
  // try to access kernel memory
  printf(stdout, "\npids 2-42 (4-44 if ran after sh) should be killed with trap 14 err 5\n");

  for (a = (char *)(KERNBASE); a < (char *)(KERNBASE + 2000000); a += 50000) {
    pid = fork();
    if (pid < 0) {
      error("bad_mem_access: fork failed");
    }
    if (pid == 0) {
      printf(stdout, "bad_mem_access: oops could read kernel addr %lp = %lx\n", a, *a);
      error("bad_mem_access: a process trying to access kernel memory is not killed");
    }
    assert(wait() == pid);
  }

  // try to access unallocated heap region
  pid = fork();
  if (pid == 0) {
    a = sbrk(0) + PGSIZE * 10;
    printf(stdout, "bad_mem_access: oops could read unallocated heap addr %x = %x\n", a, *a);
    error("bad_mem_access: a process trying to access unallocated heap memory is not killed");
  }
  assert(wait() == pid);

  pass("");
}

void malloc_test(void) {
  test("malloc_test");

  void *prev, *curr;
  int pid;
  int i;

  if ((pid = fork()) == 0) { // child case
    prev = NULL;

    // malloc 10 times, create a linked list of malloced address
    // by storing previsouly malloced address into newly malloced region
    for (i = 0; i < 10; i++) {
      curr = malloc(10001);
      if (curr == NULL) {
        error("malloc_test: failed to malloc 10001 bytes, iteration %d", i);
      }

      *(char **)curr = prev;
      prev = curr;
    }

    // free each malloced address
    while (curr) {
      prev = *(char **)curr;
      free(curr);
      curr = prev;
    }
  
    curr = malloc(1024 * 20);
    if (curr == NULL) {
      error("malloc_test: failed to malloc more after malloc and free");
    }
    free(curr);
    exit();
    error("malloc_test: returned from exit");
  }
  
  assert(pid > 0);
  assert(wait() == pid);
  pass("");
}

void sbrk_small(void) {
  test("sbrk_small");

  int i, pid;
  char *a, *b, *c;

  a = sbrk(0);
  if (a < 0) {
    error("sbrk_small: sbrk errored, returned %d", a);
  }

  for (i = 0; i < 5000; i++) {
    b = sbrk(1);
    if (b != a) {
      error("sbrk_small: sbrk(1) failed at iteration %d, expected old heap %x, got %x", i, a, b);
    }
    *b = 1;
    a = b + 1;
  }

  pid = fork();
  if (pid < 0) {
    error("sbrk_small: fork failed");
  }

  if (pid == 0) {
    c = sbrk(1);
    c = sbrk(1);
    if (c != a + 1) {
      error("sbrk_small: sbrk returned wrong old heap value post fork, expecting %x, got %x", a+1, c);
    }
    exit();
    error("sbrk_small: exit returned");
  }

  assert(wait() == pid);
  pass("");
}

void sbrk_large(void) {
  test("sbrk_large");

  int pid;
  char *a, *lastaddr, *oldbrk;
  uint64_t amt;
  struct sys_info info;

  oldbrk = sbrk(0);
  const uint64_t big = PGSIZE * 256;
  // can one grow address space to something big?
  amt = (big) - (uint64_t)oldbrk;
  if (sbrk(amt) != oldbrk) {
    error("sbrk_large: failed to grow the heap by %x bytes", amt);
  }

  // make sure we can still access it
  lastaddr = (char *)(big - 1);
  *lastaddr = 99;

  // if we run the system out of memory, does it clean up the last
  // failed allocation?
  pid = fork();
  if (pid < 0) {
    error("sbrk_large: fork failed");
  }
  if (pid == 0) {
    // child grows its heap 256 pages at a time
    do {
      // assert(sysinfo(&info) == 0);
      // free_pages = info.free_pages;
      // printf(stdout, "free pages before sbrk: %d\n", free_pages);
      a = sbrk(big);
    } while (a != (char *) -1);

    assert(sysinfo(&info) == 0);
    if (info.free_pages == 0) {
      error("sbrk_large: free pages after out-of-memory(shouldn't be 0): %d", info.free_pages);
    }

    // if those failed allocations freed up the pages they did allocate,
    // we'll be able to allocate here
    a = sbrk(PGSIZE);
    if (info.free_pages != 0 && a == (char *) -1) {
      error("sbrk_large: failed sbrk leaked memory. Uncomment do while loop sysinfo for more information.");
    }
    exit();
  }
  assert(wait() == pid);
  pass("");
}

void stack_growth_basic() {
  test("stack_growth_basic");

  int pid1 = fork();
  if (pid1 < 0) {
    error("stack_growth_basic: fork failed");
  }

  // we fork once so the parent's stack doesn't get modified (allows test to be run multiple times)
  if (pid1 == 0) {
    int pid = fork();
    if (pid < 0) {
      error("stack_growth_basic: fork failed");
    }

    if (pid == 0) {
      // in grandchild, check stack grows on write
      int i;
      struct sys_info info1, info2;
      assert(sysinfo(&info1) == 0);
      printf(stdout, "\npages_in_use before stack allocation = %d", info1.pages_in_use);

      const int page8 = 8 * PGSIZE;
      char buf[page8];
      for (i = 0; i < 8; i++) {
        buf[i * PGSIZE] = 'a';
        buf[(i + 1) * PGSIZE - 1] = buf[i * PGSIZE];
        printf(stdout, "successfully added another page\n");
      }

      assert(sysinfo(&info2) == 0);
      printf(stdout, "\npages_in_use after stack allocation = %d\n", info2.pages_in_use);

      // if grow ustack on-demand is implemented, then the 8 pages are allocated at run-time
      if (info2.pages_in_use - info1.pages_in_use != 8) {
        error("stack_growth_basic: failed to grow user stack by 8 pages"); 
      }
      exit();
    } else {
      // in child, check stack grows on read
      assert(wait() == pid);
      int i;
      struct sys_info info1, info2;
      assert(sysinfo(&info1) == 0);
      printf(stdout, "pages_in_use before stack allocation = %d\n",
            info1.pages_in_use);

      const int page8 = 8 * PGSIZE;
      char buf[page8];
      char a;
      (void) a; // Suppress "set but not used".
      for (i = 0; i < 8; i++) {
        a = buf[i * PGSIZE];
        a = buf[(i + 1) * PGSIZE - 1];
        // printf(stdout, "successfully added another page\n");
      }

      assert(sysinfo(&info2) == 0);
      printf(stdout, "pages_in_use after stack allocation = %d\n",
            info2.pages_in_use);

      // if grow ustack on-demand is implemented, then the 8 pages are allocated
      // at run-time
      if (info2.pages_in_use - info1.pages_in_use != 8) {
        error("stack_growth_basic: failed to grow user stack by 8 pages");
      }
    }

    pass("");
    exit();
  }
  else {
    // wait for stack growth tests to complete
    assert(wait() != -1);
  }
}

void stack_growth_bad_access() {
  test("stack_growth_bad_access");
  printf(stdout, "\nnext 2 processes should be killed with trap 14 err 6\n");

  int pid = fork();
  if (pid < 0) {
    error("stack_growth_bad_access: fork failed");
  }

  if (pid == 0) {
    // try to allocate 11 pages of stack, exceeding the limit
    char *buf = (char *) STACKBASE - 11 * PGSIZE;
    *buf = 0;
    error("stack_growth_bad_access: able to grow past 10 pages of stack");
  }
  assert(wait() == pid);

  pid = fork();
  if (pid < 0) {
    error("stack_growth_bad_access: fork failed");
  }

  if (pid == 0) {
    char *buf = (char *) STACKBASE;
    // can the stack grow upwards?
    *buf = 0;
    error("stack_growth_bad_access: able to grow stack upwards");
  }
  assert(wait() == pid);

  pass("");
}

void cow_fork() {
  test("cow_fork");
  printf(stdout, "\n");

  struct sys_info info1, info2, info3, info4, info5;
  char *a;
  volatile char b;
  int i, j;

  a = sbrk(200 * PGSIZE);
  if (a == (char*) -1) {
    error("cow_fork: failed to grow the heap");
  }

  for (i = 0; i < 200; i++) {
    a[i * PGSIZE] = i;
  }
  
  int small_file = open("/small.txt", O_RDONLY);

  assert(sysinfo(&info1) == 0);
  printf(stdout, "cow_fork: pages_in_use before copy-on-write fork = %d\n",
         info1.pages_in_use);

  int pid = fork();
  if (pid < 0) {
    error("cow_fork: fork failed");
  }  

  if (pid == 0) {
    sysinfo(&info2);
    printf(stdout, "cow_fork: pages_in_use after copy-on-write fork = %d\n",
           info2.pages_in_use);

    // if copy-on-write is implemented, there is no way a new process can take more than 100 pages
    if (info2.pages_in_use - info1.pages_in_use > 100) {
      error("cow_fork: too much memory is used for fork");
    }

    // read from heap, should not increase the # of pages allocated
    for (j = 0; j < 200; j++) {
      b = a[j * PGSIZE];
    }

    sysinfo(&info3);
    printf(stdout, "cow_fork: pages_in_use after read = %d\n",
           info3.pages_in_use);

    // Read should not increase the amount of memory allocated
    if (info3.pages_in_use != info2.pages_in_use) {
      error("cow_fork: too much memory is used for read");
    }

    // This validates that the copy is performed even if the write is performed in kernel mode
    uintptr_t buf_addr = (uintptr_t) (a + 5 * PGSIZE);
    // Make sure that [buf_addr, buf_addr + 100) is page-aligned and thus
    // should cause exactly one page of COW growth.
    buf_addr -= buf_addr % PGSIZE;
    if (read(small_file, (char *) buf_addr, 100) < 1) {
      error("cow_fork: unable to write to cow-forked heap memory");
    }

    close(small_file);

    sysinfo(&info4);
    printf(stdout, "cow_fork: pages_in_use after file read = %d\n",
           info4.pages_in_use);
    if (info4.pages_in_use - info3.pages_in_use != 1) {
      error("cow_fork: kernel writes should cause copy");
    }

    // write to heap, should cause copy on write
    for (j = 0; j < 200; j++) {
      a[j * PGSIZE] = b;
    }

    sysinfo(&info5);
    printf(stdout, "cow_fork: pages_in_use after write = %d\n",
           info5.pages_in_use);

    // Write should allocate the 200 pages of memory
    if (info5.pages_in_use - info3.pages_in_use < 200) {
      error("cow_fork: too little memory is used for copy on write access");
    }

    exit();
    error("cow_fork: child failed to exit");
  }

  assert(wait() == pid);

  assert(sysinfo(&info2) == 0);
  if (info1.pages_in_use != info2.pages_in_use) {
    error("cow_fork: failed to deallocate all memory used by cow forked child");
  }

  pass("");
}
