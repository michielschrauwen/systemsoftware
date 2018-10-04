#define _GNU_SOURCE
#include <sqlite3.h>
#include <stdio.h>
#include <pthread.h>

#include "sensor_db.h"
#include "sbuffer.h"
#include "config.h"


DBCONN * init_connection(char clear_up_flag)	{
    
    sqlite3 *db;	//database handle
    char *err_msg = 0;	//pointer to created error message if any
    char *sql = 0;	//pointer to sql querries 
    char *db_name = TO_STRING(DB_NAME);
    char *table_name = TO_STRING(TABLE_NAME);
    
    int rc = sqlite3_open(db_name, &db); //open connection to database - rc is return code from database
    
    if (rc != SQLITE_OK) {        
        fprintf(stderr, "SQL error - opening database: %s\n", sqlite3_errmsg(db));
        
        char * fifo_message;
		asprintf(&fifo_message, "%ld Unable to connect to SQL server.\n", time(NULL));
		pthread_mutex_lock(&mutex_fifo);
		write_to_fifo(fifo_message);
		pthread_mutex_unlock(&mutex_fifo);
		free(fifo_message);
        
        sqlite3_close(db);        
        return NULL;
    }
    else
	{
		char * fifo_message;
		asprintf(&fifo_message, "%ld Connection to SQL server established.\n", time(NULL));
		pthread_mutex_lock(&mutex_fifo);
		write_to_fifo(fifo_message);
		pthread_mutex_unlock(&mutex_fifo);
		free(fifo_message);
	}
    
    if(clear_up_flag == 1)	{
		sql = sqlite3_mprintf("DROP TABLE IF EXISTS %q;", table_name);

		rc = sqlite3_exec(db, sql, 0, 0, &err_msg); //wrapper around sqlite3_prepare_v2/step/finalize that runs multiple statements at once
		
		if (rc != SQLITE_OK ) {			
			fprintf(stderr, "SQL error when dropping existing table: %s\n", err_msg);			
			sqlite3_free(err_msg);
			sqlite3_close(db);								
			return NULL;
		}
		
		sqlite3_free(sql); 
	}
	sql = sqlite3_mprintf("CREATE TABLE %q(Id INTEGER PRIMARY KEY AUTOINCREMENT, sensor_id INT, sensor_value DECIMAL(4,2), timestamp TIMESTAMP);", table_name) ;
	
	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
		
	if (rc != SQLITE_OK ) {		
		fprintf(stderr, "SQL error - creating new table: %s\n", err_msg);		
		sqlite3_free(err_msg);        
		sqlite3_close(db);		
		return NULL;
	}
	else
	{
		char * fifo_message;
		asprintf(&fifo_message, "%ld New table %s created.\n", time(NULL), table_name);
		pthread_mutex_lock(&mutex_fifo);
		write_to_fifo(fifo_message);
		pthread_mutex_unlock(&mutex_fifo);
		free(fifo_message);
	}
	
	sqlite3_free(sql); 
	fprintf(stderr, "Database initialization succesful!\n");
	
    return db;
}

void disconnect(DBCONN *conn)	{
		
	int rc = sqlite3_close(conn);
	
	if(rc != SQLITE_OK)	{
		fprintf(stderr, "SQL error - disconnecting from database: %s>n", sqlite3_errmsg(conn));
		abort();
	}
	
	char * fifo_message;
	asprintf(&fifo_message, "%ld Connection to SQL server lost.\n", time(NULL));
	pthread_mutex_lock(&mutex_fifo);
	write_to_fifo(fifo_message);
	pthread_mutex_unlock(&mutex_fifo);
	free(fifo_message);
	
	PARENT_DEBUG_PRINTF("Disconnect from database succesful!\n");
	
}

int insert_sensor(DBCONN * conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts)	{
	
	char *err_msg = 0;
	char *table_name = TO_STRING(TABLE_NAME);
	
	char *sql = sqlite3_mprintf("INSERT INTO %q(sensor_id, sensor_value, timestamp) VALUES (%u, %f, %ld);", table_name, id, value, ts);
	
	int rc = sqlite3_exec(conn, sql, 0, 0, &err_msg);
	
	if (rc != SQLITE_OK ) {		
		fprintf(stderr, "SQL error - insert_sensor: %s\n", err_msg);		
		sqlite3_free(err_msg);		
		return 1;
	}
	sqlite3_free(sql);
	PARENT_DEBUG_PRINTF("Storagemgr -- Inserting of one measurement successful - ID:%hu - %f - %ld\n", id, value, ts);
    return 0;
}

