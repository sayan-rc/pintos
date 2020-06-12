#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"

static struct file *free_map_file;   /* Free map file. */
static struct bitmap *free_map;      /* Free map, one bit per sector. */
struct lock mem_lock;
static bool ignore_mem_lock;
bool print2 = 0;

acquire_lock(struct lock *lock) {
  ignore_mem_lock = lock_held_by_current_thread(lock);
  if (!ignore_mem_lock) {
  //  printf("acquire_lock: lock acquired by thread: %p \n", thread_current());
    lock_acquire(lock);
  }
  
}

release_lock(struct lock *lock) {
  
 // if (!ignore_mem_lock) {
  //  printf("release_lock: lock released by thread: %p \n", thread_current());
    lock_release(lock);
 // }
 // ignore_mem_lock = false;
}

/* Initializes the free map. */
void
free_map_init (void)
{
  free_map = bitmap_create (block_size (fs_device));
  if (free_map == NULL)
    PANIC ("bitmap creation failed--file system device is too large");
  bitmap_mark (free_map, FREE_MAP_SECTOR);
  bitmap_mark (free_map, ROOT_DIR_SECTOR);
  lock_init(&mem_lock);
  ignore_mem_lock = false;
}

/* Allocates CNT consecutive sectors from the free map and stores
   the first into *SECTORP.
   Returns true if successful, false if not enough consecutive
   sectors were available or if the free_map file could not be
   written. */
/*

bool
free_map_allocate (size_t cnt, block_sector_t *sectorp)
{
  if (print2 == 1)
    printf("free_map_allocate: enters free_map_allocate \n");
  if(! lock_held_by_current_thread(&mem_lock)) {
    lock_acquire(&mem_lock);
    ignore_lock = true;
  }
  if (print2 == 1)
    printf("free_map_allocate: before bitmap_scan_and_flip \n");
  block_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
  if (sector != BITMAP_ERROR
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
    {
      if (print2 == 1)
        printf("free_map_allocate: before bitmap_set_multiple \n");
      bitmap_set_multiple (free_map, sector, cnt, false);
      sector = BITMAP_ERROR;
    }
  if(! ignore_lock)
    lock_release(&mem_lock);
  if (sector != BITMAP_ERROR)
    *sectorp = sector;
    
  return sector != BITMAP_ERROR;
}
*/

bool
free_map_allocate (size_t cnt, block_sector_t *sectorp)
{
  acquire_lock(&mem_lock);
  block_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);
  if (sector != BITMAP_ERROR
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
    {
      bitmap_set_multiple (free_map, sector, cnt, false);
      sector = BITMAP_ERROR;
    }
  if (sector != BITMAP_ERROR)
    *sectorp = sector;
  release_lock(&mem_lock);
  return sector != BITMAP_ERROR;
}

void
free_map_release (block_sector_t sector, size_t cnt)
{
  ASSERT (bitmap_all (free_map, sector, cnt));
  acquire_lock(&mem_lock);
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
  release_lock(&mem_lock);
}

/* Makes CNT sectors starting at SECTOR available for use. */
/*
void
free_map_release (block_sector_t sector, size_t cnt)
{
  ASSERT (bitmap_all (free_map, sector, cnt));
  if(! lock_held_by_current_thread(&mem_lock)) {
    lock_acquire(&mem_lock);
    ignore_lock = true;
  }
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
  if(! ignore_lock)
    lock_release(&mem_lock);
}
*/

/* Opens the free map file and reads it from disk. */
void
free_map_open (void)
{
  acquire_lock(&mem_lock);
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_read (free_map, free_map_file))
    PANIC ("can't read free map");
  release_lock(&mem_lock);
}

/* Writes the free map to disk and closes the free map file. */
void
free_map_close (void)
{
  acquire_lock(&mem_lock);
  file_close (free_map_file);
  release_lock(&mem_lock);
}

/* Creates a new free map file on disk and writes the free map to
   it. */

void
free_map_create (void)
{
  /* Create inode. */
  if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map), false))
    PANIC ("free map creation failed");
  acquire_lock(&mem_lock);
  /* Write bitmap to file. */
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_write (free_map, free_map_file))
    PANIC ("can't write free map");
//  release_lock(&mem_lock);
}
