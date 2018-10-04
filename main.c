/* Sensor Monitoring System
 * System Software
 * Author: Michiel Schrauwen
 * 
 * Sources used: 
 * https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/
 * http://beej.us/net2/bgnet.html
 * https://www.geeksforgeeks.org/c-program-bubble-sort-linked-list/
 * https://www.tutorialspoint.com/learn_c_by_examples/average_of_array_in_c.htm
 * https://airtower.wordpress.com/2010/06/16/catch-sigterm-exit-gracefully/
 * https://computing.llnl.gov/tutorials/pthreads/
 * */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "config.h"
#include "sbuffer.h"
#include "datamgr.h"
#include "connmgr.h"
#include "sensor_db.h"

//GLOBAL VARIABLES
pthread_mutex_t mutex_buf, mutex_fifo;	//external
pthread_t connmgr_td, datamgr_td, storagemgr_td;	//all external except connmgr_td
bool run_datamgr, run_storagemgr;	//external
volatile sig_atomic_t run_logprocess;

//FUNCTION DECLARATIONS
void * connmgr_function(void * connmgr_td_data);
void * datamgr_function(void * buffer);
void * storagemgr_function(void * buffer);
void fifo_to_log();
void write_to_fifo(char * info_message);
void end_log_process();

struct connmgr_data	{
	sbuffer_t * buf;
	int port;
};

int main(int argc, char * argv[])
{
	PARENT_DEBUG_PRINTF("------Main function starts now ------");
	
	if(argc < 1)	{
		printf("Need to add port number for connection - example: ./main.out 1234\n");
		exit(EXIT_FAILURE);
	}
	
	pid_t child_pid;
	
	run_logprocess = 1;
	
	child_pid = fork();
	SYSCALL_ERROR(child_pid);
	
	if(child_pid == 0)	//child process
	{
		CHILD_DEBUG_PRINTF("--CHILD PROCESS START--");
		fflush(stdout);
		
		/* child process knows from parent when to end by catching SIGTERM
		/ source: https://airtower.wordpress.com/2010/06/16/catch-sigterm-exit-gracefully/
		*/
		struct sigaction action;
		memset(&action, 0, sizeof(struct sigaction));
		action.sa_handler = end_log_process;
		SIGACTION_ERROR(sigaction(SIGTERM, &action, NULL));
		
		
		fifo_to_log();		
		CHILD_DEBUG_PRINTF("--CHILD PROCESS END--");
		exit(EXIT_SUCCESS);
	}
	else
	{	//parent process
		sbuffer_t * buffer = NULL;
		PARENT_DEBUG_PRINTF("--PARENT PROCESS START--");
		fflush(stdout);
		
		SBUFFER_ERROR(sbuffer_init(&buffer));
		
		pthread_attr_t attr;
		
		struct connmgr_data connmgr_td_data;	//data struct to pass to connmgr thread
		connmgr_td_data.buf = buffer;
		connmgr_td_data.port = atoi(argv[1]);
		PARENT_DEBUG_PRINTF("Port for connmgr thread is %d", connmgr_td_data.port);
		
		//* Explicitly create threads in a joinable state */
		PTHREAD_FUNCTION_ERROR(pthread_attr_init(&attr));
		PTHREAD_FUNCTION_ERROR(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));
		
		//* Initialize mutex for sbuffer and for FIFO */
		PTHREAD_FUNCTION_ERROR(pthread_mutex_init(&mutex_buf, NULL));
		PTHREAD_FUNCTION_ERROR(pthread_mutex_init(&mutex_fifo, NULL));
		
		PARENT_DEBUG_PRINTF("Creating connmgr-,datamgr-,storagemgr- thread");
		PTHREAD_FUNCTION_ERROR(pthread_create(&connmgr_td, &attr, connmgr_function, &connmgr_td_data));
		
		run_datamgr = true;
		run_storagemgr = true;
		
		PTHREAD_FUNCTION_ERROR(pthread_create(&datamgr_td, &attr, datamgr_function, buffer));
		
		PTHREAD_FUNCTION_ERROR(pthread_create(&storagemgr_td, &attr, storagemgr_function, buffer));
		
		PARENT_DEBUG_PRINTF("Waiting for connmgr thread to join");
		PTHREAD_FUNCTION_ERROR(pthread_join(connmgr_td, NULL));	//waiting for connection manager to join after timeout
		
		run_datamgr = false;	//ending datamgr and storagemgr then wait for them to join
		run_storagemgr = false;
		PTHREAD_FUNCTION_ERROR(pthread_join(datamgr_td, NULL));
		PTHREAD_FUNCTION_ERROR(pthread_join(storagemgr_td, NULL));
		PARENT_DEBUG_PRINTF("Waiting for datamgr-,storagemgr- thread to join");
		
		kill(child_pid, SIGTERM);	//sending signal SIGTERM to terminate the child-fifo process
		PARENT_DEBUG_PRINTF("Signal SIGTERM send to terminate child process");
		
		PTHREAD_FUNCTION_ERROR(pthread_attr_destroy(&attr));	//destroying attribute and mutex of the threads since they are finished
		PTHREAD_FUNCTION_ERROR(pthread_mutex_destroy(&mutex_buf));
		
		//~ debug statement that prints content of buffer after all threads ended
		sbuffer_print_data(buffer);
		
		PARENT_DEBUG_PRINTF("--PARENT PROCESS END--");
		SBUFFER_ERROR(sbuffer_free(&buffer));
	}
	
	
	PARENT_DEBUG_PRINTF("--MAIN FINISHED SUCCES--");
	exit(EXIT_SUCCESS);
}

