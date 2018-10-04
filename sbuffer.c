#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "sbuffer.h"
#include "config.h"

struct sbuffer {
	sbuffer_node_t * head;
	sbuffer_node_t * tail;
};	


int sbuffer_init(sbuffer_t ** buffer)
{
	*buffer = malloc(sizeof(sbuffer_t));
	MALLOC_ERROR(*buffer);
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
	sbuffer_node_t * dummy;
	if (buffer == NULL) return SBUFFER_FAILURE;
	if (buffer->head == NULL) return SBUFFER_NO_DATA;
	
	if(pthread_equal(datamgr_td, pthread_self()))	{	//datamgr reads the buffer
		dummy = buffer -> head;
		while(dummy != NULL)	{
			if(dummy -> datamgr_read_status == false)	{
				break;
			}
			if(dummy -> next == NULL)	{	//last node of buffer
				return SBUFFER_NO_DATA;
			}
			dummy = dummy -> next;
		}
		*data = dummy -> data;		//set *data pointer to return to calling function
		dummy -> datamgr_read_status = true;
	}	
	else if(pthread_equal(storagemgr_td, pthread_self()))	{	//storagemgr reads the buffer
		dummy = buffer -> head;
		while(dummy != NULL)	{
			if(dummy -> storagemgr_read_status == false)	{
				break;
			}
			if(dummy -> next == NULL)	{
				return SBUFFER_NO_DATA;
			}
			dummy = dummy -> next;
		}
		*data = dummy -> data;
		dummy -> storagemgr_read_status = true;
	}
	else	{
		return SBUFFER_FAILURE;
	}		
	
	//if read by both datamgr & storagemgr we can delete the node	
	if(((dummy -> datamgr_read_status) == true) && ((dummy -> storagemgr_read_status) == true))	{	
		if (buffer->head == buffer->tail) // buffer has only one node
		{
			buffer->head = buffer->tail = NULL; 
		}
		else  // buffer has many nodes empty
		{
			buffer->head = buffer->head->next;
		}
		free(dummy);
	}
	return SBUFFER_SUCCESS;
}


int sbuffer_insert(sbuffer_t * buffer, sensor_data_t * data)
{
	sbuffer_node_t * dummy;
	if (buffer == NULL) return SBUFFER_FAILURE;
	dummy = malloc(sizeof(sbuffer_node_t));
	if (dummy == NULL) return SBUFFER_FAILURE;
	dummy -> data = *data;
	dummy -> next = NULL;
	dummy -> datamgr_read_status = false;
	dummy -> storagemgr_read_status = false;
	if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
	{
		buffer->head = buffer->tail = dummy;
	} 
	else // buffer not empty
	{
		buffer->tail->next = dummy;
		buffer->tail = buffer->tail->next; 
	}
	return SBUFFER_SUCCESS;
}

sbuffer_node_t * sbuffer_get_first_node(sbuffer_t * buffer)
{
	if(buffer == NULL)	{
		PARENT_DEBUG_PRINTF("Buffer == NULL - sbuffer_get_first_node ERROR\n");
		exit(EXIT_FAILURE);
	}
	if(buffer -> head == NULL)	{
		//~ PARENT_DEBUG_PRINTF("Buffer -> head == NULL - sbuffer is empty\n");	//wordt gespammed als je het aanzet
		return NULL;
	}
	return buffer -> head;
}

int sbuffer_print_data(sbuffer_t * buffer)
{
	PARENT_DEBUG_PRINTF("------------Start sbuffer_print_data-------------\n");
	if(buffer == NULL)	{
		PARENT_DEBUG_PRINTF("Buffer == NULL - sbuffer_print_data ERROR\n");
		exit(EXIT_FAILURE);
	}
	if(buffer -> head == NULL)	{
		PARENT_DEBUG_PRINTF("Buffer -> head == NULL - sbuffer is empty\n");
	}
	sbuffer_node_t * dummy = buffer -> head;
	int count = 0;
	while(dummy != NULL)	{
		PARENT_DEBUG_PRINTF("Data node:%d - sensornode:%hu - temperature:%f - timestamp: %ld\n", count, dummy -> data.id, dummy -> data.value, dummy -> data.ts);
		count++;
		dummy = dummy -> next;		
	}
	return SBUFFER_SUCCESS;
}


