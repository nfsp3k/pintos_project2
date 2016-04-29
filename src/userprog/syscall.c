#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "threads/init.h"
#include "devices/input.h"
#include "userprog/pagedir.h"


static void syscall_handler (struct intr_frame *);
struct thread * curr;
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  struct lock sys_lock;
  lock_init(&sys_lock);
  curr = thread_current();

  /* Variables uesd by system calls of several kinds*/
	int fd;
	char *buffer;
	int length;
	struct list_elem *find;
	struct file *file;
	unsigned position;

  /* Check the address of esp */
  isUseraddr(f->esp,0,0);
  if(!pagedir_get_page(curr->pagedir, f->esp))
	  sys_exit(-1);

  int sys_num = *(int *)(f->esp);
  switch(sys_num)
  {
	// pid_t exec (const char *file)
	case SYS_EXEC:
		isUseraddr(f->esp,1,1);
		if(*(char **)(f->esp + 4) == NULL){
			f->eax = TID_ERROR;
			break;
		}

 		f->eax = process_execute( *(char **)(f->esp + 4));
		break;
	
	//bool create (const char *file, unsigned initial_size)
	case SYS_CREATE:
		isUseraddr(f->esp,2,1);
		
  		lock_acquire(&sys_lock);
		f->eax = filesys_create( *(char **)(f->esp+4), *(off_t *)(f->esp+8));
  		lock_release(&sys_lock);
		break;

	// void exit (int status)
	case SYS_EXIT:
		isUseraddr(f->esp,1,0);
		sys_exit(*(int *)(f->esp+4));
		break;

	//int read (int fd, void *buffer, unsigned size)
	case SYS_READ:
		isUseraddr(f->esp,3,2);
  		lock_acquire(&sys_lock);

		fd = *(int*)(f->esp+4);
		buffer = *(char**)(f->esp+8);	
		length = *(int*)(f->esp+12);
		
		/* stdin case*/
		if(fd == 0){
			int i = 0;

			uint8_t returnc;
			/* read untill null character appears or length is done*/
			while(i < length){
			returnc = input_getc();
			if(returnc=='\n')
			break;
			buffer[i++]=returnc;
			}
			f->eax = i;

		}
		/* stdout in case*/
		else if(fd ==1)
		f->eax = -1;
		/* normal case read from file*/
		else{
			file=NULL;
			bool suc=false;
			for(find = list_begin(&curr->fd_list);
			find != list_end(&curr->fd_list);
			find = list_next(find))
			{
				file = list_entry(find, struct file, elem);
				if(file->fd == fd)
				{
					suc=true;
					break;
				}
				
			}
			if(suc==false){
				lock_release(&sys_lock);
				f->eax = -1;
				break;
			}
			
			f->eax = file_read(file, buffer, length);
		}
		
		lock_release(&sys_lock);
		break;

	//int filesize (int fd)
	case SYS_FILESIZE:
		isUseraddr(f->esp,1,0);
  		lock_acquire(&sys_lock);
		fd = *(int *)(f->esp + 4);
		
		/* find file from fd_list*/	
		for(find = list_begin(&curr->fd_list);
		find != list_end(&curr->fd_list);
		find = list_next(find))
		{
			file = list_entry(find, struct file, elem);
			if(file->fd == fd)
				break;
		}
		if(file == NULL){
			lock_release(&sys_lock);
			f->eax = -1;
			break;
		}
		
		f->eax = file_length(file);
	

		lock_release(&sys_lock);
		break;

    // int wait (tid_t tid)
	case SYS_WAIT:
		isUseraddr(f->esp,1,0);
		tid_t pid = *(tid_t*)(f->esp+4);
		f->eax = process_wait(pid);
		break;

    // int open (const char * file)
  	case SYS_OPEN:
  		isUseraddr(f->esp,1,1);
  		lock_acquire(&sys_lock);
  

  		char * name = *(char **)(f->esp+4);
		file = filesys_open(name);
		/* null pointer*/
		if(file == NULL)
			f->eax = -1;
		else
		{
			/* assgin fd_num*/
			f->eax = curr->fd_num;
			file->fd= curr->fd_num;
			curr->fd_num++;

			/* push fd information to thread*/
			list_push_back(&curr->fd_list, &file->elem);
		}
		lock_release(&sys_lock);
		break;
		
  
    // int write (int fd, const void *buffer, unsigned length);
	case SYS_WRITE:
		isUseraddr(f->esp,3,2);
		lock_acquire(&sys_lock);

		fd = *(int*)(f->esp+4);
		buffer = *(char**)(f->esp+8);	
		length = *(int*)(f->esp+12);

		/*stdin case*/  
		if(fd == 0)
			f->eax=-1;

		/*stdout case*/
		else if(fd==1)
		{
			putbuf(buffer, length);
			f->eax=length;
		}
		/*normal case write from file*/
		else
		{
			file = fd2file(fd);

			if(!file)
				f->eax = -1;
			else{
				f->eax = file_write(file, buffer, length);		
			}
		}
		lock_release(&sys_lock);
		break;

	//void seek (int fd, unsigned position)
	case SYS_SEEK:
		isUseraddr(f->esp,2,0);
		fd = *(int*)(f->esp+4);
		position = *(unsigned*)(f->esp+8);

		file = fd2file(fd);
		file_seek(file, (off_t)position);
		
		break;

	// void close (int fd)
	case SYS_CLOSE:
		isUseraddr(f->esp,1,0);
		fd = *(int*)(f->esp+4);

		file = fd2file(fd);

		/* Remove the elem of the file in the fd_list of the thread */
		enum intr_level old_level = intr_disable ();
		list_remove(&file->elem);
		intr_set_level (old_level);

		/* Close the file */
		file_close(file);
		break;

	case SYS_TELL:
		isUseraddr(f->esp,1,0);
		fd = *(int*)(f->esp+4);

		file = fd2file(fd);

		f->eax = file_tell(file);
		break;

	case SYS_HALT:
		power_off();
		break;

   }
}


//check that the address is user address and check it is valid address
void
isUseraddr(void *esp, int argnum, int pointer_index)
{
	//check it is user address 
	if((uint32_t)esp + 4 + argnum*4 > (uint32_t) PHYS_BASE)
	sys_exit(-1);

	//check the pointer address is valid
	if(pointer_index > 0)
	{
		char **temp= (char**)(esp+pointer_index*4);

		if((uint32_t)*temp >= (uint32_t) PHYS_BASE)
			sys_exit(-1);

		// char pointer validation check
		if(!pagedir_get_page(curr->pagedir, *temp))
        	sys_exit(-1);
	}

	
}
//change fd to file pointer
struct file *
fd2file(int fd)
{
	struct list_elem *find;
	for(find = list_begin(&curr->fd_list);
		find != list_end(&curr->fd_list);
		find = list_next(find))
	{
		struct file * f = list_entry(find, struct file, elem);
		
		if(fd== f->fd)
		return f;
	}

	//can not find fd
	return 0;
}
