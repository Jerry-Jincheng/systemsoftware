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
  list = malloc(sizeof(dplist_t));
  DPLIST_ERR_HANDLER(list==NULL,DPLIST_MEMORY_ERROR);
  list->head = NULL;  
  list->element_copy = element_copy;
  list->element_free = element_free;
  list->element_compare = element_compare; 
  printf("create success\n");
  return list;
}

void dpl_free(dplist_t ** list, bool free_element)
{
    // add your code here
    assert(*list!=NULL);

    while ((*list)->head!=NULL) {
        dpl_remove_at_index(*list,0,free_element);
    }
    free(*list);
    *list=NULL;
}

dplist_t * dpl_insert_at_index(dplist_t * list, void * element, int index, bool insert_copy)
{
    // malloc a memory for list node
    dplist_node_t * node=(dplist_node_t *)malloc(sizeof(dplist_node_t));
    //error handling
    DPLIST_ERR_HANDLER(node==NULL,DPLIST_INVALID_ERROR);
    
    if(insert_copy==true)
    {
        node->element=list->element_copy(element);
    }
    else
    {
        node->element=element;
    }
    
    //if the list is empty
    if(list->head==NULL)
    {
        node->next=NULL;
        node->prev=NULL;
        list->head=node;
        printf("\ninsert the first element\n");
    }
    
    else if(index<=0)
    {
        node->next=list->head;
        node->prev=NULL;
        list->head->prev=node;
        list->head=node;
        printf("insert at the head and index is %d\n",index);
    }
    //insert at the last node
    else if(index>=dpl_size(list)-1)
    {
        printf("index is %d and size of list is %d\n",index,dpl_size(list));
        dplist_node_t *reference=dpl_get_last_reference(list);

        reference->next=node;
        node->prev=reference;
        node->next=NULL;
        printf("insert at the end\n");
        
    }
    //insert in the middle
    else
    {
        dplist_node_t * reference=dpl_get_reference_at_index(list,index);
        node->prev=reference->prev;
        node->next=reference;
        reference->prev->next=node;
        reference->prev=node;
        printf("insert in the middle\n");
    }
    return list;
}

dplist_t * dpl_remove_at_index( dplist_t * list, int index, bool free_element)
{
    printf("removing element\n");
    // add your code here
    if(list->head!=NULL)
    {
        //get reference of the node to remove
        dplist_node_t *reference;
        //if reference is the first node in the list
        if(dpl_size(list)==1)
        {
            reference=list->head;
            list->head=NULL;
            printf("removing, this is the last one\n");
        }
        else if(index<=0)
        {
            //dplist_node_t *reference=dpl_get_reference_at_index(list,index);
            reference=list->head;
            reference->next->prev=NULL;
            list->head=reference->next;
            reference->next=NULL;
            printf("removing node at the head, the index is %d \n",index);
        }
        //if the reference is last node in the list
        else if(index>=dpl_size(list)-1)
        {
            reference=dpl_get_reference_at_index(list,(dpl_size(list)-1));
            reference->prev->next=NULL;
            reference->prev=NULL;
        }
        //if the reference is node in the middle
        else
        {
            reference=dpl_get_reference_at_index(list,index);          
            reference->prev->next=reference->next;
            reference->next->prev=reference->prev;
            reference->prev=NULL;
            reference->next=NULL;
        }
        
        //free the memory
        if(free_element==true)
        {
            list->element_free(&(reference->element));
            free(reference);
            printf("free the copied memory\n");
            reference=NULL;
        }
        else
        {
            reference->element=NULL;
            free(reference);
            printf("free the memory\n");
            reference=NULL;
        }
         printf("the size now is %d\n",dpl_size(list));
         return list;
    }
    
    printf("no element to remove\n");
    return list;
}

int dpl_size( dplist_t * list )
{
    // add your code here
    dplist_node_t *cur_node=list->head;
    int size = 0;
    while (cur_node!=NULL) {
        size++;
        cur_node = cur_node->next;
    }
    return size;
}

dplist_node_t * dpl_get_reference_at_index( dplist_t * list, int index )
{
    // add your code here
    dplist_node_t *reference= list->head;
    //if(reference==NULL)  return NULL;
    if(reference==NULL||index<=0)
    {
        return reference;
    }

    else
    {   
         //if the index is out of range, return the last node as reference
         if(index>=dpl_size(list)-1)
         {
              index=dpl_size(list)-1;
         }
         
        for(int i=0;i<index;i++,reference=reference->next){}
        return reference;
    }
}

void * dpl_get_element_at_index( dplist_t * list, int index )
{
    // add your code here
    
    dplist_node_t *reference= list->head;
    if(reference==NULL)
    {
        return (void *)0;
    }

    else if(index<=0)
    {
        return reference->element;
    }

    else
    {
        //if the index is out of range, return the last node as reference
        if(index>=dpl_size(list)-1)index=dpl_size(list)-1;
        for(int i=0;i<index;i++,reference=reference->next){}
        return reference->element;
    }
    
}

