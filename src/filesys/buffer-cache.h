#include "../threads/synch.h"
#include <stdint.h>
#include "filesys/inode.h"

#define NUM_ENTRIES 64
#define NUM_SECTOR_BYTES 512

struct cache_entry;
struct cache_buffer;

void cache_init();
char* cache_fetch_block(block_sector_t);
char* get_block(block_sector_t);
void read_cache(block_sector_t sector, void* buffer);
void write_cache(block_sector_t sector, void* buffer);
void cache_flush();