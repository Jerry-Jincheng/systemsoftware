/*   
 * created by Jincheng Ma 
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <inttypes.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "config.h"
#include "lib/tcpsock.h"
#include "lib/dplist.h"
#include "connmgr.h"
#include "sbuffer.h"

#define MAGIC_COOKIE	(long)(0xA2E1CF37D35)	// used to check if a socket is bounded

/**********************************************************************************
 *NOTE: DEBUG_PRINTF() defined in sbuffer.h for debugging purpose, 
 *      the content will be printed out only when compiled with -DDEBUG
 **********************************************************************************/

struct active_socket
{
    tcpsock_t *socket;
    socket_ts_t ts;
    sensor_id_t sensor_id;
};

dplist_t *list_socket=NULL;
tcpsock_t *server;

int read_from_socket(int client_sd,sensor_data_t *sensor_data);



//callback function for the dplist
void* element_copy_conn(void * src_element)
{
	active_socket_t *copy_element= malloc(sizeof(active_socket_t));
	//check if it malloc successfully
	assert(copy_element != NULL);
	*copy_element = *(active_socket_t *)(src_element);
	return (void *)copy_element;
}

//callback function for the dplist
void element_free_conn(void ** element)
{
    free(*element);
}

//callback function for the dplist
int element_compare_conn(void * x, void * y)
{
    if(((active_socket_t*)x)->socket>((active_socket_t *)y)->socket)
      return 1;
    if(((active_socket_t*)x)->socket==((active_socket_t *)y)->socket)
      return 0;
    else
      return -1;
}

