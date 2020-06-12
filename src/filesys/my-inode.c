#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/buffer-cache.c"
#include "threads/synch.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
bool print = 0;
bool print3 = 0;
struct inode_disk
  {
   
   /* Data blocks. */
    block_sector_t direct[118];           /* Direct pointers. */
    block_sector_t indirect;              /* Indirect pointer. */
    block_sector_t doubly_indirect;       /* Doubly indirect pointer. */

    /* Filesys metadata. */
    block_sector_t parent;                /* sector of parent directory */
    block_sector_t start;                /* inode_disk sector of the parent directory. */
    off_t ofs;                            /* Offset of entry in parent directory. */
    bool isdirectory;                           /* True if this file is a directory. */
    uint64_t num_files;                   /* The number of subdirectories or files. */

    /* Misc. */
    off_t length;                         /* File size in bytes. */
    unsigned magic;                       /* Note: magic has a different offset now. */
  //  uint8_t unused[3];

  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock file_lock;              /* file_lock for inode */
 //   struct inode_disk data;             /* inode disk associated with the inode */
  };



/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);

  struct inode_disk *data;
  data = (struct inode_disk *) get_block(inode->sector);

 // printf("byte_to_sector: this is the inode: %d, this is the pos: %d, this is inode->data.length: %d, this is inode->data.direct[0]: %d \n", inode->sector, pos, inode->data.length, inode->data.direct[0]);
 // // // lock_acquire(&inode->file_lock);
  // lock_acquire(&inode->file_lock);
  if (pos < data->length && pos < 512*118){
    // lock_release(&inode->file_lock);
    return data->direct[pos / BLOCK_SECTOR_SIZE];
  }

  /* sector if position is in a block pointed to by indirect pointer */

  if (pos >= 512*118 && pos < (118 + 128)*512) {
    block_sector_t buffer[128];
    memset(buffer, 0, 512);
    read_cache(data->indirect, buffer);
    // lock_release(&inode->file_lock);
    return buffer[(pos - 512*118) / BLOCK_SECTOR_SIZE];
  }

  /* sector if position is in a block pointed to by doubly indirect pointer */

  if (pos >= (118 + 128)*512 && pos < (118 + 128*128)* 512) {
    block_sector_t buffer2[128];
    memset(buffer2, 0, 512);
    read_cache(data->doubly_indirect, buffer2);

    block_sector_t buffer3[128];
    memset(buffer3, 0, 512);
    uint8_t indirect_idx = (pos - 512*118 - 512*128) / (128 * BLOCK_SECTOR_SIZE);
    read_cache(buffer2[indirect_idx], buffer3);
    // lock_release(&inode->file_lock);
    return buffer3[(pos - 512*118 - 512*128 - 512*128*indirect_idx) / BLOCK_SECTOR_SIZE];
  }
  else
    // lock_release(&inode->file_lock);
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  thread_current()->cwd = inode_open(ROOT_DIR_SECTOR);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */


