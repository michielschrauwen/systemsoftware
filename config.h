#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>
#include <time.h>
#include <errno.h>

typedef uint16_t sensor_id_t;
typedef double sensor_value_t;     
typedef time_t sensor_ts_t;         // UTC timestamp as returned by time() - notice that the size of time_t is different on 32/64 bit machine

typedef struct {
	sensor_id_t id;
	sensor_value_t value;
	sensor_ts_t ts;
} sensor_data_t;

typedef int bool;
#define true 1
#define false 0

#define FIFO_NAME "logFifo"
#define LOG_NAME "gateway.log"
#define MAX 200

//* GLOBAL VARIABLES */
extern pthread_mutex_t mutex_buf, mutex_fifo;

extern bool run_datamgr, run_storagemgr;

extern pthread_t datamgr_td, storagemgr_td;

//* GLOBAL FUNCTION DECLARATIONS */
extern void write_to_fifo(char * info_message);

//*	DEBUG MODE */
#ifdef DEBUG_PARENT
	#define PARENT_DEBUG_PRINTF(...) 									         \
		do {											         \
			fprintf(stderr,"In %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	 \
			fprintf(stderr,__VA_ARGS__);								 \
			fprintf(stderr,"\n");					\
			fflush(stderr);                                                                          \
                } while(0)
#else
	#define PARENT_DEBUG_PRINTF(...) (void)0
#endif

#ifdef DEBUG_CHILD
	#define CHILD_DEBUG_PRINTF(...) 									         \
		do {											         \
			fprintf(stdout,"In %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	 \
			fprintf(stdout,__VA_ARGS__);								 \
			fprintf(stdout,"\n");					\
			fflush(stdout);                                                                          \
                } while(0)
#else
	#define CHILD_DEBUG_PRINTF(...) (void)0
#endif

//* ERROR HANDLING */

#define FILE_OPEN_ERROR(fp) 								\
		do {												\
			if ( (fp) == NULL )								\
			{												\
				perror("File open failed");					\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)

#define SBUFFER_INSERT_ERROR(err) 							\
		do {												\
			if ( (err) != 0 )								\
			{												\
				perror("Sbuffer insert failed");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define SBUFFER_REMOVE_ERROR(err) 							\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("Sbuffer remove failed");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define SBUFFER_ERROR(err) 							\
		do {												\
			if ( (err) != 0 )								\
			{												\
				perror("Sbuffer init/free/.. failed");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define PTHREAD_FUNCTION_ERROR(err) 						\
		do {												\
			if ( (err) != 0 )								\
			{												\
				perror("Pthread function failed");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define SYSCALL_ERROR(err) 									\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("Error executing syscall");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define CHECK_MKFIFO(err) 									\
		do {												\
			if ( (err) == -1 )								\
			{												\
				if ( errno != EEXIST )						\
				{											\
					perror("Error executing mkfifo");		\
					exit( EXIT_FAILURE );					\
				}											\
			}												\
		} while(0)

#define WRITE_LOG_ERROR(err) 									\
		do {												\
			if ( (err) == EOF )								\
			{												\
				perror("Error writing to log file");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)
		
#define LOG_ASPRINTF_ERROR(err) 									\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("Error asprintf when writing to log file");			\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)

#define FILE_CLOSE_ERROR(err) 								\
		do {												\
			if ( (err) == -1 )								\
			{												\
				perror("File close failed");				\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)

#define SIGACTION_ERROR(err) 								\
		do {												\
			if ( (err) != 0 )								\
			{												\
				perror("Sigaction - SIGTERM failed");				\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)

#define MALLOC_ERROR(err) 								\
		do {												\
			if ( (err) == NULL )								\
			{												\
				perror("MALLOC - memory allocation failed");				\
				exit( EXIT_FAILURE );						\
			}												\
		} while(0)



#endif /* _CONFIG_H_ */

