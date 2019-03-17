#define  _GNU_SOURCE
#include<stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include"datamgr.h"
#include"lib/dplist.h"



#define ERROR_INVALID_SENSOR  "sensor id  is invalid"



typedef uint16_t room_id_t;


 struct sensor_room{
	sensor_id_t sensor_id;
	room_id_t room_id;
	sensor_value_t running_avg;
	int num_of_sensor_data;
	double latest_data[RUN_AVG_LENGTH];
        time_t last_modified;
};


//callback function for the dplist
void* element_copy(void * src_element)
{
	sensor_room_t *copy_element= malloc(sizeof(sensor_room_t));
	//check if it malloc successfully
	assert(copy_element != NULL);
	*copy_element = *(sensor_room_t *)(src_element);
	return (void *)copy_element;
}

//callback function for the dplist
void element_free(void ** element)
{
    free(*element);
}

//callback function for the dplist
int element_compare(void * x, void * y)
{
    if(*(uint16_t*)x>*(uint16_t*)y)
      return 1;
    if(*(uint16_t*)x==*(uint16_t*)y)
      return 0;
    else
      return -1;
}

//initialise a dplist 
dplist_t * list_sensor_room=NULL;

// map room with its sensor and put the struct in a dplist
void map_room_with_sensor(FILE * fp_sensor_map)
{    //initialise the list_sensor_room
     list_sensor_room=dpl_create(&element_copy,&element_free,&element_compare);
     if(list_sensor_room!=NULL)printf("list created \n");
     //malloc a memory for later use
     sensor_room_t * sensor_room=malloc(sizeof(sensor_room_t));
     
     room_id_t room_id;
     sensor_id_t sensor_id;
     
     //read from file until the end of file(EOF) in the format of uint16_t
     // map data and put into a list
     //reference of the format specifier of uint16_t:https://debrouxl.github.io/gcc4ti/inttypes.html 
    while(fscanf(fp_sensor_map,"%"SCNu16 "%"SCNu16 ,&room_id,&sensor_id) != EOF)
    {
        sensor_room->sensor_id=sensor_id;
        sensor_room->room_id=room_id;
        //initialise left attributes
        sensor_room->num_of_sensor_data=0;
        sensor_room->running_avg=0;
        sensor_room->last_modified=0;
        for(int i=0;i<RUN_AVG_LENGTH;i++)
        {
            sensor_room->latest_data[i]=0;
			
        }
        //insert always at the head of the list and make a copy of the struct
       list_sensor_room=dpl_insert_at_index( list_sensor_room, sensor_room, 0, true); 
    }
    printf("finish mapping\n");
    //free the memory 
    free(sensor_room);
    sensor_room=NULL;
    //return list_sensor_room;
    
}

void datamgr_parse_sensor_data(FILE * fp_sensor_map, sbuffer_t ** buffer)
{
    //only do the mapping for the first time
    if(list_sensor_room==NULL)
    {
        map_room_with_sensor(fp_sensor_map);
    }
    sensor_data_t sensor_data;
    int result=sbuffer_remove(*buffer,&sensor_data);
    if(result==SBUFFER_SUCCESS)
    {
        printf("datamgr get data\n");
        printf(" from datamgr sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", sensor_data.id, sensor_data.value, (long int)sensor_data.ts);
    }
    int isFind=0;
    if(result==SBUFFER_SUCCESS)
    {
                //iterate the list to compare the sensor id with already existing ones
        for(int i=0; i<dpl_size(list_sensor_room);i++)
        {
            
            //if the data matches the id of the sensor for some room
            sensor_room_t *dummy=(sensor_room_t *)dpl_get_element_at_index(list_sensor_room,i);
            if(dummy==NULL) printf("NULL pointer\n");
            if(sensor_data.id==dummy->sensor_id)
            {
                printf("find sensor\n");
                isFind=1;
                dummy->num_of_sensor_data++;
                int index=( dummy->num_of_sensor_data)%RUN_AVG_LENGTH;
                dummy->latest_data[index]=sensor_data.value;
                dummy->last_modified=sensor_data.ts;
                //if there are more than RUN_AVG_LENGTH data that has been read into list_sensor_room
                if(dummy->num_of_sensor_data>=RUN_AVG_LENGTH)
                {
                    sensor_value_t sum=0;
                    for(int j=0;j<RUN_AVG_LENGTH;j++)
                    {
                       sum=sum+dummy->latest_data[j];
                    }
                    dummy->running_avg=sum/RUN_AVG_LENGTH;
                
                   //if the running_avg exceeds the range
                   if((dummy->running_avg)< SET_MIN_TEMP)
                    { 
                        char *logmsg;
                        asprintf(&logmsg,"%ld the sensor node with %" PRIu16" reports it's too cold (running avg temperature= %lf)\n",time(0),sensor_data.id,dummy->running_avg);
                        write_to_fifo(logmsg);
                        free(logmsg);                
                    }
                    if((dummy->running_avg) > SET_MAX_TEMP)
                   {
                        char *logmsg;
                        asprintf(&logmsg,"%ld the sensor node with %" PRIu16" reports it's too hot (running avg temperature= %lf)\n",time(0),sensor_data.id,dummy->running_avg);
                        write_to_fifo(logmsg);
                        free(logmsg);   
                        
                    }
                }  
                //find the room_sensor, break     
                break;
                    
            }
        }
        if(isFind==0)
        {
            char *logmsg;
            asprintf(&logmsg,"%ld Received sensor data with invalid node ID  %" PRIu16"\n",time(0),sensor_data.id);
            write_to_fifo(logmsg);
            free(logmsg);   
        }
    }

}

