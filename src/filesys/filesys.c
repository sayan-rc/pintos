#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer-cache.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static bool parse_path (const char *path, char file_name[NAME_MAX + 1],
                        struct dir **directory_ptr);
static int get_next_part (char part[NAME_MAX + 1], const char **srcp);
static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  cache_init();

  if (format)
    do_format ();

  free_map_open ();
  thread_current ()->cwd = inode_open (ROOT_DIR_SECTOR);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
  cache_flush();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */

bool
filesys_create (const char *name, off_t initial_size, bool is_directory)
{
  block_sector_t inode_sector = 0;
  struct dir *dir = NULL;
  char file_name[NAME_MAX + 1];
  //printf("filesys create");
  bool success = (parse_path (name, file_name, &dir)
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_directory)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = NULL;
  struct inode *inode = NULL;
  char file_name[NAME_MAX + 1];

  if (parse_path (name, file_name, &dir))
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *dir = NULL;
  char file_name[NAME_MAX + 1];
  //printf("filesys remove");
  bool success = (parse_path (name, file_name, &dir)
                 && dir_remove (dir, file_name));
  dir_close (dir);

  return success;
}

bool
filesys_chdir (const char *name)
{
  struct dir *dir = NULL;
  struct inode *inode = NULL;
  char file_name[NAME_MAX + 1];
  struct thread *cur_thread = thread_current ();
  //printf("filesys chdir");
  bool success = (parse_path (name, file_name, &dir)
                  && dir_lookup (dir, file_name, &inode));
  dir_close (dir);
  if (success)
    {
      inode_close (cur_thread->cwd);
      cur_thread->cwd = inode;
    }
  return success;
}

static bool
parse_path (const char *path, char file_name[NAME_MAX + 1],
            struct dir **directory_ptr)
{
  struct inode *inode;
  struct inode *inode_next;
  if (path[0] == '\0')
    return false;
  inode_next = path[0] == '/' ? inode_open (ROOT_DIR_SECTOR)
                              : inode_reopen (thread_current ()->cwd);
  inode = inode_next;

  while (get_next_part (file_name, &path) == 1)
    {
      struct dir *directory = dir_open (inode_reopen (inode));
      //printf("parse path");
      dir_lookup (directory, file_name, &inode_next);
      dir_close (directory);
      if (inode_next == NULL || !inode_is_directory (inode_next))
        break;
      inode_close (inode);
      inode = inode_next;
    }

  if (get_next_part (file_name, &path) != 0)
    return false;
  if (inode == inode_next)
    strlcpy (file_name, ".", 2);
  else
    inode_close (inode_next);

  *directory_ptr = dir_open (inode);
  bool success = *directory_ptr == NULL ? false : true;
  return success;
}

static int
get_next_part (char part[NAME_MAX + 1], const char **srcp)
{
  const char *src = *srcp;
  char *dst = part;

  /* Skip leading slashes. If it’s all slashes, we’re done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;

  /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
  while (*src != '/' && *src != '\0')
  {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++;
  }
  *dst = '\0';

  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
