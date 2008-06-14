#include "db.h"
#include "errhandle.h"
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h>

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
    init();
    testBT();
    return 0;
}


void init(){
    config.BTdatfile="test.bt";
}
void testBT(){
	DB *dbp;
	DBT key, data;
	DBT rkey, rdata;
    int rc;
    u_int32_t k,d;
    u_int32_t rk,rd;
    unsigned int i;

	/* Create the database. */
    dbp=dbopen(config.BTdatfile,O_CREAT|O_RDWR,0600, DB_BTREE ,NULL);
   
    k=32;
    d=128;
    key.size=4;
    key.data=(void*)&k;

    data.size=4;
    data.data=(void*)&d;
    
    if(!dbp){
		err_quit("can't open database %s\n", config.BTdatfile);
	}
    //_bt_dump(dbp);
	
    //NTT_dump();
    for( i = 0 ; i<3000 ; i++){
        err_debug1("----\ni=%d\n",i);
        k = (u_int32_t)i;
        d = (u_int32_t)i*i;

        err_debug1("= BEGIN PUT (%d,%d) =", *(int*)key.data,*(int*)data.data);
        rc = dbp->put(dbp, &key, &data, R_NOOVERWRITE);
        
        
        if(rc!=RET_SUCCESS){
            err_quit("db put error");
        }
        //NTT_dump();
        
        rk=i/2+1;
        //rk=23;
        rkey.size=4;
        rkey.data=(void*)&rk;
        rdata.size=0;
        rdata.data=NULL;

        err_debug1("\n= BEGIN GET (%d,?)",*(int*)rkey.data);
        rc = dbp->get(dbp,&rkey,&rdata,0);
        if(rc==-1){
            err_quit("error while try to get ");
        }
        else if(rc==0){
            err_debug1("find the pair (%d,%d)", *(int*)rkey.data,*(int*)rdata.data);
            err_debug1("(%d,%d)", *(int*)rkey.data,*(int*)rdata.data);
        }
        else{
            err_debug1("not in the database");
        }
    }
    //__bt_dump(dbp);
	(void)dbp->close(dbp);

 } 
