#include "db.h"
#include "errhandle.h"
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h>
/*
 * TODO tmp used for quick test
 */
struct config
{

  /* configuration for testBT */
  char * BTtestcase;
  char * BTdatfile;
  char * BTmetafile;
  char * BTrecordfile;
  int BTpagesize;
  int BTrecordnum;
  int BTop; 
}config;


void init();
void testInsert();
void testSearch();
void testBT();
int main(){
  testBT();
  return 0;
}


void init(){
    config.BTdatfile="test.bt";
}
void testBT(){
	DB *dbp;
	DBT key, data;
    int rc;
    u_int32_t k,d;
    u_int32_t rk,rd;


	/* Create the database. */
    dbp=dbopen(config.BTdatfile,O_CREAT|O_RDWR,0600, DB_BTREE ,NULL);
   
    k=32;
    d=128;
    key.size=4;
    key.data=(void*)&k;

    data.size=4;
    data.data=(void*)&d;
    
    if(!dbp){
		err_quit("can't open file %s\n", config.BTdatfile);
	}
	
    rc = dbp->put(dbp, &key, &data, R_NOOVERWRITE);
    if(!rc){
        err_quit("db put error");
    }
    
    rk=32;
    key.size=4;
    key.data=(void*)&rk;
    data.size=0;
    data.data=NULL;

    rc = dbp->get(dbp,&key,&data,0);
    if(rc==-1){
        err_quit("error while try to get ");
    }
    else if(rc==0){
        printf("find the pair (%d,%d)\n", *(int*)key.data,*(int*)data.data);
    }
    else{
        printf("not in the database\n");
    }
	(void)dbp->close(dbp);

 } 
