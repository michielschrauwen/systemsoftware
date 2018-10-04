#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list 

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


#define DPLIST_ERR_HANDLER(condition,err_code)\
	do {						            \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");    \
            assert(!(condition));                                    \
        } while(0)

        
/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
  dplist_node_t * prev, * next;
  void * element;
};

struct dplist {
  dplist_node_t * head;
  void * (*element_copy)(void * src_element);			  
  void (*element_free)(void ** element);
  int (*element_compare)(void * x, void * y);
};


dplist_t * dpl_create (// callback functions
			  void * (*element_copy)(void * src_element),
			  void (*element_free)(void ** element),
			  int (*element_compare)(void * x, void * y)
			  )
{
  dplist_t * list;
  list = malloc(sizeof(struct dplist));
  DPLIST_ERR_HANDLER(list==NULL,DPLIST_MEMORY_ERROR);
  list->head = NULL;  
  list->element_copy = element_copy;
  list->element_free = element_free;
  list->element_compare = element_compare; 
  return list;
}

void dpl_free(dplist_t ** list, bool free_element)
{
    DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
    
    dplist_t * dummy_list = *list;
    dplist_node_t * this_node;
    dplist_node_t * next_node;
    
    this_node = dummy_list -> head;
    while(this_node != NULL)	{
		if(free_element)	{
			dummy_list -> element_free(&(this_node -> element));	//free element if needed
		}
		next_node = this_node -> next;
		free(this_node);	//free every reference
		this_node = next_node;		
	}
	free(*list);
	*list = NULL;
	printf("dpl_free of linked list complete\n");
}

dplist_t * dpl_insert_at_index(dplist_t * list, void * element, int index, bool insert_copy)
{
	dplist_node_t * ref_at_index, * list_node;
	DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
	DPLIST_ERR_HANDLER(element==NULL,DPLIST_INVALID_ERROR);
	
	list_node = malloc(sizeof(dplist_node_t));
	DPLIST_ERR_HANDLER(list_node==NULL,DPLIST_MEMORY_ERROR);
	
	if(insert_copy)	{
		list_node -> element = list -> element_copy(element);
	}	else
	{
		list_node -> element = element;
	}
	
	// pointer drawing breakpoint
	if (list->head == NULL)  
	{ // covers case 1 
		list_node->prev = NULL;
		list_node->next = NULL;
		list->head = list_node;
		// pointer drawing breakpoint
	} else if (index <= 0)  
	{ // covers case 2 
		list_node->prev = NULL;
		list_node->next = list->head;
		list->head->prev = list_node;
		list->head = list_node;
		// pointer drawing breakpoint
	} else 
	{
		ref_at_index = dpl_get_reference_at_index(list, index);  
		assert( ref_at_index != NULL);
		// pointer drawing breakpoint
		if (index < dpl_size(list))
		{ // covers case 4
			list_node->prev = ref_at_index->prev;
			list_node->next = ref_at_index;
			ref_at_index->prev->next = list_node;
			ref_at_index->prev = list_node;
		  // pointer drawing breakpoint
		} else
		{ // covers case 3 
			assert(ref_at_index->next == NULL);
			list_node->next = NULL;
			list_node->prev = ref_at_index;
			ref_at_index->next = list_node;    
		  // pointer drawing breakpoint
		}
	}
	return list;
}

dplist_t * dpl_remove_at_index( dplist_t * list, int index, bool free_element)
{
	dplist_node_t * dummy;
	DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
	if(list -> head == NULL)	{
		return list;
	}
	
	if(dpl_size(list) > 1)	//multiple elements in list
	{
		if(index <= 0)	{	//case begin
			dummy = dpl_get_reference_at_index(list, 0);
			if(dummy -> next != NULL)	{
				list -> head = dummy -> next;
				dummy -> next -> prev = NULL;
			}
			else	//case als er maar 1 element in de lijst is en deze wordt verwijderd
			{
				list -> head = NULL;
			}
		}
		else
		{
			dummy = dpl_get_reference_at_index(list, index);
			if(dummy -> next != NULL)	{	//case middle of list
				dummy -> next -> prev = dummy -> prev;
				dummy -> prev -> next = dummy -> next;
			}
			else 	//case end of ist
			{
				dummy -> prev -> next = NULL;
			}
		}
	}
	else
	{
		dummy = dpl_get_reference_at_index(list, 0);
		list -> head = NULL;
	}
	
	
	if(free_element)	{
		list -> element_free(&(dummy -> element));
	}
	free(dummy);
	return list;
}

