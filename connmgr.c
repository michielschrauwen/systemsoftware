#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "config.h"
#include "lib/tcpsock.h"
#include "lib/dplist.h"
#include "connmgr.h"
#include "sbuffer.h"


#ifndef TIMEOUT
	#error "ERROR: Define timeout by adding '-DTIMEOUT=...' to gcc compiler";
#endif

struct sensor_info	{
	tcpsock_t * socket;	//our socket
	sensor_ts_t ts;	//timestamp of last measurement
	int sd;	//socket descriptor
	sensor_id_t id;
};

struct timeval timeout;

//global variables
dplist_t * client_list = NULL;

//sources that helped me write and understand the coding for this lab
//https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/
//http://beej.us/net2/bgnet.html
void connmgr_listen(int portnr, sbuffer_t ** buffer)	{
	
	tcpsock_t * server_socket, * client_socket;	
    sensor_info_t * this_client;	//dummy for the client we handle
	int listener;	//the socket descriptor of our master socket
    sensor_data_t * measurement;	//sensor data packet
    int fdmax;			//maximum file desriptor number
    int nbytes;
    int i;
    int result;
    int activity;
    
	//socket descriptor list for select
	fd_set read_fds;
	
	//set up list for client sockets
	client_list = dpl_create(element_copy_connmgr, element_free_connmgr, element_compare_connmgr);
	assert(client_list != NULL);
	
	//set timeout value for our struct timeval timeout
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
	
	//creating our master-server-listener socket
	if(tcp_passive_open(&server_socket, portnr) != TCP_NO_ERROR)	{
		perror("creating master socket failed");
		exit(EXIT_FAILURE);
	}
	
	//set the server socket as non-blocking
	if(tcp_get_sd(server_socket, &listener) != TCP_NO_ERROR)	{
		perror("calling server tcp_get_sd");
		exit(EXIT_FAILURE);
	}	//actually makes it non-blocking -
	int status = fcntl(listener, F_SETFL, fcntl(listener, F_GETFL, 0) | O_NONBLOCK);	//F_SETFL:  enable generation of signals on the file descriptor
	if (status == -1){
		perror("calling fcntl");
		exit(EXIT_FAILURE);	  
	}
    
    PARENT_DEBUG_PRINTF("Start listening for tcp sensor connections...");
	while(1)
	{		
		//clear the socket read list
		FD_ZERO(&read_fds);
		
		//add server socket to set
		FD_SET(listener, &read_fds);
		
		// keep track of the biggest file descriptor
		fdmax = listener;
		
		//add client sockets to set
		int client_sd;	//client socket descriptor
		for (i = 0; i < dpl_size(client_list); i++)	{
			
			this_client = dpl_get_element_at_index(client_list, i);
			if(this_client == NULL)	{
					perror("can't find client element at index in client_list when reading data");
					exit(EXIT_FAILURE);
			}
			client_sd = this_client -> sd;	//socket descriptor of this client
			
			//if valid socket descriptor then add to read list
			if(client_sd > 0)	{
				FD_SET(client_sd, &read_fds);
			}
			
			//highest file descriptor number, need it for the select function 
			if(client_sd > fdmax)	{
				fdmax = client_sd;
			}
		}
		
		//need to set timeout value every time since select() updates it in a way we don't need
		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = 0;
		
		//select to monitor multiple file descriptors
		activity = select(fdmax+1, &read_fds, NULL, NULL, &timeout);
		
		if(activity < 0)	{	//select returns -1 on error
			perror("select ERROR");
			exit(EXIT_FAILURE);
		}
		else if(activity == 0)	{	//select returns 0 on timeout
			PARENT_DEBUG_PRINTF("Time out, we received no data within %d seconds", TIMEOUT);
			break;
		}
		//if something happens on the master socket, then it is an incoming connection
		if(FD_ISSET(listener, &read_fds))	{
			//handle new connections
			if(tcp_wait_for_connection(server_socket, &client_socket) != TCP_NO_ERROR)	{
				perror("tcp_wait_for_connection");
				exit(EXIT_FAILURE);
			}
			//add to client socket list
			sensor_info_t new_sensor;
			int new_sd;
			tcp_get_sd(client_socket, &new_sd);						
			new_sensor.socket = client_socket;
			new_sensor.ts = 0;				
			new_sensor.sd = new_sd;
			new_sensor.id = 0;	//still unknown id untill we get the first data
			
			dpl_insert_at_index(client_list, &new_sensor, dpl_size(client_list), true);	//add new socket to client list
			
			PARENT_DEBUG_PRINTF("selectserver: new connection socket %d",  new_sd);
		}
		
		// run through the existing connections looking for data to read
		for(i = 0; i < dpl_size(client_list); i++)	{
			
			this_client = (sensor_info_t*) dpl_get_element_at_index(client_list, i);
			
			measurement = malloc(sizeof(sensor_data_t));
			MALLOC_ERROR(measurement);
			
			if(this_client == NULL)	{
					perror("can't find client element at index in client_list when reading data");
					exit(EXIT_FAILURE);
			}
			//test to see if client socket descriptor is in the set read_fds
			if(FD_ISSET(this_client->sd, &read_fds))	{	//if file descripter is set -> handle data from a client
				result = -1;					
				nbytes = sizeof(measurement -> id);
				result = tcp_receive(this_client -> socket, (void *)&(measurement -> id), &nbytes);
				
				if(result == TCP_CONNECTION_CLOSED)	{	//close connection from this client
					PARENT_DEBUG_PRINTF( "Sensor with sd = %d has closed connection", this_client -> sd);
					
					char * fifo_message;
					asprintf(&fifo_message, "%ld The sensor node with %hu has closed the connection\n", time(NULL), this_client -> id);
					pthread_mutex_lock(&mutex_fifo);
					write_to_fifo(fifo_message);
					pthread_mutex_unlock(&mutex_fifo);
					free(fifo_message);
					
					if(tcp_close(&this_client -> socket) != TCP_NO_ERROR)	{
						perror("fail when closing connection");
						exit(EXIT_FAILURE);
					}
					dpl_remove_at_index(client_list, i, true);
				}	
				else if((result == TCP_NO_ERROR) && nbytes > 0)		//we have new measurement  because timestamp of last is still zero
				{
					nbytes = sizeof(measurement -> value);
					result = tcp_receive(this_client -> socket, (void *)&(measurement -> value), &nbytes);
				
					nbytes = sizeof(measurement -> ts);
					result = tcp_receive(this_client -> socket, (void *)&(measurement -> ts), &nbytes);
						
					if(this_client -> ts == 0)	{	//we have new sensor with measurement
						PARENT_DEBUG_PRINTF( "New Sensor connection with id = %hu", measurement -> id);
						this_client -> id = measurement -> id;
						char * fifo_message;
						asprintf(&fifo_message, "%ld A sensor node with %hu has opened a new connection\n", time(NULL), measurement -> id);
						pthread_mutex_lock(&mutex_fifo);
						write_to_fifo(fifo_message);
						pthread_mutex_unlock(&mutex_fifo);
						free(fifo_message);
					}			
					PARENT_DEBUG_PRINTF( "New measurement received -> Sensor id = %hu - Temperature = %f - Timestamp = %ld", measurement -> id, measurement -> value, (long int) measurement -> ts);
					this_client -> ts = measurement ->ts;	//update timestamp of latest measurement
					PTHREAD_FUNCTION_ERROR(pthread_mutex_lock(&mutex_buf));
					SBUFFER_INSERT_ERROR(sbuffer_insert(*buffer, measurement));
					PTHREAD_FUNCTION_ERROR(pthread_mutex_unlock(&mutex_buf));
				}							
			}		
			else	//for the clients that are not part of read list
			{
				//Check if need to be timed out
				if((time(NULL) - this_client-> ts) >= TIMEOUT && this_client-> ts != 0)
                {
						PARENT_DEBUG_PRINTF("This Client socket(sd: %d - ID: %hu) has to be removed - timeout!",this_client->sd, this_client->id);
						FD_CLR(this_client -> sd, &read_fds);
						tcp_close(&this_client->socket);
						dpl_remove_at_index(client_list, i, true);
				}
			}
			free(measurement);
		}
		
	}
	if(tcp_close(&server_socket) != TCP_NO_ERROR)	{
		perror("error from tcp close server socket - end");
		exit(EXIT_FAILURE);
	}
	PARENT_DEBUG_PRINTF("exiting the connection manager - Goodbye!");
	PARENT_DEBUG_PRINTF("free connmanager");
    dpl_free(&client_list, true);
    free(client_list);
    client_list = NULL;
}

void * element_copy_connmgr(void * element)	{
	
	sensor_info_t * copy_element = malloc(sizeof(sensor_info_t));
	MALLOC_ERROR(copy_element);
	if(element != NULL)	{
		*copy_element = *(sensor_info_t*) element;
	}
	else
	{
		perror("hard copy of element failed\n");
		exit(EXIT_FAILURE);
	}
	return copy_element;
}

void element_free_connmgr(void ** element)	{
	free((sensor_info_t*) * element);
	*element = NULL;
}

int element_compare_connmgr(void * x, void * y)	{
	return 0;
}
