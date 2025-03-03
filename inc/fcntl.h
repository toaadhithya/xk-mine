#pragma once

// fcntl.h defines the modes that files can be opened in.

// The following 3 modes are exclusive. A file can only be in
// one of them.

// Read-only
#define O_RDONLY 0x000
// Write-only
#define O_WRONLY 0x001
// Readable and writable
#define O_RDWR 0x002

// O_CREATE is special in that it's bit-flag encoded with the other
// flags. i.e.: a file can be any of O_RDONLY, O_WRONLY, or O_RDWR,
// and also O_CREATE by just bitwise OR'ing O_CREATE with any of the
// other modes.
// Example: to create a O_RDWR file one would pass (O_CREATE | O_RDWR)
// as the mode to sys_open(). 
#define O_CREATE 0x200