int dpl_size( dplist_t * list )
{
	int count = 0;
	dplist_node_t * dummy;
	if(list -> head == NULL)	{
		return count;
	}
	dummy = list -> head;
	count++;
	while(dummy -> next != NULL)	{		
		dummy = dummy -> next;
		count++;
	}
	return count;
}

dplist_node_t * dpl_get_reference_at_index( dplist_t * list, int index )
{
	int count;
	dplist_node_t * dummy;
	DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
	if (list->head == NULL ){
		return NULL;
	}
	for ( dummy = list->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++) 
	{ 
		if (count >= index) return dummy;
	}  
	return dummy;  
}

void * dpl_get_element_at_index( dplist_t * list, int index )
{
	int count;
	dplist_node_t * dummy;
	DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
	if(list -> head == NULL)	{
		return NULL;
	}
	for ( dummy = list->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++) 
	{
		if (count >= index) return dummy -> element;
	}  
	return dummy -> element; 
}

int dpl_get_index_of_element( dplist_t * list, void * element )
{
	int index = 0;
	dplist_node_t * dummy;
	DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
	if(list -> head == NULL)	{
		return -1;
	}
	dummy = list -> head;
	while(dummy -> next != NULL)	{
		if(list -> element_compare(dummy -> element, element) == 0)	{
			return index;
		}
		dummy = dummy -> next;
		index++;
	}
	if(list -> element_compare(dummy -> element, element) == 0)	{	//last element
			return index;
	}
	return -1;
}

// HERE STARTS THE EXTRA SET OF OPERATORS //

// ---- list navigation operators ----//
  
dplist_node_t * dpl_get_first_reference( dplist_t * list )
{
	DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
	if(list -> head == NULL)	{
		return NULL;
	}	
	return list -> head;
}

dplist_node_t * dpl_get_last_reference( dplist_t * list )
{
	DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
	if(list -> head == NULL)	{
		return NULL;
	}
    return dpl_get_reference_at_index(list, dpl_size(list)-1);
}

dplist_node_t * dpl_get_next_reference( dplist_t * list, dplist_node_t * reference )
{
	DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
	if(list -> head == NULL || reference == NULL)	{
		return NULL;
	}
	dplist_node_t * dummy;
	dummy = list -> head;
	while(dummy -> next != NULL)	{
		if(dummy == reference)	{
			return dummy -> next;
		}
		dummy = dummy -> next;
	}
	return NULL;
}

dplist_node_t * dpl_get_previous_reference( dplist_t * list, dplist_node_t * reference )
{
    DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
	if(list -> head == NULL)	{
		return NULL;
	}
	if(reference == NULL)	{
		return dpl_get_reference_at_index(list, dpl_size(list)-1);
	}
	dplist_node_t * dummy;
	dummy = list -> head;
	while(dummy -> next != NULL)	{
		if(dummy == reference)	{
			return dummy -> prev;
		}
		dummy = dummy -> next;
	}
	if(dummy == reference)	{
		return dummy -> prev;
	}
	return NULL;
}

// ---- search & find operators ----//  
  
void * dpl_get_element_at_reference( dplist_t * list, dplist_node_t * reference )
{
    DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
    if(list -> head == NULL)	{
		return NULL;
	}
	if(reference == NULL)	{
		return dpl_get_element_at_index(list, dpl_size(list)-1);
	}
	dplist_node_t * dummy;
	dummy = list -> head;
	while(dummy -> next != NULL)	{
		if(dummy == reference)	{
			return dummy -> element;
		}
		dummy = dummy -> next;
	}
	if(dummy == reference)	{
		return dummy -> element;
	}
	return NULL;
}

dplist_node_t * dpl_get_reference_of_element( dplist_t * list, void * element )
{
    DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
    if(list -> head == NULL)	{
		return NULL;
	}
	int index = dpl_get_index_of_element(list, element);
	if(index == -1)	{	//element not found
		return NULL;
	}
	return dpl_get_reference_at_index(list, index);
	
}

