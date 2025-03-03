#pragma once

// vspace.h contains declarations and class definitions relevant for managing process'
// virtual address spaces.
//
// In `xk` we track most process memory mappings twice. Once in hardware-agnostic data
// structures (`struct vspace`, `struct vregion`, etc.), and once in the hardware-specific x86-64 page
// tables (`pml4e_t`, etc.). Users of `vspace.h` are expected to manipulate the hardware-agnostic data
// structures, and then use `vspaceupdate` to mirror those changes onto the hardware-specific
// page tables.
//
// `vspace.h` introduces these hardware-agnostic datastructures to present a simpler interface for
// virtual address space management. However, at the end of the day we still need to modify the
// hardware-specific tables as those are what are actually used by the CPU to do address
// translation.

#include <defs.h>
#include <mmu.h>

// Number of vregions in a vspace.
#define NREGIONS 3

enum {
  VR_CODE   = 0,
  VR_HEAP   = 1,
  VR_USTACK = 2,
};

// Macros for vpage_info flag values.
#define VPI_PRESENT  ((short) 1)
#define VPI_WRITABLE ((short) 1)
#define VPI_READONLY ((short) 0)

// A vpage_info tracks metadata for a virtual page of memory.
// Perhaps most importantly, this includes the physical page that the page
// actually maps to, but also permissions like whether the page should be writable.
struct vpage_info {
  short used;     // 1 if the page is in use. If 0, other fields should be ignored.
  uint64_t ppn;   // The physical page number this virtual page maps to.
  short present;  // whether the page is in physical memory
  short writable; // does the page have write permissions
  // user defined fields below

  short cowpageflag;

};

// "vpage_info per page". The number of vpage_info structs we will shove into
// a page of memory.
#define VPIPPAGE ((PGSIZE/sizeof(struct vpage_info)) - 1)

// Every `vregion` owns a contiguous virtual address range [bot, top), where bot
// <= top. These macros give those bound values, accounting for the fact that
// `r->va_base` points to `bot` for VRDIR_UP vregions and `top` for VRDIR_DOWN
// regions.
#define VRTOP(r) \
  ((r)->dir == VRDIR_UP ? (r)->va_base + (r)->size : (r)->va_base)
#define VRBOT(r) \
  ((r)->dir == VRDIR_UP ? (r)->va_base : (r)->va_base - (r)->size)

// A vpi_page is a convenience container for vpage_infos (it's a "page of vpage_infos").
// `vpi_page` is sized so that we can allocate a page of physical memory and use
// it for containing as many `vpage_infos` as will fit in a page. Should a `vregion`
// require more than one page of `vpage_info`'s, `vpi_page`'s can point to other
// `vpi_page`'s to form a dynamically-growable linked list.
struct vpi_page {
  struct vpage_info infos[VPIPPAGE];  // info struct for the given page
  struct vpi_page *next;              // the next page
};

enum vr_direction {
  VRDIR_UP,   // The code and heap "grow up"
  VRDIR_DOWN  // The stack "grows down"
};

// A vregion tracks the metadata for a logical contiguous region of virtual
// memory. For example, every user program will have a code, stack, and heap
// region. Each of those regions will be tracked by a `struct vregion`.
struct vregion {
  enum vr_direction dir;   // Direction that the vregion grows. e.g.: stack grows downwards.
  uint64_t va_base;        // Base of the region. Should be page-aligned.
  uint64_t size;           // Number of bytes in the vregion.

  // Pointer to linked list of `vpi_page`'s which contain the `vpage_info` metadata
  // for the vregion. Use `va2vpage_info` to get the corresponding `vpage_info` for a
  // virtual address in a vregion.
  //
  // Using 0-indexing: The `vpage_info` for the n'th virtual page in the vregion is
  // in `vpi_page[n % VPIPPAGE]` where vpi_page is the (n / VPIPPAGE)'th `vpi_page`
  // in the `pages` linked list.
  struct vpi_page *pages;  
};

// A vspace tracks the metadata for a process' virtual address space. Every process
// needs a `vspace` in order to operate.
struct vspace {
  // The regions for the process' virtual address space.
  struct vregion regions[NREGIONS];
  // The process' machine-specific page table. The "real" page table that the hardware
  // uses for address translation.
  pml4e_t* pgtbl;                   
};
