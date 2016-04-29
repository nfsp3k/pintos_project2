#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* List of processes in THREAD_READY state, that is, processes
 *    that are ready to run but not actually running. */
struct list ready_list;

/* Thread identifier type.
   You can redefine this to whatever type you like. */


typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int nice;				/* Niceness*/
    int recent_cpu;			/* recent_cpu*/
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    struct list_elem elem2;		/* For donate_list */
    struct list_elem elem3;		/* For block_list */
    struct list donate_list;		/* List of threads which try to acquire the lock								   acquired by this thread */
    
   int64_t wakeup_ticks;		/*when wakeup_ticks equals to timer_tick(), thread wakeup */
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
 
    struct list_elem child_elem;		/* Fof child_list*/

    struct list child_list;				/* List of child threads */
    struct list terminated_child_list;	/* List of terminated child threads */

	struct semaphore exit_sema; /* exit_sema becomes down right after the thread is created.
								  * exit_sema goes up when the thread calls exit() system call. */

    struct thread * parent;		/* thread pointer to parent*/
    struct list fd_list;		/* file discript list*/

    int fd_num;				/* num of fd*/

	struct file *exec_file; /**/ 
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

struct terminated_proc_info{
    struct list_elem terminated_elem;
	tid_t tid;	
	int exit_status;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void push2sleep(int64_t ticks);
bool wakeup_tick_compare (const struct list_elem *a,
                          const struct list_elem *b,
                          void *aux UNUSED);

void updatesleep(int64_t ticks);

int get_priority(struct thread *target);
void push2mlfq(struct thread *input);
bool priority_compare (const struct list_elem *a,
                              const struct list_elem *b,
                              void *aux UNUSED);


struct list_elem * pop_from_mlfq(void);
int num_ready(void);

void load_update(void);
void recent_cpu_update(void);
void priortiy_update(void);
void increase_recent_cpu(void);
//arithmetic operation
#define ppp 17
#define qqq 14
#define fff (1<<qqq)

#define int2fp(n) n*fff
#define fp2int_zero(x) x/fff
#define fp2int_round(x) ( (x>0)?((x+fff/2)/fff) : ((x-fff/2))/fff )
#define addfp(x, y) x+y
#define subfp(x, y) x-y
#define addfp_int(x, n) x+n*fff
#define subfp_int(x, n) x-n*fff
#define multiply_fp(x, y) ((int64_t)x) * y / fff
#define multiply_fp_int(x, n) x*n
#define divide_fp(x, y) ((int64_t) x) * fff / y
#define divide_fp_int(x, n) x/n

#endif /* threads/thread.h */