/*
void datamgr_parse_sensor_files(FILE * fp_sensor_map, FILE * fp_sensor_data)
{
    map_room_with_sensor(fp_sensor_map);
    
    time_t timestamp;
    sensor_id_t temp_id_sensor;
    double temp_data_sensor;
    
    //reference: from man page 
    //The function feof() tests the  end-of-file  indicator  for  the  stream
    // pointed  to by stream, returning nonzero if it is set.  The end-of-file
    // indicator can be cleared only by the function clearerr().
    int num_hot=0;
    int num_cold=0;
    while(!feof(fp_sensor_data))
    {   //reference : man page
        //size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
        //read three data for each sensor_data
        fread(&temp_id_sensor,sizeof(temp_id_sensor),1,fp_sensor_data);
        fread(&temp_data_sensor,sizeof(temp_data_sensor),1,fp_sensor_data);
        //printf("data is %lf\n",temp_data_sensor);
        fread(&timestamp,sizeof(timestamp),1,fp_sensor_data);
        
        //iterate the list to compare the sensor id with already existing ones
        for(int i=0; i<dpl_size(list_sensor_room);i++)
        {
            
            //if the data matches the id of the sensor for some room
            sensor_room_t *dummy=(sensor_room_t *)dpl_get_element_at_index(list_sensor_room,i);
            if(dummy==NULL) printf("NULL pointer\n");
            if(temp_id_sensor==dummy->sensor_id)
            {
                dummy->num_of_sensor_data++;
                int index=( dummy->num_of_sensor_data)%RUN_AVG_LENGTH;
                dummy->latest_data[index]=temp_data_sensor;
                dummy->last_modified=timestamp;
                //if there are more than RUN_AVG_LENGTH data that has been read into list_sensor_room
                if(dummy->num_of_sensor_data>=RUN_AVG_LENGTH)
                {
                    sensor_value_t sum=0;
                    for(int j=0;j<RUN_AVG_LENGTH;j++)
                    {
                       sum=sum+dummy->latest_data[j];
                    }
                    dummy->running_avg=sum/RUN_AVG_LENGTH;
                
                   //if the running_avg exceeds the range
                   if((dummy->running_avg)< SET_MIN_TEMP)
                    { 
                        num_cold++;
                        fprintf(stderr,"Room " "%" SCNu16 " too cold !" "\n", dummy->room_id); 	
                        printf("the temp is %lf\n",dummy->running_avg);
                    }
                    if((dummy->running_avg) > SET_MAX_TEMP)
                   {
                       num_hot++;
                       fprintf(stderr,"Room " "%" SCNu16 " too hot !" "\n", dummy->room_id); 
                       printf("the hot temp is %lf\n",dummy->running_avg);
                        
                    }
                }  
                //find the room_sensor, break     
                break;
                    
            }
        }
    }
    
    printf("number of too hot is %d\n",num_hot);
    printf("number of too cold is %d\n",num_cold);
}
*/

void datamgr_free()
{  //call the function to free the list,free the copy_element
   dpl_free(&list_sensor_room,true); 
}


uint16_t datamgr_get_room_id(sensor_id_t sensor_id)
{
    //get the reference of the sensor_room_t
    sensor_room_t * reference;
    reference=get_reference_at_sensor_id(sensor_id);
    //if reference==NULL, invalid sensor id
    ERROR_HANDLER(reference==NULL,ERROR_INVALID_SENSOR);
    
    return reference->room_id;
}


sensor_value_t datamgr_get_avg(sensor_id_t sensor_id)
{
    sensor_room_t * ref=get_reference_at_sensor_id(sensor_id);
    //if reference==NULL, invalid sensor id
    ERROR_HANDLER(ref==NULL,ERROR_INVALID_SENSOR);
    return ref->running_avg;

}

sensor_room_t* get_reference_at_sensor_id(sensor_id_t sensor_id)
{
    sensor_room_t * dummy=NULL;
    int size=dpl_size(list_sensor_room);
    //int i;
    for(int i=0;i<size;i++)
    {
        dummy=(sensor_room_t *)dpl_get_element_at_index(list_sensor_room,i);
        if(dummy->sensor_id==sensor_id)
        {
            return dummy;
        }
    }
    
    return dummy;
}



time_t datamgr_get_last_modified(sensor_id_t sensor_id)
{
    sensor_room_t *ref=get_reference_at_sensor_id(sensor_id);
    //if reference==NULL, invalid sensor id
    ERROR_HANDLER(ref==NULL,ERROR_INVALID_SENSOR);
    return ref->last_modified;
}

int datamgr_get_total_sensors()
{
    return dpl_size(list_sensor_room);
}

