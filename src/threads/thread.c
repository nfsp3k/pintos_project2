// Hi hello
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#include <string.h>
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
//static struct list ready_list;
/* List of processes which is sleeping*/
static struct list sleep_list;
/* List of processes which is blocked*/
static struct list block_list;
/* load = load_avg it shows the average load in ready queue*/
static int load;
/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
void priority_update(void);
static int i;
/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&sleep_list);
  list_init (&ready_list);
  list_init (&block_list);

  if(thread_mlfqs)
	  load=0; 


  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();

}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL){
    return TID_ERROR;
  }

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  /* initialize members*/
  t->nice=thread_current()->nice;
  t->recent_cpu=0;
  /* fd is started from 2*/
  t->fd_num=2;
  list_init(&t->fd_list);

  /* update child parent information*/
  if(strcmp(t->name,"main") && strcmp(t->name,"idle"))
	  list_push_back(&thread_current()->child_list, &t->child_elem);

  /* Keep the information of the parent process into the child process structure */
  t->parent = thread_current();

  /* Add to run queue. */
  thread_unblock (t);

  if(get_priority(t) > thread_get_priority()){
    thread_yield();
  }

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);
  
  enum intr_level old_level;
  old_level = intr_disable ();
  /*push elem3 to block_list*/
  if(thread_current()!=idle_thread)
	  list_push_back (&block_list, &thread_current()->elem3);

  intr_set_level (old_level);
  
  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  /*remove elem3 from blcok_list*/
  if((t->elem3.prev != NULL) && (t->elem3.next != NULL))
  {
  	list_remove(&t->elem3);
  	t->elem3.next=NULL;
  	t->elem3.prev=NULL; 
  }
  /*upadte recent_cpu priority when thread unblocked*/
  if(thread_mlfqs)
  {
	
  	t->recent_cpu= multiply_fp( divide_fp( (2*load), (2*load+int2fp(1))), t->recent_cpu) +int2fp(t->nice);
  	t->priority = PRI_MAX - fp2int_round(t->recent_cpu/4) - t->nice*2;				

  	if(t->priority>PRI_MAX)
		t->priority=PRI_MAX;
  	if(t->priority<PRI_MIN)
		t->priority=PRI_MIN;
				
  	push2mlfq(t);
  }
  else
	  list_push_back (&ready_list, &t->elem);
  
  t->status = THREAD_READY;
  intr_set_level (old_level);
  
   
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());
 
#ifdef USERPROG
  process_exit ();
#endif

  /* Just set our status to dying and schedule another process.
     We will be destroyed during the call to schedule_tail(). */
  intr_disable ();
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *curr = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());
  old_level = intr_disable ();

  /* insert thread in ready list which is not idle*/
  if (curr != idle_thread)
  {
  	if(thread_mlfqs)
		push2mlfq(curr);
  
  	else
		list_push_back (&ready_list, &curr->elem);
  }

  curr->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}
/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
	if(!thread_mlfqs)
	{
		thread_current ()->priority = new_priority;

		/* When the changed priority of the running thread, the higher priority thread in ready list */ 
		/* should be running if the priority of the running thread is lower than the priority of one */ 
		/* of threads in ready list. */
		enum intr_level old_level;
		old_level = intr_disable ();

		if(!list_empty(&ready_list))
		{
			struct list_elem *find;
			struct thread * higher_priority_thread;
			int max=new_priority;
			int compare;

			/* Looking for the highest priority thread in the ready list */
			for(find = list_begin(&ready_list);
				find != list_end(&ready_list); 
				find = list_next(find))
			{	
				ASSERT(find!=NULL);
				struct thread * temp = list_entry(find, struct thread, elem);

				ASSERT(temp!=NULL);
				compare=get_priority(temp);

				if(compare > max){
					max = compare;
					higher_priority_thread=temp;
				}
			}

			intr_set_level (old_level);

			/* If the highest priority thread in the ready list has higher priority than the running */
			/* thread, the running thread should yield CPU to the thread immediately. */
			if(max > new_priority)
				thread_yield();
		}
	}
}

