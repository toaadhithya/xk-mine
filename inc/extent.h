#pragma once

// An extent describes a contiguous sequence of blocks on disk,
// ranging from [startblkno, startblkno + nblocks).
struct extent {
  uint startblkno; // First block in extent.
  uint nblocks;    // Total number of blocks in extent.
};