void storagemgr_parse_sensor_data(DBCONN * conn, sbuffer_t * buffer)	
{
	if(buffer == NULL)	{
		fprintf(stderr, "buffer == NULL\n");
		exit(EXIT_FAILURE);
	}	
	
	sensor_data_t * first_data_node = NULL;
	
	do
	{
		PTHREAD_FUNCTION_ERROR(pthread_mutex_lock(&mutex_buf));
		sbuffer_node_t * first_node = sbuffer_get_first_node(buffer);
		PTHREAD_FUNCTION_ERROR(pthread_mutex_unlock(&mutex_buf));
		
		if(first_node != NULL)	
		{
			first_data_node = (sensor_data_t*) malloc(sizeof(sensor_data_t));
			MALLOC_ERROR(first_data_node);
			first_data_node -> id = 0;
			first_data_node -> value = 0.0f;
			first_data_node -> ts = 0;
			
			PTHREAD_FUNCTION_ERROR(pthread_mutex_lock(&mutex_buf));
			SBUFFER_REMOVE_ERROR(sbuffer_remove(buffer, first_data_node));
			PTHREAD_FUNCTION_ERROR(pthread_mutex_unlock(&mutex_buf));
			
			if(first_data_node -> id != 0)	{
				insert_sensor(conn, first_data_node -> id, first_data_node -> value, first_data_node -> ts);
			}
			free(first_data_node);
			first_data_node = NULL;
		}	
		
	} while (run_storagemgr == true);
	
}

int insert_sensor_from_file(DBCONN * conn, FILE * sensor_data)	{
	
	if(sensor_data == NULL)	{
		fprintf(stderr, "File sensor_data is NULL\n");
		return 1;
	}
	
	sensor_id_t id_sensor = -1;
	sensor_value_t temperature = 0.0f;
	sensor_ts_t ts = 0;
		
	while(fread(&id_sensor, sizeof(sensor_id_t), 1, sensor_data) > 0)	{
		fread(&temperature, sizeof(sensor_value_t), 1, sensor_data);
		fread(&ts, sizeof(sensor_ts_t), 1, sensor_data);
		
		int return_value = 0;
		return_value = insert_sensor(conn, id_sensor, temperature, ts);
		
		if(return_value == 1)	{
			fprintf(stderr, "Error - insert_sensor_from_file\n");
			return 1;
		}
		id_sensor = -1;
		temperature = 0.0f;
		ts = 0;
	}
	
	fprintf(stderr, "Inserting measurements from file succesful!\n");
	return 0;
}

int find_sensor_all(DBCONN * conn, callback_t f)	{
	
	char *err_msg = 0;
	char *table_name = TO_STRING(TABLE_NAME);
	
	char *sql = sqlite3_mprintf("SELECT * FROM %q;", table_name);
	
	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);	//met callback funtion voor elke row hier
	
	if (rc != SQLITE_OK ) {		
		fprintf(stderr, "SQL error - find_sensor_all: %s\n", err_msg);		
		sqlite3_free(err_msg);		
		return 1;
	}
	
	sqlite3_free(sql); 
	fprintf(stderr, "Finding all measurements succesful!\n");
	return 0;
}

int find_sensor_by_value(DBCONN * conn, sensor_value_t value, callback_t f)	{
	
	char *err_msg = 0;
	char *table_name = TO_STRING(TABLE_NAME);
	
	char *sql = sqlite3_mprintf("SELECT * FROM %q WHERE sensor_value = %f;", table_name, value);
	
	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);	//met callback funtion voor elke row hier
	
	if (rc != SQLITE_OK ) {		
		fprintf(stderr, "SQL error - find_sensor_by_value: %s\n", err_msg);		
		sqlite3_free(err_msg);		
		return 1;
	}
	
	sqlite3_free(sql); 
	fprintf(stderr, "Finding all measurements by temperature succesful!\n");
	return 0;
}

int find_sensor_exceed_value(DBCONN * conn, sensor_value_t value, callback_t f)	{
	
	char *err_msg = 0;
	char *table_name = TO_STRING(TABLE_NAME);
	
	char *sql = sqlite3_mprintf("SELECT * FROM %q WHERE sensor_value > %f;", table_name, value);
	
	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);	//met callback funtion voor elke row hier
	
	if (rc != SQLITE_OK ) {		
		fprintf(stderr, "SQL error - find_sensor_exceed_value: %s\n", err_msg);		
		sqlite3_free(err_msg);		
		return 1;
	}
	
	sqlite3_free(sql); 
	fprintf(stderr, "Finding all measurements by exceeding temperature succesful!\n");
	return 0;
}

int find_sensor_by_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f)	{
	
	char *err_msg = 0;
	char *table_name = TO_STRING(TABLE_NAME);
	
	char *sql = sqlite3_mprintf("SELECT * FROM %q WHERE timestamp = %ld;", table_name, ts);
	
	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);	//met callback funtion voor elke row hier
	
	if (rc != SQLITE_OK ) {		
		fprintf(stderr, "SQL error - find_sensor_by_timestamp: %s\n", err_msg);		
		sqlite3_free(err_msg);		
		return 1;
	}
	
	sqlite3_free(sql); 
	fprintf(stderr, "Finding all measurements by timestamp succesful!\n");
	return 0;
}

int find_sensor_after_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f)	{
	
	char *err_msg = 0;
	char *table_name = TO_STRING(TABLE_NAME);
	
	char *sql = sqlite3_mprintf("SELECT * FROM %q WHERE timestamp > %ld;", table_name, ts);
	
	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);	//met callback funtion voor elke row hier
	
	if (rc != SQLITE_OK ) {		
		fprintf(stderr, "SQL error - find_sensor_after_timestamp: %s\n", err_msg);		
		sqlite3_free(err_msg);		
		return 1;
	}
	
	sqlite3_free(sql); 
	fprintf(stderr, "Finding all measurements after timestamp succesful!\n");
	return 0;
}
