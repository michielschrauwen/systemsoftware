#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"
#include "datamgr.h"
#include "lib/dplist.h"
#include "sbuffer.h"

//Function declarations
void * element_copy_datamgr(void * element);
void element_free_datamgr(void ** element);
int element_compare_datamgr(void * x, void * y);

sensor_node_t * get_sensor_node_with_id(sensor_id_t id_of_sensor);

//global variables
dplist_t * sensor_list;

void datamgr_parse_sensor_data(FILE * fp_sensor_map, sbuffer_t ** buffer)
{
	sensor_list = dpl_create(element_copy_datamgr, element_free_datamgr, element_compare_datamgr);
    assert(sensor_list != NULL);
    
	if(sensor_list == NULL)	{	//more error checks needed?
		fprintf(stderr,"Creating of sensor list failed\n");
		exit(EXIT_FAILURE);
	}
	if(fp_sensor_map == NULL)	{
		fprintf(stderr,"fp_sensor_map == NULL\n");
		exit(EXIT_FAILURE);
	}
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int nr_of_lines = 0;
	sensor_id_t id_sensor = -1;
	sensor_id_t id_room = -1;
	
	while((read = getline(&line, &len, fp_sensor_map)) != -1)	{
		nr_of_lines++;		
		sscanf(line, "%hu" "%hu" "\n", &id_room, &id_sensor);
		PARENT_DEBUG_PRINTF("room: %hu with sensor %hu is added to our list", id_room, id_sensor);
		sensor_node_t * this_sensor_node = (sensor_node_t *) malloc(sizeof(sensor_node_t));
		MALLOC_ERROR(this_sensor_node);
		this_sensor_node -> sensor_id = id_sensor;
		this_sensor_node -> room_id = id_room;		
		this_sensor_node -> running_avg = 0;
		this_sensor_node -> timestamp = time(NULL); 
		this_sensor_node -> nr_of_meass = 0;
		sensor_list = dpl_insert_at_index(sensor_list, this_sensor_node, nr_of_lines, true);		
		free(this_sensor_node);	
		this_sensor_node = NULL;	
	}
	free(line);
	line = NULL;
	
	if(*buffer == NULL)	{
		fprintf(stderr, "*buffer == NULL\n");
		exit(EXIT_FAILURE);
	}
	
	sensor_data_t * first_data_node = NULL;
	sensor_node_t * dummy_sensor_node = NULL;

	do{		
		PTHREAD_FUNCTION_ERROR(pthread_mutex_lock(&mutex_buf));
		sbuffer_node_t * first_node = sbuffer_get_first_node(*buffer);
		PTHREAD_FUNCTION_ERROR(pthread_mutex_unlock(&mutex_buf));
		
		if(first_node != NULL)	
		{			
			first_data_node = (sensor_data_t*) malloc(sizeof(sensor_data_t));
			MALLOC_ERROR(first_data_node);
			first_data_node -> id = 0;
			first_data_node -> value = 0.0f;
			first_data_node -> ts = 0;
			
			PTHREAD_FUNCTION_ERROR(pthread_mutex_lock(&mutex_buf));
			SBUFFER_REMOVE_ERROR(sbuffer_remove(*buffer, first_data_node));
			PTHREAD_FUNCTION_ERROR(pthread_mutex_unlock(&mutex_buf));
			
			if(first_data_node -> id != 0)	{
				
				dummy_sensor_node = get_sensor_node_with_id(first_data_node -> id);
				
				if(dummy_sensor_node == NULL)	{
					PARENT_DEBUG_PRINTF("Datamgr - no sensor found with this ID:%hu in sensor_list", first_data_node -> id);
					char * fifo_message;
					asprintf(&fifo_message, "%ld Received sensor data with invalid sensor node ID %hu\n", time(NULL), first_data_node -> id);
					pthread_mutex_lock(&mutex_fifo);
					write_to_fifo(fifo_message);
					pthread_mutex_unlock(&mutex_fifo);
					free(fifo_message);
				}
				else
				{
					dummy_sensor_node -> timestamp = first_data_node -> id;
					
					//Some code to decide where to insert our new measurement into the array of temperatures                   
					int array_position;
					int nr_of_meass_minus_1 = dummy_sensor_node -> nr_of_meass;                        
					if(nr_of_meass_minus_1 < RUN_AVG_LENGTH)	
					{
						array_position = nr_of_meass_minus_1;
					}
					else
					{
						while(nr_of_meass_minus_1 >= RUN_AVG_LENGTH)	
						{
							nr_of_meass_minus_1 -= RUN_AVG_LENGTH;
							if(nr_of_meass_minus_1 < RUN_AVG_LENGTH)	
							{
								array_position = nr_of_meass_minus_1;
							}
						}
					}						
					dummy_sensor_node -> temperatures[array_position] = first_data_node -> value;
					
					dummy_sensor_node -> nr_of_meass++;					
					dummy_sensor_node -> running_avg = datamgr_get_avg(first_data_node -> id);
					PARENT_DEBUG_PRINTF("Datamgr -- Adding new measurement to array of sensor-room successful - ID:%hu - %f - %ld",first_data_node -> id, first_data_node -> value,first_data_node -> ts);
				}
			}
			free(first_data_node);
			first_data_node = NULL;			
		}
	} while(run_datamgr == true);
	
	
						
}

