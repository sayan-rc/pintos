#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;
struct inode_disk;
struct inode;

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool is_directory);
bool inode_resize(struct inode_disk *id, off_t size);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
struct inode *inode_parent_open (struct inode *inode);
bool inode_is_directory (struct inode *inode);
off_t inode_ofs (struct inode *inode);
uint32_t inode_file_cnt (struct inode *inode);
bool inode_file_remove (struct inode *inode);
int inode_open_cnt (struct inode *inode);

#endif /* filesys/inode.h */
