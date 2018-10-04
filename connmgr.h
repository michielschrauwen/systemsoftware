#ifndef __CONNMGR_H__
#define __CONNMGR_H__

#include "config.h"
#include "sbuffer.h"


#ifndef TIMEOUT
	#error "ERROR: Define timeout by adding '-DTIMEOUT=...' to gcc compiler";
#endif

typedef struct sensor_info sensor_info_t;

void connmgr_listen(int portnr, sbuffer_t ** buffer);
/*Handling of multiple temperature sensors using TCP/IP sockets on server without multi threading
 */

/*Callback functions for dplist
 */
void * element_copy_connmgr(void * element);
void element_free_connmgr(void ** element);
int element_compare_connmgr(void * x, void * y);


#endif //__CONNMGR_H__
