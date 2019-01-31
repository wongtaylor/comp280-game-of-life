/**
 * File: gol.c
 * Name: Taylor Wong
 * Project 6: Game of Life Program
 * Description: A program that plays Conway's Game of Life where each time step
 * simulates a round in which entities in the world either live, die, or are
 * born based on surrounding neighbors.
 */

#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <getopt.h>
#define MSGSIZE 32

// forward declarations
int *initBoard(char *filename, int *rows, int *columns, int *iterations, int *coord_pairs);
int translate_to_1d(int row, int col, int num_rows, int num_cols);
void simulateBoard(int *board, int rows, int columns, int iterations, int verbose_mode);
int *updateBoard(int *board, int rows, int cols);
void printBoard(int *board, int rows, int columns, int iteration, int last);
static void timeval_subtract(struct timeval *result, struct timeval *end, struct timeval *start);
int getNeighbors(int *board, int row, int col, int num_row, int num_cols);
int getSum(int *neighbors);
int open_clientfd(char *hostname, char *port);
void send_receive(const void *msg, void *buf);
void writeFile(char *buf);

int main(int argc, char *argv[]) {

	// Step 1: Parse command line args (I recommend using getopt again).
	int c = -1;
	int verbose_mode = 0;
	char *config_filename = NULL;
	bool case_c = false;
	bool case_n = false;
	bool case_v = false;
	bool case_l = false;
	char msg[MSGSIZE];
	char *buf = calloc(1000, sizeof(char));
	int done = 0;

	while((c = getopt(argc, argv, "ln:c:v")) != -1){
		switch(c){
			case 'c':
				// specify configuration filename
				config_filename = optarg;
				case_c = true;
				break;
			case 'v':
				case_v = true;
				// enables verbose mode
				verbose_mode = 1;
				break;
			case 'l':
				case_l = true;
				snprintf(msg, MSGSIZE, "list");
				break;
			case 'n':
				case_n = true;
				config_filename = "config.txt";
				snprintf(msg, MSGSIZE, "get %s", optarg);
				break;
			default:
				//usage(argv[0]);
				exit(1);
		}
	}
	// check which options were specified by user and if they are valid
	if(case_n && case_c){
		printf("ERROR: Cannot specify both the -n and -c options \n");
		exit(1);
	}
	if(case_l && (case_v || case_c || case_n)){
		printf("Error");
		exit(1);
	}
	if(case_l){
		send_receive(msg, buf);
		printf("%s", buf);
		done = 1;
	}
	if(case_n){
		send_receive(msg, buf);
		writeFile(buf);
	}
	if(done){    // frees buf before exiting
		free(buf);
		exit(0);
	}
	// Step 2: Read in the configuration file and use it to initialize your game board
	int rows = 0;
	int columns = 0;
	int iter = 0;
	int pairs = 0;
	// reads file and initializes the board
	int *game_board = NULL;
	game_board = initBoard(config_filename, &rows, &columns, &iter, &pairs);

	if(game_board == NULL){
		printf("Error initializing the board \n");
		exit(1);
	}
	// Step 3: Start your timer
	struct timeval start_time;
	gettimeofday(&start_time, NULL);

	// Step 4: Simulate for the required number of steps.
	simulateBoard(game_board, rows, columns, iter, verbose_mode);

	// Step 5: Stop your timer, calculate amount of time simulation ran for and then print that out.
	struct timeval end_time;
	gettimeofday(&end_time, NULL);
	struct timeval time;
   	timeval_subtract(&time, &end_time, &start_time);
	printf("\n Total time for %d iterations of a %dx%d world is %ld.%06ld seconds \n", iter, rows, columns, time.tv_sec, time.tv_usec);

	// free allocated memory
	free(game_board);
	free(buf);
	return 0;

}

/* Reads the file specified and saves the information in appropriate variables
 * while filling in the board with the correct data.  The coordinates read in
 * from the file indicate a live person and function changes the values to 1.
 *
 * @param filename is the name of the file to be read
 * @param rows is the length of the board array
 * @param columns is the length of the column array for each row array value
 * @param iterations is the number of time steps
 * @param coord_pairs is the number of coordinate pairs that lead to a
 * person's value being set to 1 (number of "live" spots)
 * @returns the initialized board
 */
