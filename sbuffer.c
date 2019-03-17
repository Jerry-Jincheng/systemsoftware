#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include "sbuffer.h"
#include <signal.h>
#include <unistd.h>

/**********************************************************************************
 *NOTE: DEBUG_PRINTF() IS defined in sbuffer.h for debugging purpose, 
 *      the content will be printed out only when compiled with -DDEBUG
 **********************************************************************************/

pthread_mutex_t data_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond =PTHREAD_COND_INITIALIZER; 
pthread_barrier_t sync_thread;


/*
 * All data that can be stored in the sbuffer should be encapsulated in a
 * structure, this structure can then also hold extra info needed for your implementation
 */
struct sbuffer_data {
    sensor_data_t data;
};

typedef struct sbuffer_node {
  struct sbuffer_node * next;
  sbuffer_data_t element;
  int read_flag;            /*default value is 0, when one thread read, it is set  to be one*/
} sbuffer_node_t;

struct sbuffer {
  sbuffer_node_t * head;
  sbuffer_node_t * tail;
};	


int sbuffer_init(sbuffer_t ** buffer)
{
  pthread_barrier_init( &sync_thread, NULL, 2);
  pthread_mutex_init(&data_mutex,NULL);
  *buffer = malloc(sizeof(sbuffer_t));
  if (*buffer == NULL) return SBUFFER_FAILURE;
  (*buffer)->head = NULL;
  (*buffer)->tail = NULL;
  return SBUFFER_SUCCESS; 
}


int sbuffer_free(sbuffer_t ** buffer)
{
  sbuffer_node_t * dummy;
  if ((buffer==NULL) || (*buffer==NULL)) 
  {
    return SBUFFER_FAILURE;
  } 
  while ( (*buffer)->head )
  {
    dummy = (*buffer)->head;
    (*buffer)->head = (*buffer)->head->next;
    free(dummy);
  }
  free(*buffer);
  *buffer = NULL;
  return SBUFFER_SUCCESS;		
}


int sbuffer_remove(sbuffer_t * buffer,sensor_data_t * data)
{
      pthread_barrier_wait( &sync_thread );
  int rc=pthread_mutex_lock(&data_mutex);
  DEBUG_PRINTF("error_code in pthread_mutex_lock :%d",rc);
  sbuffer_node_t * dummy;
  if (buffer == NULL) 
  {
      pthread_mutex_unlock(&data_mutex);
      return SBUFFER_FAILURE;
  }
  if (buffer->head == NULL) 
  {
      DEBUG_PRINTF("no data, waiting for new\n");
      pthread_cond_wait(&cond,&data_mutex);
  }
 
  if(buffer->head!=NULL)// safe caution when connmgr exiting 
  {
  *data = buffer->head->element.data;
  dummy = buffer->head;
  /*this data has no been read before  DO NOT delete and set the read_flag*/
  if(dummy->read_flag==0)
  {
      dummy->read_flag=1;
  }
  else
  {
    if (buffer->head == buffer->tail) // buffer has only one node
    {
        buffer->head = buffer->tail = NULL; 
    }
    else  // buffer has many nodes empty
    {
    buffer->head = buffer->head->next;
    }
    free(dummy);
    DEBUG_PRINTF("remove data from sbuffer: id= %"PRIu16" value= %f",data->id,data->value);
    pthread_mutex_unlock(&data_mutex);
     return SBUFFER_SUCCESS;
  }
 }
  pthread_mutex_unlock(&data_mutex);
   return SBUFFER_NO_DATA;
}


int sbuffer_insert(sbuffer_t * buffer, sensor_data_t * data)
{
  pthread_mutex_lock(&data_mutex);
  DEBUG_PRINTF("Inserting data into sbuffer:id=%" PRIu16" value= %f",data->id,data->value);
  sbuffer_node_t * dummy;
  if (buffer == NULL) return SBUFFER_FAILURE;
  dummy = malloc(sizeof(sbuffer_node_t));
  if (dummy == NULL) 
  {
      pthread_mutex_unlock(&data_mutex);
      return SBUFFER_FAILURE;
  }
  dummy->element.data = *data;
  dummy->read_flag=0;
  dummy->next = NULL;
  if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
  {
    buffer->head = buffer->tail = dummy;
  } 
  else // buffer not empty
  {
    buffer->tail->next = dummy;
    buffer->tail = buffer->tail->next; 
  }
  pthread_mutex_unlock(&data_mutex);
  pthread_cond_broadcast(&cond);
  DEBUG_PRINTF("start to signal\n");
  return SBUFFER_SUCCESS;
}

void notify_thread()
{
    pthread_cond_signal(&cond);
}
void sbuffer_destroy()
{
    pthread_cond_broadcast(&cond);
    pthread_mutex_destroy( &data_mutex );
    pthread_cond_destroy( &cond );
    
}

