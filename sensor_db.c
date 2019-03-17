#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "config.h"
#include <sqlite3.h>
#include "sensor_db.h"
#include <time.h>
#include "sbuffer.h"

/*
 * 
 * only use asprintf and sqlite3_exec
 * 
 */

DBCONN * init_connection(char clear_up_flag)
{
    //reference: zetcode.com/db/sqlitec 
    DBCONN *db;
    char* errmsg=0;
    char *logmsg;
    int rc=sqlite3_open(TO_STRING(DB_NAME),&db);
    
    if(rc!=SQLITE_OK)
    {
        
        asprintf(&logmsg,"%ld unable to connect to SQL server",time(0));
        write_to_fifo(logmsg);
        free(logmsg);
        sqlite3_close(db);
        return NULL;
    }
    //connect success send message to write_to_fifo()
    //char *logmsg;
    printf("hello,sql\n");
     asprintf(&logmsg,"%ld Connection to SQL server established\n",time(0));
     write_to_fifo(logmsg);
     free(logmsg);
    printf("finish first fifo\n");
    
    if(clear_up_flag==1)
    {
        char *sql;
        asprintf(&sql,"DROP TABLE IF EXISTS %s",TO_STRING(TABLE_NAME));
        rc=sqlite3_exec(db,sql,0,0,&errmsg);
        //if failed to delete the database, log the error
        if(rc!=SQLITE_OK)
        {
            fprintf(stderr,"failed to delete database: %s\n",errmsg);
            free(errmsg);
        }
        free(sql);
    }

     char *sql1;
     asprintf(&sql1,"CREATE TABLE IF NOT EXISTS %s (id INTEGER PRIMARY KEY, sensor_id INT,sensor_value DECIMAL(4,2), timestamp TIMESTAMP )",TO_STRING(TABLE_NAME));
     rc=sqlite3_exec(db,sql1,0,0,&errmsg);
     if(rc!=SQLITE_OK)
        {
            fprintf(stderr,"failed to create table: %s %s\n",errmsg,sql1);
            sqlite3_free(errmsg);
            return NULL;
        }
        sqlite3_free(errmsg);
        free(sql1);
    //char *logmsg;
    asprintf(&logmsg,"%ld New table %s created\n",time(0),TO_STRING(TABLE_NAME));
    write_to_fifo(logmsg);
    free(logmsg);
    
    return db;
}

void disconnect(DBCONN *conn)
{
    if(conn!=NULL)
    {  
        //close the connection 
        sqlite3_close(conn);
        char *logmsg;
        asprintf(&logmsg,"%ld Connection to SQL server lost",time(0));
        write_to_fifo(logmsg);
        free(logmsg);
        
    }
}

int insert_sensor(DBCONN * conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts)
{
    char *errmsg;
    char *query;
    asprintf(&query,"INSERT INTO %s (sensor_id,sensor_value,timestamp) VALUES (%d,%lf,%ld)",TO_STRING(TABLE_NAME),id,value,ts) ;
    int rc=sqlite3_exec(conn,query,0,0,&errmsg);
    if(rc!=SQLITE_OK)
        {    //log the error message,store in errmsg
            fprintf(stderr,"failed to execute query: %s\n",query);
            sqlite3_free(errmsg);
            return -1;
        }
    free(query);
        return 0;
    
}

void storagemgr_parse_sensor_data(DBCONN * conn, sbuffer_t ** buffer)
{
    
    sensor_data_t sensor_data;
    char *query;
    int result=sbuffer_remove(*buffer,&sensor_data);
    if(result==SBUFFER_SUCCESS)
    {
        printf("sensor db get data\n");
        printf(" from sensor_db sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", sensor_data.id, sensor_data.value, (long int)sensor_data.ts);
    
   
    asprintf(&query,"INSERT INTO %s (sensor_id,sensor_value,timestamp) VALUES (%d,%lf,%ld)",TO_STRING(TABLE_NAME),sensor_data.id,sensor_data.value,sensor_data.ts) ;
    sqlite3_exec(conn,query,0,0,NULL);
        free(query); 
    }
//     if(rc!=SQLITE_OK)
//     {    //log the error message,store in errmsg
//         fprintf(stderr,"failed to execute query: %s\n",query);
//         sqlite3_free(errmsg);
//         //return -1;
//     }

    printf("sensor db success\n");
    
}

