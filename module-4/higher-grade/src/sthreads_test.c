#include <stdlib.h>   // exit(), EXIT_FAILURE, EXIT_SUCCESS
#include <stdio.h>    // printf(), fprintf(), stdout, stderr, perror(), _IOLBF
#include <stdbool.h>  // true, false
#include <limits.h>   // INT_MAX
#include <string.h>
#include "sthreads.h" // init(), spawn(), yield(), done()
int flag = 0;
/*******************************************************************************
                   Functions to be used together with spawn()

    You may add your own functions or change these functions to your liking.
********************************************************************************/

/* Prints the sequence 0, 1, 2, .... INT_MAX over and over again.
 */
void numbers() {
	int n = 0;
	while (true) {
		printf(" n = %d\n", n);
		n = (n + 1) % (INT_MAX);
		if (n > 3 && flag == 0 ) done();
	}
}

/* Prints the sequence a, b, c, ..., z over and over again.
 */
void letters() {
	char c = 'a';
	while (true) {
		printf(" c = %c\n", c);
		if (c == 'f' && flag == 0 ) done();
		c = (c == 'z') ? 'a' : c + 1;
	}
}

/* Calculates the nth Fibonacci number using recursion.
 */
int fib(int n) {
	switch (n) {
		case 0:
			return 0;
		case 1:
			return 1;
		default:
			return fib(n-1) + fib(n-2);
	}
}

/* Print the Fibonacci number sequence over and over again.

   https://en.wikipedia.org/wiki/Fibonacci_number

   This is deliberately an unnecessary slow and CPU intensive
   implementation where each number in the sequence is calculated recursively
   from scratch.
*/

void fibonacci_slow() {
	int n = 0;
	int f;
	while (true) {
		f = fib(n);
		if (f < 0) {
			if(flag) {
				// Restart on overflow.
				n = 0;
			}
			else {
				done();
			}
		}
		printf("slow fib(%02d) = %d\n", n, fib(n));
		n = (n + 1) % INT_MAX;
	}
}

/* Print the Fibonacci number sequence over and over again.

   https://en.wikipedia.org/wiki/Fibonacci_number

   This implementation is much faster than fibonacci().
*/
void fibonacci_fast() {
	int a = 0;
	int b = 1;
	int n = 0;
	int next = a + b;
	int cnt = 0;
	while(true) {
		printf(" fast fib(%02d) = %d\n", n, a);
		next = a + b;
		a = b;
		b = next;
		n++;
		if (a < 0) {
			if(flag){
				// Restart on overflow
				a = 0;
				b = 1;
				n = 0;
			} else {
				done();
			}
		}	
		cnt++;
  	}
}

/* Prints the sequence of magic constants over and over again.

   https://en.wikipedia.org/wiki/Magic_square
*/
void magic_numbers() {
	int n = 3;
	int m;
	
	while (true) {
		m = (n*(n*n+1)/2);
		if (m > 0) {
			printf(" magic(%d) = %d\n", n, m);
			n = (n+1) % INT_MAX;
		} else {
			// Start over when m overflows.
			if(flag)
				n = 3;
			else
				done();
		}
		yield();
	}
}

/*******************************************************************************
                                     main()

            Here you should add code to test the Simple Threads API.
********************************************************************************/


int main(int argc, char * argv[] ){
	if( argc < 2 ) {
		perror("few arguments\n");
		exit(EXIT_FAILURE);
	}

	puts("\n==== Test program for the Simple Threads API ====\n");
	init();
	if(strcmp(argv[1], "done_join") == 0){
		puts("done_join_test\n");
		int fib_fast_id = spawn(fibonacci_fast);
		int numbers_id = spawn(numbers);
		int letters_id = spawn(letters);
		int magic_id = spawn(magic_numbers);
		while(1){
			if ( join() == fib_fast_id ){
				puts("=================================\n");
				fib_fast_id = -1;
			}
			if ( join() == numbers_id ){
				puts("=================================\n");
				numbers_id = -1;
			}
			if ( join() == letters_id ){
				puts("=================================\n");
				letters_id = -1;
			}
			if ( join() == magic_id ){
				puts("=================================\n");
				magic_id = -1;
			}
			if ( fib_fast_id == -1 && numbers_id == -1 && letters_id == -1 && magic_id == -1 ){
				done();
				break;
			}
		}
		return 0;

	}

	if(strcmp(argv[1], "preemptive") == 0){
		flag = 1;
		puts("preemptive scheduling\n");
		spawn(numbers);
		spawn(letters);
		spawn(fibonacci_fast);
		while(1){
			puts("main\n");
		}
		return 0;
	}

}