int dpl_get_index_of_element( dplist_t * list, void * element )
{
    // add your code here
    dplist_node_t *dummy=list->head;
    if(dummy==NULL)return -1;
    else
    {
        int index=0;
        while(dummy!=NULL){
        if(list->element_compare(dummy->element,element)==0)
            return index;
        dummy=dummy->next;
        index++;
        }
        return -1;
    }
}

// HERE STARTS THE EXTRA SET OF OPERATORS //

// ---- list navigation operators ----//
  
dplist_node_t * dpl_get_first_reference( dplist_t * list )
{
    // add your code here
    return list->head;
}

dplist_node_t * dpl_get_last_reference( dplist_t * list )
{
    // add your code here
    if(list==NULL||list->head==NULL)return NULL;
    return dpl_get_reference_at_index(list,dpl_size(list)-1);
}

dplist_node_t * dpl_get_next_reference( dplist_t * list, dplist_node_t * reference )
{
    // add your code here
    //if list is empty
    if(list->head==NULL) return NULL;
    //if the reference is NULL, return NULL
    if(reference==NULL) return NULL;
    //if the reference is not existing reference in the list
    if(dpl_get_index_of_reference(list,reference)==-1) return NULL;
    //else
    return reference->next;
}

dplist_node_t * dpl_get_previous_reference( dplist_t * list, dplist_node_t * reference )
{
    //if list is empty
    if(list->head==NULL) return NULL;
    //if the reference is NULL, return NULL
    if(reference==NULL) return dpl_get_last_reference(list);
    //if the reference is not existing reference in the list
    if(dpl_get_index_of_reference(list,reference)==-1) return NULL;
    //else
    return reference->prev;
}

// ---- search & find operators ----//  
  
void * dpl_get_element_at_reference( dplist_t * list, dplist_node_t * reference )
{
    //if the list is empty
    if(list->head==NULL)return NULL;
    //if the reference is NULL, get the last element in the list
    if(reference==NULL) return dpl_get_last_reference(list)->element;
    //if reference is  not a reference in the list,return NULL
    if(dpl_get_index_of_reference(list,reference)==-1) return NULL;
    //else
    return reference->element;
}

dplist_node_t * dpl_get_reference_of_element( dplist_t * list, void * element )
{
    // if the list is empty, return NULL
    if(list->head==NULL) return NULL;
    if(dpl_get_index_of_element(list,element)==-1) return NULL;
    else
    {
        int index=dpl_get_index_of_element(list,element);
        return dpl_get_reference_at_index(list,index);
    }
    
}

int dpl_get_index_of_reference( dplist_t * list, dplist_node_t * reference )
{   //if the list is empty
    if(dpl_size(list)==0)return -1;
    //if the reference is NULL, return the index of last element
    if(reference==NULL) return dpl_size(list)-1;
    
    //if the reference is not NULL
    for(int index=0;index<dpl_size(list);index++)
    {
        if(reference==dpl_get_reference_at_index(list,index))
            return index;
    }
    //if the reference is not an reference of existing node in the list, return -1
    return -1;
}
  
// ---- extra insert & remove operators ----//

dplist_t * dpl_insert_at_reference( dplist_t * list, void * element, dplist_node_t * reference, bool insert_copy )
{
    int index_of_ref=dpl_get_index_of_reference(list,reference);
    //if reference is not existing reference of list 
    if(index_of_ref==-1)return list;
    else
    {
        return dpl_insert_at_index(list,element,index_of_ref,insert_copy);
    }
}

dplist_t * dpl_insert_sorted( dplist_t * list, void * element, bool insert_copy )
{
    // if the list is empty
    if(list->head==NULL)
    {
        return dpl_insert_at_index(list,element,0,insert_copy);
    }
    else
    {   
        dplist_node_t *dummy=list->head;
        int index=0;
        while(dummy!=NULL)
        {
            if(list->element_compare(dummy->element,element)==1)
            {
                    return dpl_insert_at_index(list,element,index,insert_copy);
            }
            index++;
            dummy=dummy->next;
        }
        return dpl_insert_at_index(list,element,index,insert_copy);
      
    }
}

dplist_t * dpl_remove_at_reference( dplist_t * list, dplist_node_t * reference, bool free_element )
{
    DPLIST_ERR_HANDLER(list==NULL,DPLIST_INVALID_ERROR);
    // if list is empty or the reference is not in the list
    if(list->head==NULL||dpl_get_index_of_reference(list,reference)==-1)
        return list;
    else{
        int index=dpl_get_index_of_reference(list,reference);
        dpl_remove_at_index(list,index ,free_element);

        return list;
    }
}

dplist_t * dpl_remove_element( dplist_t * list, void * element, bool free_element )
{
    int index_of_element=dpl_get_index_of_element(list,element);
    if(list->head==NULL||index_of_element==-1)return list;
    else{
        printf("the size is %d and head is%p\n",dpl_size(list),list->head);
        dpl_remove_at_index(list,index_of_element,free_element);
        return list;
    }
}
  
// ---- you can add your extra operators here ----//