void datamgr_parse_sensor_files(FILE * fp_sensor_map, FILE * fp_sensor_data)
{
	sensor_list = dpl_create(element_copy_datamgr, element_free_datamgr, element_compare_datamgr);
        
	if(sensor_list == NULL)	{	//more error checks needed?
		fprintf(stderr,"Creating of sensor list failed\n");
		exit(EXIT_FAILURE);
	}
	if(fp_sensor_map == NULL)	{
		fprintf(stderr,"fp_sensor_map == NULL\n");
		exit(EXIT_FAILURE);
	}
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int nr_of_line = 0;
	sensor_id_t id_sensor = -1;
	sensor_id_t id_room = -1;
	
	while((read = getline(&line, &len, fp_sensor_map)) != -1)	{
		nr_of_line++;		
		sscanf(line, "%hu" "%hu" "\n", &id_room, &id_sensor);
		PARENT_DEBUG_PRINTF("room: %hu with sensor %hu is added to our list\n", id_room, id_sensor);
		sensor_node_t * this_sensor_node = (sensor_node_t *) malloc(sizeof(sensor_node_t));
		MALLOC_ERROR(this_sensor_node);
		this_sensor_node -> sensor_id = id_sensor;
		this_sensor_node -> room_id = id_room;		
		this_sensor_node -> running_avg = 0;
		this_sensor_node -> timestamp = time(NULL); 
		this_sensor_node -> nr_of_meass = 0;
		sensor_list = dpl_insert_at_index(sensor_list, this_sensor_node, nr_of_line, true);		
		free(this_sensor_node);		
	}
	free(line);
	line = NULL;
		
	if(fp_sensor_data == NULL)	{
		fprintf(stderr,"fp_sensor_data == NULL\n");
		exit(EXIT_FAILURE);
	}		
        sensor_node_t * this_sensor_node;
        id_sensor = -1;
        sensor_value_t temperature = 0.0f;
        sensor_ts_t ts = 0;
        
        while(fread(&id_sensor, sizeof(sensor_id_t), 1, fp_sensor_data) > 0)	{
                fread(&temperature, sizeof(sensor_value_t), 1, fp_sensor_data);
                fread(&ts, sizeof(sensor_ts_t), 1, fp_sensor_data);
                //printf("sensorID: %d - temp: %f - ts: %ld is added to data\n", id_sensor, temperature, ts);
                this_sensor_node = get_sensor_node_with_id(id_sensor);
                
                if(this_sensor_node == NULL)	{
					PARENT_DEBUG_PRINTF("Can't find sensorID %hu in the list", id_sensor);
                }
                else
                {
					this_sensor_node -> timestamp = ts;  
					
					//Some code to decide where to insert our new measurement into the array of temperatures                   
					int array_position;
					int nr_of_meass_minus_1 = this_sensor_node -> nr_of_meass;                        
					if(nr_of_meass_minus_1 < RUN_AVG_LENGTH)	
					{
						array_position = nr_of_meass_minus_1;
					}
					else
					{
						while(nr_of_meass_minus_1 >= RUN_AVG_LENGTH)	
						{
							nr_of_meass_minus_1 -= RUN_AVG_LENGTH;
							if(nr_of_meass_minus_1 < RUN_AVG_LENGTH)	
							{
								array_position = nr_of_meass_minus_1;
							}
						}
					}						
					this_sensor_node -> temperatures[array_position] = temperature;
					this_sensor_node -> nr_of_meass++;
					this_sensor_node -> running_avg = datamgr_get_avg(id_sensor);
                        
                }
                
        }			
			
		
				
}
	
	
sensor_node_t * get_sensor_node_with_id(sensor_id_t id_of_sensor)	{
	
	sensor_node_t * this_sensor_node, * dummy_sensor_node;
	this_sensor_node = malloc(sizeof(sensor_node_t));
	MALLOC_ERROR(this_sensor_node);
	this_sensor_node -> sensor_id = id_of_sensor;
	int index = 0;
	
	while(index < dpl_size(sensor_list))	{
		
		dummy_sensor_node = (sensor_node_t*) dpl_get_element_at_index(sensor_list, index);
		
		if(element_compare_datamgr(this_sensor_node, dummy_sensor_node) == 0)	{
			free(this_sensor_node);
			this_sensor_node = NULL;
			return dummy_sensor_node;
		}
		index++;
	}
	free(this_sensor_node);
	this_sensor_node = NULL;	
	PARENT_DEBUG_PRINTF("sensor node not found!!!!!!! \n");
	return NULL;
}