/* A function for tracking the highest priority recursively among the waiters in donate_list */
int 
get_priority(struct thread *target){
  
  if(!thread_mlfqs)
  {
	int max = target->priority;

	/* If donate_list is empty, */
	if(list_size(&target->donate_list)==0){
		/* then it returns its own priority. */
		return max;
	}else{
		/* If not, it tracks all elem2 from the donate_list recursively. */
		struct list_elem *find;
		for(find = list_begin(&target->donate_list);
			find != list_end(&target->donate_list);
			find = list_next(find))
		{
			struct thread *temp = list_entry(find ,struct thread, elem2);
			int tt = get_priority(temp);
			/* If the corresponding thread has higher priority */
			if( max < tt ){
				/* then it changes the max value with the priority.*/
				max = tt;
			}
		}

		/* It returns the maximum value. */
		return max;
	}
  }
	return -1;
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
	return get_priority(thread_current());  
}

/* Sets the current thread's nice value to NICE.
 * update also recent_cpu and priority */
void
thread_set_nice (int nice ) 
{
  
  struct thread * t=thread_current();
  t->nice=nice;
  t->recent_cpu= multiply_fp( divide_fp( (2*load), (2*load+int2fp(1))), t->recent_cpu) +int2fp(t->nice);
  t->priority = PRI_MAX - fp2int_round(t->recent_cpu/4) - t->nice*2;				

  if(t->priority>PRI_MAX)
	  t->priority=PRI_MAX;
  if(t->priority<PRI_MIN)
	  t->priority=PRI_MIN;
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
	return fp2int_round(load*100);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
	return fp2int_round(thread_current()->recent_cpu*100);
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);
                    
  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Since `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);
 
  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;

  /*advanced scheduler does not use set, get priority*/
  if(!thread_mlfqs) 
	  t->priority = priority;

  list_init(&t->donate_list); /* Initialize donate_list  */
  list_init(&t->child_list);
  list_init(&t->terminated_child_list);

  /* This semaphore will be used for system call wait(). */
  sema_init(&t->exit_sema, 0);
  t->magic = THREAD_MAGIC;
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if(thread_mlfqs)
  {
	struct thread * result;
	struct list_elem * elem = pop_from_mlfq();

	/*if ready_list empty */
	if(elem==NULL)
	{
		result=NULL;
		/*return idle_thread*/
		return idle_thread;
	}
	else
		result = list_entry(elem, struct thread, elem);

	if(result!=NULL)
		return result;
  }
  else
  {
	  if (list_empty (&ready_list))
	    return idle_thread;
	  else{
	    struct list_elem *find;
	    struct thread * higher_priority_thread = list_entry(list_begin(&ready_list), struct thread, elem);
	    int max = -1;

	    /* 1. Looking for the highest priority thread */
	    for(find = list_begin(&ready_list);
		find != list_end(&ready_list);
		find = list_next(find))
	    {
			struct thread * temp = list_entry(find, struct thread, elem);
			int tt = get_priority(temp);
			if(tt > max){
				max = tt;
				higher_priority_thread = temp;
			}
	    }  

	    /* 2. Pop the thread from the ready list */
	    enum intr_level old_level;
	    old_level = intr_disable ();
	    list_remove(&higher_priority_thread->elem);
	    intr_set_level (old_level);

	    /* 3. Return the thread */
	    return higher_priority_thread;  
	  }
  }

  return NULL;
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */

void
schedule_tail (struct thread *prev) 
{
  struct thread *curr = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  curr->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != curr);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   
   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *curr = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;
  
  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (curr->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (curr != next)
	  prev = switch_threads (curr, next);

  schedule_tail (prev); 
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
/* function to let thread elem save in sleep_list*/
void push2sleep(int64_t ticks){
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  old_level = intr_disable ();

  /*assign when thread should wake up*/
  cur->wakeup_ticks = ticks;
  /*sleep list should be maintained by wakeup_ticks order*/
  list_insert_ordered (&sleep_list, &cur->elem, &wakeup_tick_compare, NULL);

  thread_block();
  intr_set_level (old_level);
}
/*function to decide sleep_list order*/
bool wakeup_tick_compare (const struct list_elem *a,
                              const struct list_elem *b,
                              void *aux UNUSED)
{
  struct thread *thread_a = list_entry (a, struct thread, elem);
  struct thread *thread_b = list_entry (b, struct thread, elem);
  return thread_a->wakeup_ticks < thread_b->wakeup_ticks;
}

/*funcion to wake up threads in sleep_list*/
void updatesleep(int64_t ticks){

	if(!list_empty(&sleep_list))
	{
		struct list_elem *first;
		struct thread* temp;

		/*find thread which wake up time is up*/
		for(i=0; i<(int)list_size(&sleep_list)+1; i++)
		{
			temp=list_entry(list_front(&sleep_list), struct thread, elem);

			if(temp->wakeup_ticks<=ticks)
				first=list_pop_front(&sleep_list);
			else
				break;

			thread_unblock (temp);
		}
	}
}



/*function to insert thread's elem in ready_list when we use mlfq*/
void push2mlfq(struct thread *input)
{
	int priority= input->priority;
	ASSERT(priority>=0 && priority<64);
	
	enum intr_level old_level;
	old_level = intr_disable ();
	/*mlfqs list should be maintained by priority order*/
	list_insert_ordered (&ready_list, &input->elem, &priority_compare, NULL);
	intr_set_level (old_level);
}

/*function to decide ready_list order*/
bool priority_compare (const struct list_elem *a,
                              const struct list_elem *b,
                              void *aux UNUSED)
{
  struct thread *thread_a = list_entry (a, struct thread, elem);
  struct thread *thread_b = list_entry (b, struct thread, elem);
  return thread_a->priority > thread_b->priority;
}

/*function to get thread's elem which has highest priority in ready_list when we use mlfq*/
struct list_elem * pop_from_mlfq(void)
{
  if(list_size(&ready_list))
	  return list_pop_front(&ready_list);
  else
	  return NULL;
}
/*function to get number of thread in ready except idle_thread*/
int num_ready(void)
{
	int num=1;
	if(thread_current()==idle_thread)
		num--;	
		
	return num+list_size(&ready_list);
}
/*function to update load_avg*/
void load_update(void)
{ 
  enum intr_level old_level;
  old_level = intr_disable ();

  load = multiply_fp(divide_fp_int(int2fp(59),60), load) + divide_fp_int(int2fp(1),60)*num_ready();
 
  intr_set_level(old_level);
}

/*function to update every thread's recent_cpu*/
void recent_cpu_update()
{	

	int front = divide_fp( (2*load), (2*load+int2fp(1)));
	struct list_elem * e;;
	struct thread * t;

	/*update thread"s recent_cpu in ready_list*/
	for(e = list_begin(&ready_list);
        e != list_end(&ready_list);
        e = list_next(e))
	{
		t=list_entry(e, struct thread, elem);
		t->recent_cpu= multiply_fp(front,t->recent_cpu) +int2fp(t->nice);
	}

	/*update thread"s recent_cpu which is blocked */
	for(e = list_begin(&block_list);
        e != list_end(&block_list);
        e = list_next(e))
	{
		t=list_entry(e, struct thread, elem3);
		t->recent_cpu= multiply_fp(front,t->recent_cpu) +int2fp(t->nice);
	}
	
	t=thread_current();

	/*upadte current thread's recent_cpu*/	
	if(t!=idle_thread)
		t->recent_cpu= multiply_fp(front,t->recent_cpu) +int2fp(t->nice);
		
}

/*function to update every thread's priority*/
void priority_update(void)
{
	
	/*upadte current thread's priority*/	
	struct thread * c = thread_current();
	if(c!=idle_thread)
		c->priority= PRI_MAX - fp2int_round(c->recent_cpu/4) - c->nice*2;
	if(c->priority>PRI_MAX)
		c->priority=PRI_MAX;
	if(c->priority<PRI_MIN)
		c->priority=PRI_MIN;
	
	/*upadte thread's recent_cpu in ready_list*/	
	enum intr_level old_level;
	old_level = intr_disable ();
	struct list_elem * e;;
	struct thread * t;
	for(e = list_begin(&ready_list);
        e != list_end(&ready_list);
        e = list_next(e))
	{
		t=list_entry(e, struct thread, elem);
		if(t!=idle_thread)	
			t->priority = PRI_MAX - fp2int_round(t->recent_cpu/4) - t->nice*2;				

		if(t->priority>PRI_MAX)
			t->priority=PRI_MAX;

		if(t->priority<PRI_MIN)
			t->priority=PRI_MIN;
	 }

	/*resort ready_list because the priority is changed*/
	list_sort(&ready_list,&priority_compare, NULL );	
	intr_set_level(old_level);

}

/*function to increase recent_cpu for every tick*/
void increase_recent_cpu(void)
{
    if(thread_current()!=idle_thread)
		thread_current()->recent_cpu=thread_current()->recent_cpu+int2fp(1);
}
