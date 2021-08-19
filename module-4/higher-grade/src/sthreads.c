/* On Mac OS (aka OS X) the ucontext.h functions are deprecated and requires the
   following define.
*/
#define _XOPEN_SOURCE 700

/* On Mac OS when compiling with gcc (clang) the -Wno-deprecated-declarations
   flag must also be used to suppress compiler warnings.
*/

#include <signal.h>   /* SIGSTKSZ (default stack size), MINDIGSTKSZ (minimal
                         stack size) */
#include <stdio.h>    /* puts(), printf(), fprintf(), perror(), setvbuf(), _IOLBF,
                         stdout, stderr */
#include <stdlib.h>   /* exit(), EXIT_SUCCESS, EXIT_FAILURE, malloc(), free() */
#include <ucontext.h> /* ucontext_t, getcontext(), makecontext(),
                         setcontext(), swapcontext() */
#include <stdbool.h>  /* true, false */

#include "sthreads.h"
#include <sys/time.h>
#include <string.h>
/* Stack size for each context. */
#define STACK_SIZE SIGSTKSZ*100
#define TIMEOUT 50
#define TIMER_TYPE ITIMER_REAL
/*******************************************************************************
                             Global data structures

                Add data structures to manage the threads here.
********************************************************************************/
typedef struct thread_queue {
	thread_t * first_t ;
	int thread_n ;
} thread_queue ;

int tid_s ;
struct thread_queue * t_queue; 
thread_t * curr ;
/*******************************************************************************
                             Auxiliary functions

                      Add internal helper functions here.
********************************************************************************/
int scheduler();
int timer_signal(int timer_type){

	int sig;

	switch(timer_type) {
		case ITIMER_REAL:
			sig = SIGALRM;
			break;
		case ITIMER_VIRTUAL:
			sig = SIGVTALRM;
			break;
		case ITIMER_PROF:
			sig = SIGPROF;
			break;
		default:
			fprintf(stderr, "ERROR: unknown timer type %d!\n", timer_type);
			exit(EXIT_FAILURE);
	}

	return sig;
}


void set_timer (int type, void (* handler) (int), int ms)
{
	struct itimerval timer;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	sigaction(timer_signal(type), &sa, NULL );

	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = ms;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;

	if( setitimer (type, &timer, NULL) < 0 ){
		perror("Setting timer");
		exit(EXIT_FAILURE);
	}

}

void timer_handler (int signum){
	static int count = 0;
	fprintf(stderr, "=======> timer( %03d ) curr_tid : %d\n", count++, curr->tid);
	yield();
}

void init_context(ucontext_t * ctx, ucontext_t * next){

	void * stack = malloc(STACK_SIZE);
	if( stack == NULL ){
		perror("Allocating stack");
		exit(EXIT_FAILURE);
	}

	if( getcontext(ctx) < 0 ){
		perror("get context");
		exit(EXIT_FAILURE);
	}

	ctx->uc_link = next;
	ctx->uc_stack.ss_sp = stack;
	ctx->uc_stack.ss_size = STACK_SIZE;
	ctx->uc_stack.ss_flags = 0;

	set_timer(TIMER_TYPE, timer_handler, TIMEOUT);
}

void init_context_0 (ucontext_t * ctx, void (* func)(), ucontext_t * next) {
	init_context(ctx, next);
	makecontext(ctx, func, 0);
}

void init_context_1 (ucontext_t * ctx, void (* func)(), const char * str, ucontext_t * next){
	init_context(ctx, next);
	makecontext(ctx, func, 1, str);
}

int insert_t_queue(thread_t * new){

	if( t_queue->first_t == NULL ){
		t_queue->first_t = new;
		t_queue->thread_n++;
		return 0;
	}

	thread_t * i = t_queue->first_t;
	for( ; i->next != NULL ; i = i->next ) ;
	i->next = new;
	t_queue->thread_n++;
	return 0;;
}

