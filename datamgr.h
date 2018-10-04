#ifndef DATAMGR_H_
#define DATAMGR_H_

#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "sbuffer.h"

#ifndef RUN_AVG_LENGTH
  #define RUN_AVG_LENGTH 5
#endif

#ifndef SET_MAX_TEMP
	//#define SET_MAX_TEMP 25
  #error SET_MAX_TEMP not set
#endif

#ifndef SET_MIN_TEMP
	//#define SET_MIN_TEMP 18
  #error SET_MIN_TEMP not set
#endif

typedef struct	sensor_node{
	sensor_id_t sensor_id;
	sensor_id_t room_id;
	sensor_value_t running_avg;
	sensor_value_t temperatures[RUN_AVG_LENGTH];
	time_t timestamp;
	int nr_of_meass;
} sensor_node_t;

/*
 *  This method holds the core functionality of your datamgr. It takes in 2 file pointers to the sensor files and parses them. 
 *  When the method finishes all data should be in the internal pointer list and all log messages should be printed to stderr.
 *  REMARK: this is the function you already implemented in a previous lab but for the final assignment you read data from the shared buffer and not from file. That's why you find   *  below the modified function 'datamgr_parse_sensor_data'
 */

void datamgr_parse_sensor_files(FILE * fp_sensor_map, FILE * fp_sensor_data);

/*
 * Reads continiously all data from the shared buffer data structure, parse the room_id's
 * and calculate the running avarage for all sensor ids
 * When *buffer becomes NULL the method finishes. This method will NOT automatically free all used memory
 */
void datamgr_parse_sensor_data(FILE * fp_sensor_map, sbuffer_t ** buffer);


/*
 * This method should be called to clean up the datamgr, and to free all used memory. 
 * After this, any call to datamgr_get_room_id, datamgr_get_avg, datamgr_get_last_modified 
 * or datamgr_get_total_sensors will not return a valid result
 */
void datamgr_free();

/*
 * Gets the room ID for a certain sensor ID
 */
uint16_t datamgr_get_room_id(sensor_id_t sensor_id);

/*
 * Gets the running AVG of a certain senor ID 
 */
sensor_value_t datamgr_get_avg(sensor_id_t sensor_id);

/*
 * Returns the time of the last reading for a certain sensor ID
 */
time_t datamgr_get_last_modified(sensor_id_t sensor_id);

/*
 * Return the total amount of unique sensor ID's recorded by the datamgr
 */
int datamgr_get_total_sensors();
   


#endif  //DATAMGR_H_

