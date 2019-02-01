#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

struct ThreadArgs{
	int tid;
	int *board;
	int start_index;
	int end_index;
	int width;
	int height;
	int verbose;
	int iterations;
	int num_threads;
	pthread_barrier_t *barrier;
	int row;
	int col;
};
typedef struct ThreadArgs ThreadArgs;

int *readFile(char *filename, int *iterations, int *width, int *height, int *pairs);
int convert(int col_index, int row_index, int num_col);
int edges(int x, int y, int width, int height);
int neighbors(int *board, int x, int y, int width, int height);
static void timeval_subtract(struct timeval *result, struct timeval *end, struct timeval *start);
void* simulate(void *arg);
int *update(ThreadArgs *arg);
void print(int *board, int steps, int width, int height, int last);
void convert_1d_to_2d(ThreadArgs *arg, int index, int width);

int main(int argc, char *argv[]) {
	int *board = NULL;
	int width = 0;
	int height = 0;
	int iterations = 0;
	int verbose = 0;
	int pairs = 0;
	char *filename = NULL;
	int c = -1;
	int p = 0;
	int num_threads = 4;

	while ((c = getopt(argc, argv, "pt:vhc:")) != -1) {
		switch(c) {
			case 'h':
				printf("\n-v:\tverbose mode (print out board as it evolves\n");
				exit(1);
			case 'v':
				verbose = 1;
				break;
			case 'c':
				filename = optarg;
				break;
			case 't':
				num_threads = strtol(optarg, NULL, 10);
				break;
			case 'p':
				p = 1;  // set p option to true
				break;
			default:
				printf("Problem occured");
				exit(1);
		}
	}
	// Read in the configuration file and initialize your game board.
	board = readFile(filename, &iterations, &width, &height, &pairs);
	// check if the board was initialized correctly
	if(board == NULL){
		printf("Error initalizing board");
		exit(1);
	}
	// check if num_threads is 0, less than 0, or more than there are rows
	if(num_threads <= 0 || num_threads > height){
		printf("Invalid thread number input\n");
		exit(1);
	}
	// Start the timer
	struct timeval start_time;
	gettimeofday(&start_time, NULL);

	// initialize the barrier
	int ret;
	pthread_barrier_t barrier1;
	ret = pthread_barrier_init(&barrier1, NULL, num_threads);
	if(ret != 0) // check if there was an error initializing the barrier
		printf("Error initializing the barrier \n");

	// allocate space for workers and arg_structs
	pthread_t *workers = malloc(num_threads*sizeof(pthread_t));
	ThreadArgs *arg_structs = malloc(num_threads*sizeof(ThreadArgs));
	long remainder = height % num_threads;

	// initialize the threads
	int i = 0, j = 0;
	while(i < num_threads && j < width){
		long rows_needed = height/num_threads;
		if(remainder > 0) {
			rows_needed++;
			remainder--;
		}
		arg_structs[i].tid = i;
		arg_structs[i].board = board;
		arg_structs[i].start_index = j * width;
		arg_structs[i].end_index = ((j + rows_needed) * width) - 1;
		arg_structs[i].width = width;
		arg_structs[i].height = height;
		arg_structs[i].verbose = verbose;
		arg_structs[i].iterations = iterations;
		arg_structs[i].num_threads = (int)rows_needed;
		arg_structs[i].barrier = &barrier1;
		// Simulate for the required number of steps.
		pthread_create(&workers[i], NULL, simulate, &arg_structs[i]);
		i++;
		j += rows_needed;
	}
	for(int i = 0; i < num_threads; i++){ // join all the threads
		pthread_join(workers[i], NULL);
	}
	pthread_barrier_destroy(&barrier1);
	// stop the timer and calculate the time the simulation took
	struct timeval end_time;
	gettimeofday(&end_time, NULL);
	struct timeval time;
	timeval_subtract(&time, &end_time, &start_time);
	printf("\n Total time for %d iterations of a %dx%d world is %ld.%06ld seconds \n", iterations, height, width, time.tv_sec, time.tv_usec);
	if(p){
		for(int i = 0; i < num_threads; i++){
			printf("tid:  %d: rows:  %d:%d (%d)\n",arg_structs[i].tid,(arg_structs[i].start_index/arg_structs[i].width),(arg_structs[i].end_index/arg_structs[i].width), arg_structs[i].num_threads);
			}
	}
	// free allocated memory
	free(board);
	free(workers);
	free(arg_structs);
	return 0;
}

/* Convert method changes the 2D array coordinates into a 1D index
 * @param col_index: the index of the column you are accessing
 * @param row_index: the index of the row you are accessing
 * @param num_col: total number of columns
 * @return index of a 1D array emmulating a 2D
*/
int convert(int col_index, int row_index, int num_col){
	int t = (row_index * num_col) + col_index;
	return t;
}

/* timeval_subtract calculates the total time elapsed
 * @param *result: pointer to the time result
 * @param *end: end time
 * @param *start: starting time
 * @returns nothing
 */
static void timeval_subtract(struct timeval *result, struct timeval *end, struct timeval *start) {
	if(end->tv_usec < start->tv_usec) {
		int nsec = (start->tv_usec - end->tv_usec) / 1000000 + 1;
		start->tv_usec -= 1000000 *nsec;
		start->tv_sec -= nsec;
	}
	if(end->tv_usec - start->tv_usec > 1000000) {
		int nsec = (end->tv_usec - start->tv_usec) / 1000000;
		start->tv_usec += 1000000 *nsec;
		start->tv_sec -= nsec;
	}
	result->tv_sec = end->tv_sec - start->tv_sec;
	result->tv_usec = end->tv_usec - start->tv_usec;
}