void * connmgr_function(void * connmgr_td_data)	{
	
	struct connmgr_data * td_data = (struct connmgr_data*) connmgr_td_data;
	
	sbuffer_t * td_buffer = td_data -> buf;
	int td_port = td_data -> port;
	
	PARENT_DEBUG_PRINTF("connmgr_listen is called now with port: %d", td_port);
	
	connmgr_listen(td_port, &td_buffer);
	
	PARENT_DEBUG_PRINTF("connmgr thread will exit now");	
	pthread_exit(NULL);
	
}

void * datamgr_function(void * buffer)	{
	
	sbuffer_t * td_buffer = (sbuffer_t*) buffer;
	
	FILE * room_sensor_fp = fopen("room_sensor.map", "r");
	FILE_OPEN_ERROR(room_sensor_fp);
	
	PARENT_DEBUG_PRINTF("datamgr_parse_sensor_data is called now");
	
	datamgr_parse_sensor_data(room_sensor_fp, &td_buffer);
	
	datamgr_free();
	
	FILE_CLOSE_ERROR(fclose(room_sensor_fp));
	
	PARENT_DEBUG_PRINTF("datamgr thread will exit now");
	pthread_exit(NULL);
}

void * storagemgr_function(void * buffer)	{
	
	sbuffer_t * td_buffer = (sbuffer_t*) buffer;
	
	DBCONN * connection = NULL;
	
	PARENT_DEBUG_PRINTF("Initializing connection to database");
	connection = init_connection(1);
	
	int attempt_nr = 1;	
	while(connection == NULL && attempt_nr <3)	{
		PARENT_DEBUG_PRINTF("Initializing database failed, waiting 2 seconds to retry initialization");
		sleep(2);
		connection = init_connection(1);
		attempt_nr++;
	}
	if(connection == NULL)	{
		PARENT_DEBUG_PRINTF("Initializing database failed, shutting down gateway");
		pthread_exit(NULL);
	}
	
	PARENT_DEBUG_PRINTF("storagemgr_parse_sensor_data is called now");
	storagemgr_parse_sensor_data(connection, td_buffer);
	
	PARENT_DEBUG_PRINTF("Disconnecting from database");	
	disconnect(connection);
	
	PARENT_DEBUG_PRINTF("storagemgr thread will exit now");
	pthread_exit(NULL);
	
}

void fifo_to_log()	{
	CHILD_DEBUG_PRINTF("fifo_to_log started");
	FILE * fifo_pointer;
	FILE * log_pointer;
	int result;
	char *str_result;
	char *info_message;
	char recv_buf[MAX];
	int sequence_number = 0;
	
	// Create the FIFO if it does not exist
	CHECK_MKFIFO(mkfifo(FIFO_NAME, 0666));
	
	fifo_pointer = fopen(FIFO_NAME, "r");
	FILE_OPEN_ERROR(fifo_pointer);
	
	log_pointer = fopen(LOG_NAME, "w");
	FILE_OPEN_ERROR(log_pointer);
	
	do
	{
		do
		{
			info_message = fgets(recv_buf, MAX, fifo_pointer);
			if(info_message != NULL)
			{				
				sequence_number++;
				CHILD_DEBUG_PRINTF("Message read from fifo: \n%s", info_message);
				
				result = asprintf(&str_result, "%d %s", sequence_number, info_message);
				LOG_ASPRINTF_ERROR(result);
				
				CHILD_DEBUG_PRINTF("Message to be written to log: \n%s", str_result);
				result = fputs(str_result, log_pointer);
				WRITE_LOG_ERROR(result);
				
				free(str_result);	//asprintf allocates memory so we need to free this	
			}			
		} while (info_message != NULL);
	} while (run_logprocess == 1);
		
	result = fclose(fifo_pointer);
	FILE_CLOSE_ERROR(result);
	
	result = fclose(log_pointer);
	FILE_CLOSE_ERROR(result);
	
	CHILD_DEBUG_PRINTF("fifo_to_log ended");
}

void end_log_process()	{	//function called when receiving SIGTERM
	CHILD_DEBUG_PRINTF("end_log_process was called - ending log-fifo-child process now");
	run_logprocess = 0;
}

void write_to_fifo(char * info_message)	{
	CHILD_DEBUG_PRINTF("write_to_fifo started");
	FILE * fifo_pointer;
	int result;
	
	// Create the FIFO if it does not exist
	result = mkfifo(FIFO_NAME, 0666);
	CHECK_MKFIFO(result); 	
	
	fifo_pointer = fopen(FIFO_NAME, "w");
	FILE_OPEN_ERROR(fifo_pointer);
	
	if(fputs(info_message,fifo_pointer) == EOF)
	{
		CHILD_DEBUG_PRINTF("Oops, something went wrong writing info message to FIFO");
		exit(EXIT_FAILURE);
	}
	
	CHILD_DEBUG_PRINTF("This has been written to fifo: %s\n", info_message);
	fflush(fifo_pointer);	
	
	result = fclose(fifo_pointer);
	FILE_CLOSE_ERROR(result);
	CHILD_DEBUG_PRINTF("write_to_fifo ended");
	
}
	
