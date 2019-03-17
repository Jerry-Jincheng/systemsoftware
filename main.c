

/********************************************************************************************************************
 *                                  final assignment for system software
 *                                    created by Jincheng Ma R0650257
 *********************************************************************************************************************/


/********************************************************************************************************************
 *                                summary of final assignment
 * 
 * 1. one sbuffer for three threads: thread_connmgr, thread_sensor_db,thread_datamgr
 * 2. two mutex, one for operation of the sbuffer, declared and used in sbuffer.c.  The other for the writing operation
 *    to the fifo and exiting the threads
 * 3. one barrier and condition variable together with previous mutex are used in sbuffer to ensure right operations on 
 *    sbuffer
 * 4. one global variable act as condition for threads of datamgr and sensor_db
 * 5. send signal to child process to wake up and read log Message when some activity is needed to be logged, signal 
 *    handler is used to handle the signal 
 * 6. when the program ends,connmgr thread exit first,set thread_end global flag and then two other threads exit
 *    SIGKILL signal is sent via kill() to log process, destroys all the mutex, condition variable and barrier
 *    clean up the memory and exit program
 * 
 *********************************************************************************************************************/

#define _GNU_SOURCE
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include "lib/dplist.h"
#include "lib/tcpsock.h"
#include "connmgr.h"
#include "sensor_db.h"
#include "datamgr.h"
#include "sbuffer.h"
#include "errmacros.h"

#define FIFO_NAME "logFifo"             
#define EXIT_FAILURE_FORK  -1
#define MAX_SIZE      200
#define LOG_FILE_NAME  "gateway.log"
#define DEFAULT_PORT    1234        /*if no argument about port number is specified in terminal,this port number will be used by default*/
#define INIT_FAILURE    -1          /*failed to init_connection to database*/

#define ERR_PRINTF(...) 									         \
		do {											         \
			fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	 \
			fprintf(stderr,__VA_ARGS__);								 \
			fflush(stderr);                                                                          \
                } while(0)