void * element_copy_datamgr(void * element)
{
	sensor_node_t * copy_element = malloc(sizeof(sensor_node_t));
	MALLOC_ERROR(copy_element);
	if(element != NULL)	{
		*copy_element = *(sensor_node_t*) element;
	}
	else
	{
		fprintf(stderr,"hard copy of element failed\n");
		exit(EXIT_FAILURE);
	}
	return copy_element;
}

void element_free_datamgr(void ** element)
{
	free((sensor_node_t*)* element);
	*element = NULL;
}

int element_compare_datamgr(void * x, void * y)
{
	if((((sensor_node_t*) x) -> sensor_id) > (((sensor_node_t*) y) -> sensor_id))	{
		return 1;
	} else if ((((sensor_node_t*) x) -> sensor_id) == (((sensor_node_t*) y) -> sensor_id))
	{
		return 0;	
	} else
	{
		return -1;
	}	
}

/*
 * This method should be called to clean up the datamgr, and to free all used memory. 
 * After this, any call to datamgr_get_room_id, datamgr_get_avg, datamgr_get_last_modified or datamgr_get_total_sensors will not return a valid result
 */
void datamgr_free()
{
	PARENT_DEBUG_PRINTF("datamgr_free is called");
	dpl_free(&sensor_list, 1);	
	if(datamgr_get_total_sensors() == 0)	{
		free(sensor_list);
		sensor_list = NULL;
		PARENT_DEBUG_PRINTF("freeing of sensor list succesful\n");
	}
}
    
/*   
 * Gets the room ID for a certain sensor ID
 * Use ERROR_HANDLER() if sensor_id is invalid 
 */
uint16_t datamgr_get_room_id(sensor_id_t sensor_id)
{
	sensor_node_t * this_sensor_node = get_sensor_node_with_id(sensor_id);
	return this_sensor_node -> room_id;
}
    


/*
 * Gets the running AVG of a certain senor ID (if less then RUN_AVG_LENGTH measurements are recorded the avg is 0)
 * Use ERROR_HANDLER() if sensor_id is invalid 
 * source: https://www.tutorialspoint.com/learn_c_by_examples/average_of_array_in_c.htm
 */
sensor_value_t datamgr_get_avg(sensor_id_t sensor_id) //it calculates the avg.
{
	sensor_node_t * this_sensor_node = get_sensor_node_with_id(sensor_id);
	PARENT_DEBUG_PRINTF("CHECK sensor id: %d", this_sensor_node -> sensor_id);
	if(this_sensor_node -> nr_of_meass >= RUN_AVG_LENGTH)	{
		sensor_value_t total = 0;
		sensor_value_t average = 0;
		for(int i = 0; i < RUN_AVG_LENGTH; i++)	{
			total += this_sensor_node -> temperatures[i];
		}
		average = total / RUN_AVG_LENGTH;
		PARENT_DEBUG_PRINTF("Average temperature in room %d: %f°C", this_sensor_node -> room_id, average);
		if(average < SET_MIN_TEMP)	{
			PARENT_DEBUG_PRINTF("Average temperature is too low in room %d: %f°C", this_sensor_node -> room_id, average);
			
			char * fifo_message;
			asprintf(&fifo_message, "%ld The sensor node with %hu reports it's too cold (running avg temperature = %f)\n", time(NULL), this_sensor_node -> sensor_id, average);
			pthread_mutex_lock(&mutex_fifo);
			write_to_fifo(fifo_message);
			pthread_mutex_unlock(&mutex_fifo);
			free(fifo_message);
		}
		if(average > SET_MAX_TEMP)	{
			PARENT_DEBUG_PRINTF("Average temperature is too high in room %d: %f°C", this_sensor_node -> room_id, average);
			
			char * fifo_message;
			asprintf(&fifo_message, "%ld The sensor node with %hu reports it's too hot (running avg temperature = %f)\n", time(NULL), this_sensor_node -> sensor_id, average);
			pthread_mutex_lock(&mutex_fifo);
			write_to_fifo(fifo_message);
			pthread_mutex_unlock(&mutex_fifo);
			free(fifo_message);
		}
		
		this_sensor_node -> running_avg = average;
		return average;
	}
	else
	{
		PARENT_DEBUG_PRINTF("Not enough readings to calculate average temperature in room %d\n",this_sensor_node -> room_id);
		return 0;
	}	
}
    

/*
 * Returns the time of the last reading for a certain sensor ID
 * Use ERROR_HANDLER() if sensor_id is invalid 
 */
time_t datamgr_get_last_modified(sensor_id_t sensor_id)
{
	sensor_node_t * this_sensor_node = get_sensor_node_with_id(sensor_id);
	return this_sensor_node -> timestamp;
}
    

/*
 *  Return the total amount of unique sensor ID's recorded by the datamgr
 */
int datamgr_get_total_sensors()
{
	if(sensor_list == NULL)	{
		return 0;
	}
	else
	{
		return dpl_size(sensor_list);
	}
}
    
