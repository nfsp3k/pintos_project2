#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void isUseraddr(void * esp, int argnum, int pointer_index);
struct file * fd2file(int fd);
#endif /* userprog/syscall.h */
