#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "threads/init.h"
#include "filesys/buffer-cache.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "userprog/process.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
bool valid_address (void *address);
void validate_pointer (void *ptr, size_t size);
void validate_string (char *str);
struct file_object * get_file (int fd);

/* Global lock to avoid concurrent filesystem function calls. */
struct lock filesys_lock;

void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t *args = ((uint32_t *) f->esp);
  validate_pointer (args, sizeof (uint32_t));

  /* Conditions to check for the validity of the arguments
  passed into each call to SYS_CALL*/
  if (args[0] != SYS_HALT)
  {
  	validate_pointer (&args[1], sizeof (uint32_t));
  }
  if (args[0] == SYS_CREATE || args[0] == SYS_READ || args[0] == SYS_WRITE 
      || args[0] == SYS_SEEK || args[0] == SYS_READDIR)
  {
    validate_pointer (&args[2], sizeof (uint32_t));
  }
  if (args[0] == SYS_READ || args[0] == SYS_WRITE)
  {
    validate_pointer (&args[3], sizeof (uint32_t));
  }

  if (args[0] == SYS_EXEC || args[0] == SYS_CREATE || args[0] == SYS_REMOVE 
      || args[0] == SYS_OPEN || args[0] == SYS_MKDIR || args[0] == SYS_CHDIR)
  {
  	validate_string ((char *) args[1]);
  }
  else if (args[0] == SYS_READ || args[0] == SYS_WRITE)
  {
  	validate_pointer ((void *) args[2], args[3]);
  }
  else if (args[0] == SYS_READDIR)
  {
    validate_pointer ((void *) args[2], (NAME_MAX + 1) * sizeof (char));
  }

  /* Conditions to handle Process System Calls */ 
  if (args[0] == SYS_EXIT)
  {
    thread_current()->wait_status->exit_code = args[1];
    printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
    thread_exit ();
  }
  else if (args[0] == SYS_HALT)
  {
    shutdown_power_off();
  }
  else if (args[0] == SYS_EXEC)
  {
    f->eax = process_execute((char *) args[1]);
  }
  else if (args[0] == SYS_WAIT)
  {
    f->eax = process_wait((char *) args[1]);
  }
  else if (args[0] == SYS_PRACTICE)
  {
  	f->eax = args[1] + 1;
  }

  /* Conditions to handle File System Calls */ 
  else if (args[0] == SYS_CREATE)
  {
  	//lock_acquire (&filesys_lock);
  	f->eax = filesys_create ((char *) args[1], args[2], false);
  	//lock_release (&filesys_lock);
  }
  else if (args[0] == SYS_REMOVE)
  {
  	//lock_acquire (&filesys_lock);
  	f->eax = filesys_remove ((char *) args[1]);
  	//lock_release (&filesys_lock);
  }
  else if (args[0] == SYS_OPEN)
  {
  	//lock_acquire (&filesys_lock);
    struct file *new_file = filesys_open ((char *) args[1]);
    //lock_release (&filesys_lock);

    if (!new_file)
    {
      f->eax = -1;
    }
    else
    {
      struct thread *t = thread_current ();
      struct file_object *file_obj = malloc (sizeof (struct file_object));
      file_obj->file_ptr = new_file;
      file_obj->fd = t->next_fd;
      f->eax = t->next_fd;
      t->next_fd++;
      list_push_back (&t->files, &file_obj->elem);
    }
  }
  else if (args[0] == SYS_READ && args[1] == 0)
  {
    uint8_t *buf = (uint8_t *) args[2];
    size_t i = 0;

    for (; i < args[3]; i++) {
      buf[i] = input_getc ();
      if (buf[i] == '\n')
      {
        break;
      }
    }
    f->eax = i;
  }
  else if (args[0] == SYS_WRITE && args[1] == 1)
  {
    putbuf ((void *) args[2], args[3]);
    f->eax = args[3];
  }
  else if (args[0] == SYS_CHDIR)
  {
    f->eax = filesys_chdir ((char *) args[1]);
  }
  else if (args[0] == SYS_MKDIR)
  {
    f->eax = filesys_create ((char *) args[1], 0, true);
  }
  else
  {
  	struct file_object *file_obj = get_file (args[1]);
  	//lock_acquire (&filesys_lock);

  	if (file_obj == NULL)
  	{
  	  f->eax = -1;
  	}
    else if (args[0] == SYS_FILESIZE)
  	{
  	  f->eax = file_length (file_obj->file_ptr);
  	}
  	else if (args[0] == SYS_READ)
  	{
      if (file_is_directory (file_obj->file_ptr))
        f->eax = -1;
      else
  	    f->eax = file_read (file_obj->file_ptr, (void *) args[2], args[3]);
  	}
  	else if (args[0] == SYS_WRITE)
  	{
      if (file_is_directory (file_obj->file_ptr))
        f->eax = -1;
      else
  	    f->eax = file_write (file_obj->file_ptr, (void *) args[2], args[3]);
  	}
  	else if (args[0] == SYS_SEEK)
  	{
  	  file_seek (file_obj->file_ptr, args[2]);
  	}
  	else if (args[0] == SYS_TELL)
  	{
  	  f->eax = file_tell (file_obj->file_ptr);
  	}
  	else if (args[0] == SYS_CLOSE)
  	{
  	  file_close (file_obj->file_ptr);
  	  list_remove (&file_obj->elem);
  	  free (file_obj);
    }
    else if (args[0] == SYS_INUMBER)
    {
      f->eax = file_inumber (file_obj->file_ptr);
    }
    else if (args[0] == SYS_READDIR)
    {
      f->eax = dir_readdir ((struct dir *) file_obj->file_ptr,
                            (char *) args[2]);
    }
    else if (args[0] == SYS_ISDIR)
    {
      f->eax = file_is_directory (file_obj->file_ptr);
    }
    //lock_release (&filesys_lock);
  }
}

/* Checks whether inputted address is a valid address in
the user space */
bool valid_address (void *address)
{
  return is_user_vaddr (address) &&
         pagedir_get_page (thread_current ()->pagedir, address) != NULL;
}

/* Checks whether inputted address is a valid pointer in
the user space */
void validate_pointer (void *ptr, size_t size)
{
  if (!valid_address (ptr) || !valid_address (ptr + size))
  {
  	printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
  	thread_exit ();
  }
}

/* Checks whether inputted address is a valid string in
the user space */
void validate_string (char *str)
{
  if (is_user_vaddr (str))
  {
    char *kernel_str = pagedir_get_page (thread_current ()->pagedir, str);
    if (kernel_str != NULL && valid_address (str + strlen (kernel_str) + 1))
    {
      return;
    }
  }
  printf ("%s: exit(%d)\n", &thread_current ()->name, -1);
  thread_exit ();
}

struct file_object * get_file (int fd)
{
  struct list *file_list = &thread_current ()->files;
  struct list_elem *e = list_begin (file_list);

  while (e != list_end (file_list))
  {
    struct file_object *f = list_entry (e, struct file_object, elem);
    if (f->fd == fd)
    {
      return f;
    }
    e = list_next (e);
  }
  return NULL;
}
