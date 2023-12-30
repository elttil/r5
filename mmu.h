#include "types.h"
#include <stdbool.h>

struct Memory {
  u8 *ram;
  u64 size;
};

bool ram_init(struct Memory *mem, u64 size);
void memory_write(struct Memory *mem, u64 destination, void *buffer,
                  u64 length);
void memory_read(struct Memory *mem, u64 source, void *buffer, u64 length);
