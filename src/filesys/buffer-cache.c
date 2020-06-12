#include <stdbool.h>
#include "threads/synch.h"

#define NUM_ENTRIES 64
#define NUM_SECTOR_BYTES 512


struct cache_entry{
	//Meta data
	block_sector_t sector;
	bool dirty;
	bool use;
	bool valid;

	char data[NUM_SECTOR_BYTES];
};

struct cache_buffer{
	struct lock entry_locks[NUM_ENTRIES];
	struct cache_entry cache_entries[NUM_ENTRIES];
	
	//Data used for clock algorithm
	int clock_hand;
};

struct cache_buffer cache_buffer;
struct lock cache_lock;

void advance_clock_hand(int *clock_hand) {
	*clock_hand = (*clock_hand + 1) % NUM_ENTRIES;
}

void cache_entry_init(struct cache_entry *cache_entry) {
	cache_entry->valid = false;
	cache_entry->use = false;
	cache_entry->dirty = false;
}

void cache_init() {
	lock_init(&cache_lock);
	int i;
	
	for (i = 0; i < NUM_ENTRIES; i++) {
		lock_init(&cache_buffer.entry_locks[i]);
		cache_entry_init(&cache_buffer.cache_entries[i]);

		cache_buffer.clock_hand = 0;	
	}
}

struct cache_entry *cache_fetch_block(block_sector_t sector) {
	struct cache_entry *cache_entries = cache_buffer.cache_entries;
	struct cache_entry *temp;
	int clock_hand;
	while (true) {
		clock_hand = cache_buffer.clock_hand;
		if (!cache_entries[clock_hand].valid || !cache_entries[clock_hand].use) {
			if (!cache_entries[clock_hand].use && cache_entries[clock_hand].valid) {
				if (cache_entries[clock_hand].dirty) {
					block_write(fs_device, cache_entries[clock_hand].sector, cache_entries[clock_hand].data);
				}
			}
			block_read(fs_device, sector, cache_entries[clock_hand].data);
			cache_entries[clock_hand].valid = true;
			cache_entries[clock_hand].use = true;
			cache_entries[clock_hand].sector = sector;

			temp = &cache_entries[clock_hand];
			advance_clock_hand(&cache_buffer.clock_hand);
			return temp;
		}

		cache_entries[clock_hand].use = false;
		advance_clock_hand(&cache_buffer.clock_hand);
	}
}

struct cache_entry *get_cache_entry(block_sector_t sector) {
	struct cache_entry *cache_entry;
	int i;
	for (i = 0; i < NUM_ENTRIES; i++) {
		cache_entry = &cache_buffer.cache_entries[i];

		if(cache_entry->valid) {
			if (cache_entry->sector == sector) {
				return cache_entry;
			}
		} 
	}
	return cache_fetch_block(sector);
}

void read_cache(block_sector_t sector, void* buffer) {
    struct cache_entry *cache_entry;

    cache_entry = get_cache_entry(sector);
    memcpy(buffer, cache_entry->data, 512);
}

void write_cache(block_sector_t sector, void* buffer) {
	struct cache_entry *cache_entry;

	cache_entry = get_cache_entry(sector);
	memcpy(cache_entry->data, buffer, 512);
	cache_entry->dirty = true;
}

void *get_block(block_sector_t sector) {
    struct cache_entry *cache_entry;

    cache_entry = get_cache_entry(sector);
    return cache_entry->data;
}

void cache_flush() {
    struct cache_entry *cache_entry;
    int i;
    for (i = 0; i < NUM_ENTRIES; i++) {
        cache_entry = &cache_buffer.cache_entries[i];

        if (cache_entry->dirty) {
            block_write(fs_device, cache_entry->sector, cache_entry->data);
        }
    }
}