void connmgr_listen(int port_number,sbuffer_t **buffer)
{
    struct timeval timeout;
    
    timeout.tv_sec=TIMEOUT;
    timeout.tv_usec=0;
    int server_sd;
    
    //reference: www:gnu.org/software/lib/manual/html_node/Server-Example.html
    fd_set active_fd_set,read_fd_set;
    
    DEBUG_PRINTF("Test server is started\n");
    if (tcp_passive_open(&server,port_number)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
    list_socket=dpl_create(&element_copy_conn,&element_free_conn,&element_compare_conn);
    if(list_socket==NULL) fprintf(stderr,"Failed to create list_socket\n");
    //use the socket with non-blocking mode
    tcp_get_sd(server,&server_sd);
    int flags=fcntl(server_sd,F_GETFL);
    flags |= O_NONBLOCK;
    if (fcntl(server_sd, F_SETFL, flags) == -1)
    {
        perror("fcntl error in settting flags\n");
    }
    
    
    /**********************************
    *  Initialize the active_fd_set
    * ********************************/
    FD_ZERO(&active_fd_set);
    tcp_get_sd(server,&server_sd);
    FD_SET(server_sd,&active_fd_set);
    
    while(1)
    {
        read_fd_set=active_fd_set;
        if(select(FD_SETSIZE,&read_fd_set,NULL,NULL,&timeout)<0)
        {
            perror("Error: select\n");
            exit(EXIT_FAILURE);
        }
        
        for(int i=0;i<FD_SETSIZE;++i)
        {
            if(FD_ISSET(i,&read_fd_set))
            {
                if(i==server_sd)
                {  
                    tcpsock_t *client;
                    if (tcp_wait_for_connection(server,&client)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
                    active_socket_t *client_active=malloc(sizeof(active_socket_t));
                    time_t timestamp=time(0);
                    client_active->socket=client;
                    client_active->ts=timestamp;
                    client_active->sensor_id=1;
                    dpl_insert_at_index(list_socket,client_active,0,false);
                    int client_sd;
                    tcp_get_sd(client,&client_sd);
                    int flags=fcntl(client_sd,F_GETFL);
                    flags |= O_NONBLOCK;
                    if (fcntl(client_sd, F_SETFL, flags) == -1)
                    {
                        perror("fcntl error in settting flags\n");
                    }
                    FD_SET(client_sd,&active_fd_set);
                }
            
                else
                {
                    //fprintf(stderr,"the client sd is %d\n",i);
                    sensor_data_t data;
                    int rc=read_from_socket(i,&data);
                    if(rc==TCP_NO_ERROR)/*if receive some data*/
                    {
                        DEBUG_PRINTF("socket got data and starts to insert\n");
                        sbuffer_insert(*buffer, &data);
                    }
                }
            }
        }
        dplist_node_t *dummy=dpl_get_first_reference(list_socket);
        while(dummy!=NULL)
        {
            time_t now=time(0);
            active_socket_t *socket_out=(active_socket_t*)dpl_get_element_at_reference(list_socket,dummy);
            if(socket_out->ts<(now-TIMEOUT))
            {
                int sd;
                sensor_id_t sensor_id=socket_out->sensor_id;/*get the sensor_id of the closing socket connection--> used for log message*/
                tcp_get_sd(socket_out->socket,&sd);         /*get the fd of the closig socket*/
                FD_CLR(sd,&active_fd_set);                  /*remove from the active fd set*/
                close(sd);                                  /*close the socket connection*/
                char *ip_addr;                              /*free the malloc memory in the tcpsock struct */
                tcp_get_ip_addr(socket_out->socket,&ip_addr);
                free(ip_addr);
                free(socket_out->socket);                   /*free the memory of tcpsock caused by tcp_wait_for_connection*/
                free(socket_out);
                dplist_node_t *reference=dummy;             
                dummy=dpl_get_next_reference(list_socket,dummy);
                dpl_remove_at_reference(list_socket,reference,false);
                char *logmsg;                               /*send log message to fifo*/
                asprintf(&logmsg,"%ld A sensor node with %" PRIu16" has closed the connection",time(0),sensor_id);
                write_to_fifo(logmsg);
                free(logmsg);                                   
            }
            dummy=dpl_get_next_reference(list_socket,dummy);
        }        
        if(dpl_size(list_socket)<1)
        {
             exit_thread();
             break;
        }
            
    } 
}
    
void connmgr_free()
{   
    if(server!=NULL)free(server);/*free all the socket created by tcp_passive_open and tcp_wait_for_connection*/
    dplist_node_t * reference=dpl_get_first_reference(list_socket);
    while(reference!=NULL)
    {
        active_socket_t * tcpsocket=(active_socket_t *) dpl_get_element_at_reference(list_socket,reference);
        free(tcpsocket->socket);
        free(tcpsocket);
        dplist_node_t *dummy=reference;
        reference=dpl_get_next_reference(list_socket,reference);
        dpl_remove_at_reference(list_socket,dummy,true);        
    }
    if(list_socket!=NULL)free(list_socket);
}


int read_from_socket(int socket_client,sensor_data_t *sensor_data)
{       DEBUG_PRINTF("socket with sd=%d starts to read from socket\n",socket_client);
        sensor_data_t data;
        int bytes,result;
    
        active_socket_t * ref_of_read_socket;
        tcpsock_t *read_socket;
        int read_socket_sd;
        //get the right tcpsock_t form the list_socket
        dplist_node_t * ref;
        ref=dpl_get_first_reference(list_socket);
        //iterate the list_socket to find the right socket
        while(ref!=NULL)
        {
            ref_of_read_socket=dpl_get_element_at_reference(list_socket,ref);
            tcp_get_sd(ref_of_read_socket->socket,&read_socket_sd);
            if(socket_client==read_socket_sd)
            {
                read_socket=ref_of_read_socket->socket;
                break;
            }            
            ref=dpl_get_next_reference(list_socket,ref);
        }
        // read sensor ID
        bytes = sizeof(data.id);
        result = tcp_receive(read_socket,(void *)&data.id,&bytes);
        // read temperature
        bytes = sizeof(data.value);
        result = tcp_receive(read_socket,(void *)&data.value,&bytes);
        // read timestamp
        bytes = sizeof(data.ts);
        result = tcp_receive(read_socket, (void *)&data.ts,&bytes);
        
        if ((result==TCP_NO_ERROR) && bytes) 
        {
            DEBUG_PRINTF("sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", data.id, data.value, (long int)data.ts);
            ref_of_read_socket->ts=time(0);
            if(ref_of_read_socket->sensor_id==1)
            {
                ref_of_read_socket->sensor_id=data.id;
                DEBUG_PRINTF("first data this sensor with id= %"PRIu16"\n", data.id);
                char *logmsg;
                asprintf(&logmsg,"%ld A sensor node with %" PRIu16" has opened a new connection\n",time(0),data.id);
                write_to_fifo(logmsg);
                free(logmsg);
            }
            *sensor_data=data;
        }
        return result;       
}