/*
int insert_sensor_from_file(DBCONN * conn, FILE * sensor_data)
{
    char *query;
    char* errmsg=0;
    //sqlite3_stmt *result;
    sensor_ts_t ts;
    sensor_value_t data;
    sensor_id_t sensor_id;
    while(!feof(sensor_data))
    {
        fread(&sensor_id,sizeof(sensor_id_t),1,sensor_data);
        fread(&data,sizeof(sensor_value_t),1,sensor_data);
        fread(&ts,sizeof(sensor_ts_t),1,sensor_data);
        if(ts)
        {
        asprintf(&query,"INSERT INTO %s (sensor_id,sensor_value,timestamp) VALUES (%d,%lf,%ld)",TO_STRING(TABLE_NAME),sensor_id,data,ts) ;
        //printf("%s\n",query);

        int rc=sqlite3_exec(conn,query,0,0,&errmsg);
        if(rc!=SQLITE_OK)
        {    //log the error message,store in errmsg
            fprintf(stderr,"failed to execute query: %s\n",query);
            sqlite3_free(errmsg);
            return -1;
        }
        free(query); 
        }
        
    }
    return 0;
}
*/
int execute_query_with_callback(DBCONN * conn, char *query, callback_t f)
{
    
        char* errmsg=0; 
        if(conn==NULL)
        {
            fprintf(stderr,"invalid database connection\n");
            return -1;
        }
        //char *query="SELECT *FROM TABLE_NAME";
        int rc=sqlite3_exec(conn,query,f,0,&errmsg);
        if(rc!=SQLITE_OK)
        {    //log the error message,store in errmsg
            fprintf(stderr,"failed to execute query: %s\n",query);
            sqlite3_free(errmsg);
            return -1;
        }
       // free(query);
        return 0;
}

int find_sensor_all(DBCONN * conn, callback_t f)
{
        char *query;
        asprintf(&query,"SELECT *FROM  %s",TO_STRING(TABLE_NAME));
        int rc =execute_query_with_callback(conn,query,f);
        
        free(query);
        //if returned code is -1 , error occurred 
        if(rc==-1)
        {
            fprintf(stderr,"failed to find all the sensor \n");
            return -1;
        }
        
        return 0;
}

int find_sensor_by_value(DBCONN * conn, sensor_value_t value, callback_t f)
{
        char *query;
        //bind the parameter
        asprintf(&query,"SELECT * FROM  %s WHERE sensor_value= %f",TO_STRING(TABLE_NAME),value);
        int rc =execute_query_with_callback(conn,query,f);
        //if returned code is -1 , error occurred 
        free(query);
        if(rc==-1)
        {
            fprintf(stderr,"failed to find sensor with value of: %lf\n",value);
            return -1;
        }
        
        return 0;
  
       
}

int find_sensor_exceed_value(DBCONN * conn, sensor_value_t value, callback_t f)
{
    
        char *query;
        //connection is ok prepare the statement
        asprintf(&query,"SELECT * FROM  %s WHERE sensor_value > %f",TO_STRING(TABLE_NAME),value);
        int rc =execute_query_with_callback(conn,query,f);
        free(query);
        //if returned code is -1 , error occurred 
        if(rc==-1)
        {
            fprintf(stderr,"failed to find sensor with value exceeding: %lf\n",value);
            return -1;
        }
        
        return 0;
}

int find_sensor_by_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f)
{
        char *query;
        //connection is ok prepare the statement
        
        //bind the parameter
        asprintf(&query,"SELECT * FROM %s WHERE timestamp=%ld",TO_STRING(TABLE_NAME),ts);
        int rc =execute_query_with_callback(conn,query,f);
        free(query);
      
        //if returned code is -1 , error occurred 
        if(rc==-1)
        {
            fprintf(stderr,"failed to find sensor with this timestamp\n");
            return -1;
        }
        
        return 0;
}

int find_sensor_after_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f)
{
       
      char *query;
      asprintf(&query,"SELECT * FROM %s WHERE timestamp>%ld",TO_STRING(TABLE_NAME),ts);
      int rc =execute_query_with_callback(conn,query,f);
      free(query);
        //if returned code is -1 , error occurred 
        if(rc==-1)
        {
            fprintf(stderr,"failed to find sensor after timestamp:\n");
            return -1;
        }
        
        return 0;
}