int delete_t_queue(tid_t term_tid){

	if( t_queue->first_t == NULL ){
		perror("delete_t_queue");
		return -1;
	}

	thread_t * i = t_queue->first_t;
	thread_t * prev = NULL;

	if( i->tid == term_tid ){
		t_queue->thread_n--;
		t_queue->first_t = i->next;
		free(i);
		return 0;
	}

	for( ; i != NULL ; i = i->next ){
		if( i->tid == term_tid ){
			break;
		}
		prev = i;
	}
	
	if( i == NULL ){
		return -1;
	}

	t_queue->thread_n--;
	prev->next = i->next;
	free(i);

	printf("current t_queue size = %d\n", t_queue->thread_n);
	thread_t * k = t_queue->first_t;
	for( ; k != NULL ; k = k->next )
		printf("tid : %d | ", k->tid);
	printf("=======================\n");
	return 0;

}


int scheduler(){

	// TODO change the state of the calling thread from running to ready
	if( curr == 0x0 ){
		perror("scheduler");
		return -1;
	}


	thread_t * prev = curr;
	thread_t * i = curr->next;
	for( ; i != NULL ; i = i->next ){
		if(i->state != ready)
			continue;
		// dispatch one of the threads in the ready state
		curr = i;
		curr->state = running;	
		set_timer(TIMER_TYPE, timer_handler, TIMEOUT);
		swapcontext(&prev->ctx,&curr->ctx);
		return 0;
	}

	for( i = t_queue->first_t ; i != curr->next ; i = i->next ){
		if(i->state != ready)
			continue;
		curr = i;
		curr->state = running;
		set_timer(TIMER_TYPE, timer_handler, TIMEOUT);
		swapcontext(&prev->ctx,&curr->ctx);
		return 0;
	}

	return 0;

}

/*******************************************************************************
                    Implementation of the Simple Threads API
********************************************************************************/


int init(){

	t_queue = (thread_queue *) malloc (sizeof(thread_queue));
	if(t_queue == NULL){
		perror("init thread_queue");
		return -1;
	}
	t_queue->first_t = NULL;
	t_queue->thread_n = 0;
	
	tid_s = 0;
	curr = (thread_t *) malloc (sizeof(thread_t));
	curr->state = running;
	getcontext(&curr->ctx);
	curr->next = NULL;
     	curr->tid = tid_s++;

	if( insert_t_queue(curr) < 0 ){
		perror("insert t_queue");
		return -1;
	}

	return curr->tid;
}


tid_t spawn(void (*start)()){

	thread_t * new = (thread_t *) malloc ( sizeof(thread_t) );
	new->state = ready;
	init_context_0(&new->ctx, start, 0);
	new->next = NULL;
	new->tid = tid_s++;

	if ( insert_t_queue(new) < 0){
		perror("insert t_queue");
		return -1;
	}

     	return new->tid;
}

void yield(){

	curr->state = ready;
	scheduler();
		
}

void done(){
	
	curr->state = terminated;
	thread_t * i = t_queue->first_t;
	for( ; i != NULL ; i = i->next ){
		if( i->state == waiting ){
			i->state = ready;
		}
	}
	scheduler();
}


tid_t join() {
	
	curr->state = waiting;
	scheduler();

	thread_t * i = t_queue->first_t;
	for( ; i != NULL ; i = i->next ){
		if( i->state == terminated ){
			tid_t term_tid = i->tid;
			i = t_queue->first_t;
			printf("delete_t_queue start\n");
			for( ; i != NULL ; i = i->next )
				printf("-- tid %d | ", i->tid);
		        printf("\n\n");	
			delete_t_queue(term_tid);
			for( ; i != NULL ; i = i->next )
                                printf("-- tid %d | ", i->tid);
                        printf("\n\n");
			printf("term_tid : %d\n", term_tid);
			return term_tid;
		}
	}

	return -1;
}