#define PTHREAD_ERR_HANDLER(condition,err_code)\
	do {						            \
            if ((condition)) ERR_PRINTF(#condition"\t"#err_code " failed\n");    \
        } while(0)
/*********************************************************************************************************
 *NOTE: DEBUG_PRINTF() defined in sbuffer.h for debugging purpose, 
 *      the content will be printed out only when compiled with -DDEBUG
 ********************************************************************************************************/

/*********************************************************************************************************
 *                                             global variables                                          *
 *********************************************************************************************************/
    int sequence_number=0;  /*used to track the sequence_number of the log file*/
    sbuffer_t * sbuffer;    /*shared buffer: used to store the sensor data     */
    int result;   
    char *str_result;       /*used to check if log process really read something from logFifo*/
    char recv_buf[MAX_SIZE]; /*buffer used in fifo_reader*/
    char send_buf[MAX_SIZE];/*buffer used in fifo_writer*/
    pid_t child_pid;        /* the track the pid of the child process*/
    int port=DEFAULT_PORT;  /*DEFAULT_PORT if no port was specified from terminal*/
    int data_ready=0;       /*flag set indicating data is already in sbuffer to wake up thread of datamgr, sensor_db*/
    int end_program=0;      /*when timout reaches, this flag set to 1 and program terminates*/
    volatile  pid_t log_pid;          /*pid of log process used to send signal from main process*/
    DBCONN * connection;
    pthread_mutex_t log_mutex=PTHREAD_MUTEX_INITIALIZER;/*used for write to fifo*/
    pthread_mutex_t clean_mutex=PTHREAD_MUTEX_INITIALIZER;
    pthread_t thread_datamgr, thread_sensor_db,thread_connmgr;/*three threads to run on the main process*/
    FILE *fp_log,*fp_fifo_r, *fp_fifo_w;
    int thread_end=0;
    int num_thread_exited=0;

/*********************************************************************************************************
 *                                             functions used                                            *
 *********************************************************************************************************/

/*fifo read and write operations*/
void read_from_fifo();
void write_to_fifo(char *logmsg);

/*signal handler for log process */
void signal_handler(int sig);

/*three functions for threads*/
void * run_connmgr();
void * run_datamgr();
void * run_sensor_db();

/*clean up memory and thread*/
void thread_cleanup();

/*set thread_end flags*/
void exit_thread()
{
    thread_end=1;
    notify_thread();
}

void read_from_fifo()
{
    if(thread_end==0){
    fp_fifo_r = fopen(FIFO_NAME, "r"); 
   
    DEBUG_PRINTF("syncing with writer ok");
    FILE_OPEN_ERROR(fp_fifo_r);
    fp_log=fopen(LOG_FILE_NAME,"a");
  do 
   {
    str_result = fgets(recv_buf, MAX_SIZE, fp_fifo_r); // fgets : return recv_buf on SUCCESS, return NULL On Failure
    if ( str_result != NULL )
    { char *log;
        sequence_number++;
        DEBUG_PRINTF("sequence_number is %d\n",sequence_number);
        asprintf(&log,"%d %s\n", sequence_number,recv_buf); 
        DEBUG_PRINTF("%s\n",recv_buf);
        DEBUG_PRINTF(" %s\n",log);
      if(fp_log!=NULL)
      {
          fwrite(log,strlen(log),1,fp_log);
      }
     if(log!=NULL)free(log);
    
    }
  } while ( str_result != NULL ); 
  fclose(fp_log);
    }
}

/*to be used by connmgr.c sensor_db.c datamgr.c */
void write_to_fifo(char *logmsg)
{
    pthread_mutex_lock(&log_mutex);
    DEBUG_PRINTF("enter write_to_fifo function\n");
    kill(log_pid,SIGUSR1);
    char * send_buf;
    fp_fifo_w = fopen(FIFO_NAME, "w"); 
    asprintf(&send_buf,"%s",logmsg);
    fputs( send_buf, fp_fifo_w );
    FFLUSH_ERROR(fflush(fp_fifo_w));
    DEBUG_PRINTF("Message send: %s", send_buf); 
    if(send_buf!=NULL)free( send_buf );
    fclose(fp_fifo_w);
    pthread_mutex_unlock(&log_mutex);
}


/*To be used by log process */
void signal_handler(int sig)
{
    if(sig==SIGUSR1)
    {
        DEBUG_PRINTF("waking up, start to read from fifo\n");
        read_from_fifo();
    }
}


/*
 * run the thread to manage the connmgr.c code
 * when there is sensor data ready to write
 */
void * run_connmgr( )
{
        DEBUG_PRINTF("this is connmgr thread id= %lu\n",pthread_self());
        connmgr_listen(port,&sbuffer);
        DEBUG_PRINTF("timeout reached,cleaning up and exiting\n ");
        connmgr_free();
        if(num_thread_exited<2)
         {
            DEBUG_PRINTF("waiting for thread to exit\n");
            notify_thread();
        }
        
        return (void *)0;
}


/*
 * run the thread to manage the datamgr.c code
 * when there is sensor data ready to write
 */
void * run_datamgr()
{
    DEBUG_PRINTF("this is datamgr thread id= %lu\n",pthread_self());
    FILE *fp_sensor_map=fopen("room_sensor.map","r");
    PTHREAD_ERR_HANDLER(fp_sensor_map!=NULL,"open room sensor map\n");
    
    while(thread_end==0)
    {
        datamgr_parse_sensor_data(fp_sensor_map,&sbuffer);
       
    }
    fclose(fp_sensor_map);
    pthread_mutex_lock(&log_mutex);
    num_thread_exited++;
    DEBUG_PRINTF("exited from thread_datamgr %d\n",num_thread_exited);
    pthread_mutex_unlock(&log_mutex);
    fflush(stdout);
    return (void*)0;
}
/*
 * run the thread to manage the sensor_db.c code
 * when there is sensor data ready to write
 */
void * run_sensor_db(void *id)
{
        DEBUG_PRINTF("this is sensor_db thread id= %lu\n",pthread_self());
        
        while(thread_end==0)
        {
            storagemgr_parse_sensor_data(connection, &sbuffer);
        }
        pthread_mutex_lock(&log_mutex);
        num_thread_exited++;
        DEBUG_PRINTF("exited from thread_sensor_db %d\n",num_thread_exited);
        pthread_mutex_unlock(&log_mutex);
       
    
    return (void *)0;
}


void  thread_cleanup()
{
     disconnect(connection);                /*close database connection*/
     pthread_detach(thread_sensor_db);
     datamgr_free();                        /*free the list */
     pthread_detach(thread_sensor_db);

     
}

int main(int argc, char *argv[])
{
    if(argc>1)
    {
        port=atoi(argv[1]);
    }
    
    /* Create the FIFO if it does not exist */ 
    result = mkfifo(FIFO_NAME, 0666);
    CHECK_MKFIFO(result); 
    //create process
    child_pid=fork();
    
    if(child_pid<0)
    {
        fprintf(stderr,"error occured when fork()\n");
        exit (EXIT_FAILURE_FORK);
    } else if(child_pid==0) //if fork succeeded and this is child process
    {
       printf("this is child process with pid= %d, waiting for signal to wake up \n",getpid());
       fp_log=fopen(LOG_FILE_NAME,"w");
       if(fp_log!=NULL)printf("log file open\n");
       signal(SIGUSR1,signal_handler);
       while (1)
           sleep(1);
    } else
    {
        log_pid=child_pid;
        DEBUG_PRINTF("this is main process with pid= %d, starts to make threads \n",getpid());
        
//         sleep(1);
//         int num_trial=0;
//         do
//         {
//             num_trial++;
//             connection=init_connection(1);
//         }while(connection==NULL&&num_trial<=3);
//         
//         if(num_trial>3)
//         {
//             kill(log_pid,SIGKILL);
//             return INIT_FAILURE;
//         }
        
         sleep(1);
         sbuffer_init(& sbuffer);
         connection=init_connection(1);
        
	int id_datamgr, id_sensor_db,id_connmgr,presult;
        //pthread create and err_handling
	id_datamgr = 1;	
	presult = pthread_create( &thread_datamgr, NULL, &run_datamgr, &id_datamgr );
	PTHREAD_ERR_HANDLER( presult!=0, "pthread_create");	
	id_sensor_db = 2;
	presult = pthread_create( &thread_sensor_db, NULL, &run_sensor_db, &id_sensor_db );
	PTHREAD_ERR_HANDLER( presult!=0, "pthread_create");
        id_connmgr = 3;	
	presult = pthread_create( &thread_connmgr, NULL, &run_connmgr, &id_connmgr );
	PTHREAD_ERR_HANDLER( presult!=0, "pthread_create");
    
	//pthread_join and err_handling
        presult= pthread_join(thread_datamgr, NULL);
	PTHREAD_ERR_HANDLER( presult!=0, "pthread_join");
	presult= pthread_join(thread_sensor_db, NULL);
	PTHREAD_ERR_HANDLER( presult!=0, "pthread join");
        presult= pthread_join(thread_connmgr, NULL);
	PTHREAD_ERR_HANDLER( presult!=0, "pthread_join");
        
        sleep(2);
        sbuffer_free(&sbuffer);
        sbuffer_destroy();
        if(fp_fifo_r!=NULL)fclose(fp_fifo_r);
        pthread_mutex_destroy( &log_mutex );
        thread_cleanup();
        kill(log_pid,SIGKILL);
        DEBUG_PRINTF("finish clean\n");
        fflush(stdout);
        DEBUG_PRINTF("hello,after thread \n");
    }
    
    return 0;
}