int *initBoard(char *filename, int *rows, int *columns, int *iterations, int *coord_pairs){
	FILE *ifp;
	char *mode = "r";
	ifp = fopen(filename, mode);
	if(ifp == NULL){
		printf("cannot open %s \n", filename);
		exit(1);
	}

	// save first 4 lines of information separately
	fscanf(ifp, "%d \n", rows);
	fscanf(ifp, "%d \n", columns);
	fscanf(ifp, "%d \n", iterations);
	fscanf(ifp, "%d \n", coord_pairs);

	printf("\n %d, %d, %d, %d", *rows, *columns, *iterations, *coord_pairs);
	// creating 1D array of people that emulates a 2D array
	int *board = calloc((*rows)*(*columns), sizeof(int));
	int index = 0;
	int i = 0, j = 0;
	// loop through to get x and y coordinates
	while(fscanf(ifp, "%d %d \n ", &j, &i) == 2){
		// getneighbors(board, neighbors, i, j, *rows, *columns);
		index = translate_to_1d(i, j, *rows, *columns);
		board[index] = 1;
	}
	fclose(ifp);
	return board;
}

/* Translates a 2D set of indexes and determine the index in the
 * 1D array where it would be located.
 *
 * @param row is the row index of the person
 * @param col is the column index of the person
 * @param num_rows is the number of rows in the world
 * @param num_cols is the number of columns in the world
 * @return the index in the 1D array where the arr[row][col] would
 * be located
 */
int translate_to_1d(int row, int col, int num_rows, int num_cols){
	if(row < 0)
		row = num_rows + row;
	if(row >= num_rows)
		row = row % num_rows;
	if(col < 0)
		col = num_cols + col;
	if(col >= num_cols)
		col = col % num_cols;
	return (row*num_cols) + col;
}

/* Simulates a game of life board that updates the people to living or dying
 * based on the surrounding neighbors.
 *
 * @param board is the simulated world
 * @param neighbors is the 8 neighbors around the specific person at (x,y)
 * @param rows is the row dimensions of the world
 * @param columns is the column dimensions of the world
 * @param iterations is the number of time steps
 * @param verbose_mode indicates if the board should be printed out (1) or not
 */
void simulateBoard(int *board, int rows, int columns, int iterations, int verbose_mode){
	for(int i = 0; i < iterations; i++){
		if(verbose_mode){
			if(i == (iterations - 1)) // last iteration
				printBoard(board, rows, columns, iterations, 1);
			else
				printBoard(board, rows, columns, i+1, 0);
		}
		board =	updateBoard(board, rows, columns);
	}
}

/* Updates the board based on the surrounding neighbor's states.
 *
 * @param board is the simulated world
 * @param rows is the number of rows in the world
 * @param cols is the number of columns in the world
 * @returns the updated (newest version) of the world after one iteration
 */
int *updateBoard(int *board, int rows, int cols){
	int *cp_board = malloc(rows*cols*sizeof(int));
	for(int i = 0; i < rows*cols; i++){
		cp_board[i] = board[i];
	}
	if(cp_board == NULL)
		printf("Error copying game board \n");

	int sum = 0;
	for(int i = 0; i < rows; i++){
		for(int j = 0; j < cols; j++){
			sum = getNeighbors(cp_board, i, j, rows, cols);
			if(sum == 10){
				printf("Error correctly getting the sum of the neighbors");
				exit(1);
			}
			int index = translate_to_1d(i, j, rows, cols);
			if(cp_board[index] == 0){ // dead
				if(sum == 3)
					board[index] = 1;
			}
			else{  // alive
				if(sum <= 1 || sum >= 4)
					board[index] = 0;
			}
		}
	}
	free(cp_board);
	return board;
}

/* Method that adds the values of the neighbor
 * array and returns the sum.
 *
 * @param neighbors is the array of neighbors surrounding
 * the person in the world
 * @returns the sum of neighbors array
 */
int getSum(int *neighbors){
	int sum = 0;
	for(int i = 0; i < 8; i++){
		sum += neighbors[i];
	}
	return sum;
}

/* Prints the board (the world) out indicating "@" for a
 * living person and "." for a dead person.
 *
 * @param board is the simulated world
 * @param rows is the row dimension of the world
 * @param column is the column dimension of the world
 */
void printBoard(int *board, int rows, int columns, int iteration, int last){
	printf("\n Time Steps: %d \n", iteration);
	for(int i = 0; i < rows*columns; i++){
		if(i%columns == 0)
			printf(" \n");
		if(board[i] == 1)
			printf("@ ");
		else
			printf("- ");
		if(i == rows*columns-1)
			printf("\n");
	}
	usleep(150000);
	if(!last)
		system("clear");
}

/* Method that calculates the time difference from start and end
 *
 * @param *result is a pointer to the value of the difference
 * @param *end is a pointer to the value of the end time
 * @param *start is a pointer to the value of the start time
 */