bool
inode_create (block_sector_t sector, off_t length, bool is_directory)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
 // disk_inode->start = disk_inode->direct;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);

  if (disk_inode != NULL)
    {
      if (print3 == 1)
        printf("inode_create: disk_inode is at: %p \n", disk_inode);
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->parent = sector;
      disk_inode->isdirectory = is_directory;
      disk_inode->num_files = 0;
      success = inode_resize(disk_inode, length);
      if (success)
        write_cache(sector, disk_inode);
      free(disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      // lock_acquire(&inode->file_lock);
      if (inode->sector == sector)
        {
          //printf("inode open reopen ");
          inode_reopen (inode);
          // lock_release(&inode->file_lock);
          return inode;
        }
        // lock_release(&inode->file_lock);
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  lock_init(&inode->file_lock);
  // lock_acquire(&inode->file_lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  struct inode_disk *data;
  data = get_block(inode->sector);
  data->parent = sector;
 // if (print == 1) 
  // // lock_acquire(&inode->file_lock);
//  read_cache(inode->sector, &inode->data);
//  printf("inode_open: inode->data.length is: %d \n", inode->data.length);
//  printf("inode_open: inode->sector is %d and data->direct[0] is %d \n", inode->sector, inode->data.direct[0]);
   // lock_release(&inode->file_lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  // // lock_acquire(&inode->file_lock);
  
  if (inode != NULL){
    // lock_acquire(&inode->file_lock);
    inode->open_cnt++;
    // lock_release(&inode->file_lock);
  }
  // // lock_release(&inode->file_lock);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Return's where INODE is directory */
bool
inode_isdir(const struct inode *inode)
{
  struct inode_disk *data;
  data = get_block(inode->sector);
  return data->isdirectory;
}


//TO ASK: DOES BITMAP HAVE TO CHANGE AT ALL

bool 
inode_resize(struct inode_disk *id, off_t size) {
  //printf("inode resize ");
   if (print == 1) 
    printf("inode resize: enters inode resize function \n");
  bool success = 1;
  int i = 0;
  for (i; i < 118; i++) {
    if (size <= 512 * i && id->direct[i] != 0) {
      free_map_release(id->direct[i], 1);
       if (print == 1) 
        printf("inode_resize: freemap release was called \n");
    }
    if (size > 512 * i && id->direct[i] == 0) {
       if (print == 1) 
        printf("inode_resize: before free_map_allocate is called \n");
       if (print == 1) 
        printf("inode_resize: address of parameter to free_map_allocate: %p \n", &id->direct[i]);
      success = free_map_allocate(1, &id->direct[i]);
       if (print3 == 1) 
        printf("inode_resize: freemap allocate was called, i is %d, sector id is: %d \n", i, id->direct[i]);

      if (success == 0) {
        inode_resize(id, id->length);
        if (print == 1)
          printf("inode_resize: return 1 \n");
        return false;
        }
    }
  }
  
  if (id->indirect == 0 && size <= 118 * 512) {
    id->length = size;
    write_cache(id->parent, id);
    if (print == 1)
          printf("inode_resize: return 2, id->length = %d \n", id->length);
    /*
    if (print == 1)
      printf("inode_resize: start value is %d", id->start);
    */
    return true;
  }
  /* if need more than 118*512 B, then use singly indirect ptr */
  
  block_sector_t buffer[128];
  memset(buffer, 0, 512);
  if (id->indirect == 0) {
     if (print == 1) 
      printf("inode_resize: address of parameter to free_map_allocate: %p \n", &id->indirect);
    success = free_map_allocate(1, &id->indirect);
    if (success == 0) {
      inode_resize(id, id->length);
      if (print == 1)
          printf("inode_resize: return 3 \n");
      return false;
    }
  } else {
    read_cache(id->indirect, buffer);
  }
  int j = 0;
  for (j; j < 128; j++) {
    if (size <= (118 + j) * 512 && buffer[j] != 0) {
      free_map_release(buffer[j], 1);
      buffer[j] = 0;
    }
    if ((size > (118 + j) * 512) && buffer[j] == 0){
      success = free_map_allocate(1, &buffer[j]);
      if (success == 0) {
        inode_resize(id, id->length);
        if (print == 1)
          printf("inode_resize: return 4 \n");
        return false;
      }
    }
  }
  write_cache(id->indirect, buffer); //FIX??? write_cache(sector_idx, bounce);

  if (id->doubly_indirect == 0 && size <= 246 * 512) {
    id->length = size;
    if (print == 1)
          printf("inode_resize: return 2, id->length = %d \n", id->length);
    write_cache(id->parent, id);
    return true;
  }

  block_sector_t buffer2[128];
  memset(buffer2, 0, 512);
  if (id->doubly_indirect == 0) {
     if (print == 1) 
      printf("inode_resize: address of parameter to free_map_allocate: %p \n", &id->doubly_indirect);
    success = free_map_allocate(1, &id->doubly_indirect);
    if (success == 0) {
      inode_resize(id, id->length);
      if (print == 1)
          printf("inode_resize: return 3 \n");
      return false;
    }
  } else {
    read_cache(id->doubly_indirect, buffer2);
  } 

  int m = 0;
  int k = 0;

  for (m; m < 128; m++) {
    if(size <= (246 + 128 * m) * 512 && buffer2[m] != 0) {
      free_map_release(buffer2[k], 1);
        buffer2[k] = 0;
    }
    if ((size > (246 + 128 *m) * 512) && buffer2[m] == 0) {
      success = free_map_allocate(1, &buffer2[k]);
      if (success == 0) {
        inode_resize(id, id->length);
        return false;
      }
    }

    block_sector_t buffer3[128];
    memset(buffer3, 0, 512);
    if(buffer2[m] == 0)
      success = free_map_allocate(1, &buffer2[m]);
      if (success == 0) {
        inode_resize(id, id->length);
        return false;
      }
      else {
        read_cache(buffer2[m], buffer3);
      }
      for (k; k < 128; k++) {
        if (size <= (246 + (128 * m + k)) * 512 && buffer3[k] != 0) {
          free_map_release(buffer3[k], 1);
          buffer3[k] = 0;
        }
        if ((size > (118 + k) * 512) && buffer3[k] == 0) {
          success = free_map_allocate(1, &buffer3[k]);
          if (success == 0) {
            inode_resize(id, id->length);
            if (print == 1)
              printf("inode_resize: return 4 \n");
            return false;
          }
        }
      }
      write_cache(buffer2[m], buffer3);
  }
  write_cache(id->doubly_indirect, buffer2); 


  id->length = size;
  if (print == 1)
          printf("inode_resize: return 5 \n");
  write_cache(id->parent, id);
  return true;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */

void
inode_close (struct inode *inode)
{
//  if (print3 == 1)
  //  printf("inode_close: size of file at close is: %d, inode sector is %d, inode->data.direct[0] is %d \n", inode->data.length, inode->sector, inode->data.direct[0]);
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  lock_acquire(&inode->file_lock);
  /* Release resources if this was the last opener. */
  // lock_acquire(&inode->file_lock);
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          struct inode_disk *data;
          data = get_block(inode->sector);
          free_map_release (inode->sector, 1);
          free_map_release (data->direct[0],
                            bytes_to_sectors (data->length));
        }
      lock_release(&inode->file_lock);
    //  printf("inode_close: size of file at close is: %d, inode sector is %d, inode->data.direct[0] is %d \n", inode->data.length, inode->sector, inode->data.direct[0]);
      free (inode);
      return;
    }
    lock_release(&inode->file_lock);
}


/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  lock_acquire(&inode->file_lock);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
    //  printf("inode_read_at: this is the sector_idx: %d \n", sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          if (print == 1)
            printf("inode_read_at: this is the sector_idx: %d \n", sector_idx);
          read_cache(sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          read_cache(sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
  lock_release(&inode->file_lock);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */



off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
//  printf("inode_write_at: lock acquired by thread: %p \n", thread_current());
  //printf("page fault");
  lock_acquire(&inode->file_lock);
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  // lock_acquire(&inode->file_lock);
  if (inode->deny_write_cnt) {
    lock_release(&inode->file_lock);
    return 0;
  }
  // lock_release(&inode->file_lock);
 // if (print == 1)
   //   printf("inode_write_at: data length = %d, size = %d, offset = %d \n", inode->data.length, size, offset);
  struct inode_disk data;
  read_cache(inode->sector, &data);
  // lock_acquire(&inode->file_lock);
  if(data.length < offset+size) {
    if(!inode_resize(&data, offset+size)) {
       if (print == 1) 
        printf("inode_write_at: resize failed \n");
      lock_release(&inode->file_lock);
      return 0;
    }
  }
  // lock_release(&inode->file_lock);
  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      if (print3 == 1)
            printf("inode_write_at: this is the sector_idx: %d \n", sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          write_cache(sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            read_cache(sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          write_cache(sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
 // printf("inode_write_at: lock released by thread: %p \n", thread_current());
  lock_release(&inode->file_lock);
  return bytes_written;
}


/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  // lock_acquire(&inode->file_lock);
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  // lock_release(&inode->file_lock);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  // lock_acquire(&inode->file_lock);
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
  // lock_release(&inode->file_lock);
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk *data;
  data = get_block(inode->sector);
  return data->length;
}

struct inode *
inode_parent_open (struct inode *inode)
{
  //printf("inode parent open read");
  if (inode != NULL)
    {
      struct inode_disk inode_sector;
      read_cache (inode->sector, &inode_sector);
      block_sector_t parent_sector = inode_sector.parent;
      /*if (inode->sector == parent_sector)
      {
        printf("inode sector equals parent sector ");
      }*/
      inode = inode_open (parent_sector);
      //inode = inode_open (inode->data.parent);
    }
  return inode;
}

bool
inode_is_directory (struct inode *inode)
{
  //printf("inode is directory read");
  struct inode_disk inode_sector;
  read_cache (inode->sector, &inode_sector);
  bool is_directory = inode_sector.isdirectory;
  return is_directory;
  //return inode->data.isdirectory;
}

off_t
inode_ofs (struct inode *inode) {
  //printf("inode ofs read");
  struct inode_disk inode_sector;
  read_cache (inode->sector, &inode_sector);
  off_t ofs = inode_sector.ofs;
  return ofs;
  //return inode->data.ofs;
}

uint32_t
inode_file_cnt (struct inode *inode)
{
  //printf("inode file cnt read");
  struct inode_disk inode_sector;
  read_cache (inode->sector, &inode_sector);
  uint32_t num_files = inode_sector.num_files;
  return num_files;
  //return inode->data.num_files;
}

bool
inode_file_add (struct inode *parent, block_sector_t sector, off_t ofs)
{
  //printf("inode file add read/write");
  if (!inode_is_directory (parent))
    return false;
  struct inode_disk inode_sector;
  read_cache (sector, &inode_sector);
  inode_sector.parent = parent->sector;
  inode_sector.ofs = ofs;
  write_cache (sector, &inode_sector);
  read_cache (parent->sector, &inode_sector);
  inode_sector.num_files += 1;
  write_cache (parent->sector, &inode_sector);
  return true;
  /*if (!inode_is_directory (parent))
    return false;
  struct inode_disk *inode_sector;
  inode_sector->parent = parent->sector;
  inode_sector->ofs = ofs;
  block_write (fs_device, sector, inode_sector);
  parent->data.num_files += 1;
  return true;*/
}

bool
inode_file_remove (struct inode *inode)
{
  //printf("inode file remove read/write");
  if (inode_is_directory (inode))
    {
      struct inode_disk inode_sector;
      read_cache (inode->sector, &inode_sector);
      inode_sector.num_files -= 1;
      write_cache (inode->sector, &inode_sector);
      return true;
    }
  return false;
  /*if (inode_is_directory (inode))
    {
      inode->data.num_files -= 1;
      static char zeros[BLOCK_SECTOR_SIZE];
      block_write (fs_device, inode->sector, zeros);
      return true;
    }
  return false;*/
}

int
inode_open_cnt (struct inode *inode)
{
  return inode->open_cnt;
}
