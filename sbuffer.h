#ifndef _SBUFFER_H_
#define _SBUFFER_H_

#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#define SBUFFER_FAILURE -1
#define SBUFFER_SUCCESS 0
#define SBUFFER_NO_DATA 1


#ifdef DEBUG
	#define DEBUG_PRINTF(...) 									         \
		do {											         \
			fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	 \
			fprintf(stderr,__VA_ARGS__);								 \
			fflush(stderr);                                                                          \
                } while(0)
#else
	#define DEBUG_PRINTF(...) (void)0
#endif

                

typedef struct sbuffer sbuffer_t;

/*
 * All data that can be stored in the sbuffer should be encapsulated in a
 * structure, this structure can then also hold extra info needed for your implementation
 */
typedef struct sbuffer_data sbuffer_data_t;


/*
 * Allocates and initializes a new shared buffer
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
 */
int sbuffer_init(sbuffer_t ** buffer);


/*
 * All allocated resources are freed and cleaned up
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
 */
int sbuffer_free(sbuffer_t ** buffer);


/*
 * Removes the first data in 'buffer' (at the 'head') and returns this data as '*data'  
 * 'data' must point to allocated memory because this functions doesn't allocated memory
 * If 'buffer' is empty, the function doesn't block until new data becomes available but returns SBUFFER_NO_DATA
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
 */
int sbuffer_remove(sbuffer_t * buffer, sensor_data_t * data);


/* Inserts the data in 'data' at the end of 'buffer' (at the 'tail')
 * Returns SBUFFER_SUCCESS on success and SBUFFER_FAILURE if an error occured
*/
int sbuffer_insert(sbuffer_t * buffer, sensor_data_t * data);

/*
 * send the log message to the fifo 
 * used by the datamgr.c connmgr.c and sensor_db.c 
 * implementation is in the main file
 */
void write_to_fifo(char *logmsg);

/*destroy mutex and condition variables*/
void sbuffer_destroy();

void exit_thread();
void notify_thread();

#endif  //_SBUFFER_H_

