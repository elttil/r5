// This files relates to how the emulated software writes to the memory. Most
// often stuff will be written to RAM but sometimes attached devices may occupy
// certain memory regions which are handeled differently from RAM.
//
// Paging will also be handeled in this file when/if that gets implemented
#include "mmu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define U64_OVERFLOW_CHECK(_a, _b, _exp)                                       \
  {                                                                            \
    if ((_a) > UINT64_MAX - (_b)) {                                            \
      _exp;                                                                    \
    }                                                                          \
  }

// UART
#define Ns16650a_BASE 0x10000000

bool ram_init(struct Memory *mem, u64 size) {
  mem->ram = malloc(size);
  if (!mem->ram) {
    perror("malloc");
    return false;
  }
  mem->size = size;
  return true;
}

// Bounds checked memory write for instructions to use.
void memory_write(struct Memory *mem, u64 destination, void *buffer,
                  u64 length) {
  U64_OVERFLOW_CHECK(destination, length, goto write_fail);

  // TODO: Make this more general and not hardcoded
  if (Ns16650a_BASE == destination) {
    write(STDOUT_FILENO, buffer, 1);
    return;
  }

  U64_OVERFLOW_CHECK(destination, (u64)mem->ram, goto write_fail);
  U64_OVERFLOW_CHECK(length, (u64)mem->ram, goto write_fail);

  if (destination + length >= mem->size) {
    goto write_fail;
  }
  memcpy(mem->ram + destination, buffer, length);
  return;
write_fail:
#ifdef DEBUG
  assert(0);
#else
  assert(0);
  return;
#endif
}

// Bounds checked memory read for instructions to use.
void memory_read(struct Memory *mem, u64 source, void *buffer, u64 length) {
  U64_OVERFLOW_CHECK(source, (u64)mem->ram, goto read_fail);
  U64_OVERFLOW_CHECK(length, (u64)mem->ram, goto read_fail);
  U64_OVERFLOW_CHECK(source, length, goto read_fail);

  if (source + length >= mem->size) {
    goto read_fail;
  }
  memcpy(buffer, mem->ram + source, length);
  return;
read_fail:
  memset(buffer, 0, length);
#ifdef DEBUG
  assert(0);
#else
  assert(0);
  return;
#endif
}