static void timeval_subtract(struct timeval *result, struct timeval *end, struct timeval *start){
	// perform the carry for the later subtraction by updating start
	if(end->tv_usec < start->tv_usec){
		int nsec = (start->tv_usec - end->tv_usec) / 1000000 + 1;
		start->tv_usec -= 1000000 *nsec;
		start->tv_sec += nsec;
	}
	if(end->tv_usec - start->tv_usec > 1000000){
		int nsec = (end->tv_usec - start->tv_usec) / 1000000;
		start->tv_usec += 1000000 *nsec;
		start->tv_sec -= nsec;
	}
	// compute the time remaining to wait.tv_usec is certainly positive
	result->tv_sec = end->tv_sec - start->tv_sec;
	result->tv_usec = end->tv_usec - start->tv_usec;
}

/* Method that finds the neighbors around a specified person and
 * then calls getSum to get the sum of the neighbors.
 *
 * @param *board is the simulated game_board of the world
 * @param row is the row index of the specified person
 * @param col is the column index of the specified person
 * @param num_rows is the number of rows in the world
 * @param num_cols is the number of columns in the world
 * @returns the sum of the neighbors surrounding the person
 */
int getNeighbors(int *board, int row, int col, int num_rows, int num_cols){
	int *neighbors = malloc(8*sizeof(int));
	if(neighbors == NULL){
		printf("Error creating neighbors array \n");
		exit(1);
	}
	// left
	neighbors[0] = board[translate_to_1d(row, col-1, num_rows, num_cols)];
	// upper left diagonal
	neighbors[1] = board[translate_to_1d(row-1, col-1, num_rows, num_cols)];
	// above
	neighbors[2] = board[translate_to_1d(row-1, col, num_rows, num_cols)];
	// upper right diagonal
	neighbors[3] = board[translate_to_1d(row-1, col+1, num_rows, num_cols)];
	// right
	neighbors[4] = board[translate_to_1d(row, col+1, num_rows, num_cols)];
	// lower right diagonal
	neighbors[5] = board[translate_to_1d(row+1, col+1, num_rows, num_cols)];
	// below
	neighbors[6] = board[translate_to_1d(row+1, col, num_rows, num_cols)];
	// lower left diagonal
	neighbors[7] = board[translate_to_1d(row+1, col-1, num_rows, num_cols)];

	// find the sum of the neighbors around the specified person
	int sum = 10;
	sum = getSum(neighbors);
	free(neighbors);

	return sum;
}

/* Method helps open a connection with a server running on
 * hostname and listens for connection requests on port
 *
 * @param *hostname is the name of the host
 * @param *port is a 16-bit integer that identifies a process
 * @returns an open socket descriptor
 */
int open_clientfd(char *hostname, char *port){
	int clientfd;
	struct addrinfo hints, *listp, *p;

	// Get list of potential server addresses
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM; // open a connection
	hints.ai_flags = AI_NUMERICSERV; // using numeric port arg
	hints.ai_flags |= AI_ADDRCONFIG; // recommended for connections
	getaddrinfo(hostname, port, &hints, &listp);

	// walk the list for one that we can successfully connect to
	for(p = listp; p; p = p->ai_next){
		// create a socket descriptor
		if((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue; // socket failed, try the next

		// conext to the server
		if(connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
			break; // success
		close(clientfd); // connect failed, try annother
	}

	// clean up
	freeaddrinfo(listp);
	if(!p) // all connects failed
		return -1;
	else // the last connect succeeded
		return clientfd;
}

/* Send_receive function connects to the remote server
 *
 * @param msg is the message sent specified by the user,
 * that either says get a file or list the files on the server
 * @param buf is the message received that contains
 * the contents of the file specified or the files on the server
 * @returns nothing
 */
void send_receive(const void *msg, void *buf){
	int clientfd = open_clientfd("comp280.sandiego.edu", "9181");
	if(clientfd == -1){
		printf("Connection failed");
		exit(1);
	}
	int len = strlen(msg);
	int bytes_sent = send(clientfd, msg, len, 0);
	int bytes_received = recv(clientfd, buf, 1000, 0);
	if(bytes_sent == -1 || bytes_received == -1){
		printf("Something went wrong with sending and receiving the message! %s \n", strerror(errno));
		exit(1);
	}
}

/* writeFile method opens config.txt and writes buf contents to it
 *
 * @param buf is the data and contents of the files on the server
 * that is written to a file, config.txt
 * @returns nothing
 */
void writeFile(char *buf){
	FILE *outfile = fopen("config.txt", "w+");
	if(outfile == NULL){
		printf("error opening file - writeFile function \n");
		exit(1);
	}
	fprintf(outfile, "%s", buf); // write to file
	fflush(outfile);
	fclose(outfile);
}