/* ReadFile reads file and initializes 1D array as well as associated variables
 * @param *filename: name of the file
 * @param *iterations: number of iterations
 * @param *width: number of columns
 * @param *height: number of rows
 * @param *pairs: number of live pairs read in
 * @return *board: 1D initialized board
 */
int *readFile(char *filename, int *iterations, int *width, int *height, int *pairs) {
	FILE *file = fopen(filename,"r");
	if(file == NULL){
		printf("cannot open %s \n", filename);
		exit(1);
	}
	int num = 4;
	for(int i = 0; i < num; i++) {
		int line = 0;
		fscanf(file, "%d", &line);
		if(i == 0){ *height = line; }
		if(i == 1){ *width = line;}
		if(i == 2){ *iterations = line;}
		if(i == 3){ *pairs = line;}
	}

	int *board =  calloc((*width) * (*height), sizeof(int));
	int x, y = 0;
	for(int i = 0; i < *pairs; i++)
	{
		fscanf(file, "%d %d", &x, &y);
		board[convert(x,y,*width)]  = 1;
	}
	fclose(file);
	return board;
}

/* Edges method wraps the edges around, making the board a torus
 * @param x: index of x
 * @param y: index of y
 * @param width: number of columns
 * @param height: number of rows
 * @return: index of array adjusted for torus shape, converted to 1D
 */
int edges(int x, int y, int width, int height) {
	if(x < 0)
		x = width - 1;
	if(x >= width)
		x = 0;
	if(y < 0)
		y = height - 1;
	else if( y >= height)
	   y = 0;
	return convert(x,y, width);
}

/* Converts a 1D index to a 2D point
 *
 * @param index is the index in the 1D array
 * @param width is the number of columns
 * @param height is the number of rows
 * @returns nothing
 */
void convert_1d_to_2d(ThreadArgs *arg, int index, int width){
	int row = index / (width);
	int col = index % width;
	arg->row = row;
	arg->col = col;
}

/* Neighbors method counts the value of the neighbors
 * surrounding an inputed coordinate pair
 *
 * @param *board: board that is being checked
 * @param x: x value being checked
 * @param y: y value being checked
 * @param width: width of board
 * @param height: height of board
 * @return: number of neightbors a point has
*/
int neighbors(int *board, int x, int y, int width, int height) {
	int top_l = board[edges((x-1),(y+1),width, height)];
	int top = board[edges((x),(y+1),width, height)];
	int top_r = board[edges((x+1),(y+1),width, height)];
	int left =  board[edges((x-1),(y),width, height)];
	int right =  board[edges((x+1),(y),width, height)];
	int bot_l =  board[edges((x-1),(y-1),width, height)];
	int bot =  board[edges((x),(y-1),width, height)];
	int bot_r =  board[edges((x+1),(y-1),width, height)];
	return top_l + top + top_r + left + right + bot_l + bot + bot_r;
}

/* Simulates behavior of board, steps through iterations
 *
 * @param *board: board used
 * @param height: height of board
 * @param width: width of board
 * @param iterations: total number of planned iterations
 * @param verbose: whether verbose mode is enabled
 * @returns nothing
 */
void* simulate(void *arg){

	ThreadArgs *args = (ThreadArgs*)arg;
	int ret;
	for(int i = 0; i < args->iterations; i++) {
		ret = pthread_barrier_wait(args->barrier); // this one
		if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD)
			printf("Error with wait barrier during iterations\n");
		if(args->verbose) {
			if(ret == PTHREAD_BARRIER_SERIAL_THREAD){
				if(i == (args->iterations)-1) // last iteration
					print(args->board, i, args->width, args->height, 1);
				else
					print(args->board, i+1, args->width, args->height, 0);
			}
		}
		ret = pthread_barrier_wait(args->barrier);
		args->board = update(args);
	}
	return NULL;
}

/* prints out the board
 *
 * @param *board: board array that we are using
 * @param steps: number of iterations completed
 * @param width: width of board
 * @param HEIGTH: height of board
 */
void print(int *board, int steps, int width, int height, int last) {

	system("clear");
	printf("\n Time Steps: %d\n", (steps+1));
	for(int i = 0; i <height*width; i ++){
			if(i%width == 0)
				printf(" \n");
			if(board[i] == 1)
				printf("@");
			else
				printf("-");
			if(i == width*height-1)
				printf(" \n");
	}
	usleep(100000);
	if(!last)
		system("clear");
}

/* updates the board for an iteration by determining which
 * points are live and dead
 *
 * @param *board: board operating with
 * @param width: width of board
 * @param height: height of board
 * @return: returns new updated array
 */
int *update(ThreadArgs *args){
	int *copy = malloc(args->width*args->height*sizeof(int));
	//copies elements in original board to copy board
	for(int i = 0; i < args->width*args->height; i++) {
		copy[i] = args->board[i];
	}
	int sum = 0;
	int ret = pthread_barrier_wait(args->barrier);
	if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD)
		printf("Error with wait barrier in update function, tid: %d\n", args->tid);
	// checks the neighbors of the indeces from specified start to specified end
	for(int i = args->start_index; i <= args->end_index; i++){
		convert_1d_to_2d(args, i, args->width);
		sum = neighbors(copy, args->col, args->row, args->width, args->height);
		// updates the board accordingly
		if(copy[i] == 0) {
			if(sum == 3) {
				args->board[i] = 1;
			}
		}
		else {
			if(sum <= 1 || sum >= 4) {
				args->board[i] = 0;
			}
		}
	}
	free(copy);
	return args->board;
}