int dpl_get_index_of_reference( dplist_t * list, dplist_node_t * reference )
{
	dplist_node_t * dummy;
	int index = 0;
    DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
    if(list -> head == NULL)	{
		return -1;
	}
	if(reference == NULL)	{
		return dpl_size(list) - 1;
	}
	dummy = list -> head;
	while(dummy -> next != NULL)	{
		if(dummy == reference)	{
			return index;
		}
		dummy = dummy -> next;
		index++;
	}
	if(dummy == reference)	{
		return index;
	}
	return -1;
}
  
// ---- extra insert & remove operators ----//

dplist_t * dpl_insert_at_reference( dplist_t * list, void * element, dplist_node_t * reference, bool insert_copy ) //
{
	DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
	if(reference == NULL)	{
		return dpl_insert_at_index(list, element, dpl_size(list), insert_copy);
	}
	int index = dpl_get_index_of_reference(list, reference);
	
	if(index != -1)	{	//index is -1 if reference doesnt exist
		return dpl_insert_at_index(list, element, index, insert_copy);
	}
	return list;
}

dplist_t * dpl_insert_sorted( dplist_t * list, void * element, bool insert_copy )
{
	DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
	
	if(list -> head == NULL)	{
		return dpl_insert_at_index(list, element, 0, insert_copy);
	}	
	
	/*
	dplist_node_t * this_node;
	dplist_node_t * compare_node;
	
	new_list -> head = list -> head;
	this_node = new_list -> head;
	bool found = false;
	
	while(this_node != NULL)	{		
		compare_node = new_list -> head;
		while(found = false)	{			
			if(new_list -> element_compare(this_node, compare_node) == -1)	{
				dpl_insert_at_index(new_list, this_node -> element, dpl_get_index_of_reference(new_list, compare_node), false);	//check insert copy true or false? free??
				found = true;
			}
			if(compare_node -> next != NULL)	{
				compare_node = compare_node -> next;	//if compare_node last of new_list and found false dan erachter zetten
			}
			dpl_insert_at_index(new_list, this_node -> element, dpl_size(new_list), false);
			found = true;
		}
		this_node = this_node -> next;
	}	//free old list?????????
	*/
	
	//source bubble sort: https://www.geeksforgeeks.org/c-program-bubble-sort-linked-list/
	dplist_node_t * dummy;
	dplist_node_t * ldummy = NULL;
	int swapped;
	void * temp_element;
	
	do
	{
		swapped = 0;
		dummy = list -> head;
		
		while (dummy -> next != ldummy)	{
			if((list -> element_compare(dummy -> element, dummy -> next -> element)) == 1)
			{//swap the elements
				temp_element = dummy -> element;
				dummy -> element = dummy -> next -> element;
				dummy -> next -> element = temp_element;
				swapped = 1;
			}
			dummy = dummy -> next;
		}
		ldummy = dummy;
	}
	while(swapped);
	
	
	int index = 0;
	dummy = list -> head;
		
	while(dummy != NULL)	//insert new element
	{
		if((list -> element_compare(dummy -> element, element)) == 1)
		{
			return dpl_insert_at_index(list, element, index, insert_copy);
		}
		index++;
		if(dummy -> next != NULL)	{
			dummy = dummy -> next;
		}
		else
		{
			return dpl_insert_at_index(list, element, index, insert_copy);
		}
		
	}
	return list;
	
}

dplist_t * dpl_remove_at_reference( dplist_t * list, dplist_node_t * reference, bool free_element )
{
	DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
	
	if(list -> head == NULL)	{
		return list;
	}	
	if(reference == NULL)	{
		return dpl_remove_at_index(list, dpl_size(list)-1, free_element);
	}
		
	int index = dpl_get_index_of_reference(list, reference);
	
	if(index != -1)	{
		return dpl_remove_at_index(list, index, free_element);
	}
	return list;
}

dplist_t * dpl_remove_element( dplist_t * list, void * element, bool free_element )
{
    DPLIST_ERR_HANDLER(list==NULL, DPLIST_INVALID_ERROR);
    
    if(list -> head == NULL)	{
		return list;
	}
	
	int index = dpl_get_index_of_element(list, element);
	
	if(index != -1)	{
		return dpl_remove_at_index(list, index, free_element);
	}
	return list;
}
  
// ---- you can add your extra operators here ----//



