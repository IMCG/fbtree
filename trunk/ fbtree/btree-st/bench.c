#include "db.h"
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h>
#include "RCconfig.h"
#include "timer.h"
#include "errhandle.h"

extern void sqlite3Randomness(int N, void *pBuf);

void testInsert();
void testSearch();
void testBT();

int main(){
  testBT();
  return 0;
}


void testBT(){
	loadRCconfig();
	if(config.BTop==BT_INSERT){
		testInsert();
	}
	else if(config.BTop==BT_SEARCH){
		//testSearch();
	}
	else{
		printf("no such operation!\n");
	}
}
    
 
void testInsert(){
	
	int rc;
    int i;
	double totalTime=0;


	DB *dbp;
	DBT key, data;
	DBT rkey, rdata;
    long long rk,rd;
    char buf[64];
    long long ibuf; 
	FILE* fp=fopen(config.BTtestcase,"r");
    if(fp==NULL){
        err_sys("can't open test case file '%s'", config.BTtestcase);
    }
	/* Create the database. */
    dbp=dbopen(config.BTdatfile,O_CREAT|O_RDWR,0600, DB_BTREE ,NULL);
	if(!dbp){
		printf("can't open file");
		exit(-1);
	}
	/*
	 * Insert records into the database, where the key is the user
	 * input and the data is the user input in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
   
    data.data=buf;

	//for (i=0; i<config.BTrecordnum;i++) {
	for (i=0; i<30000;i++) {
      rd=0;
	  fscanf(fp,"%lld",&ibuf);

      key.size = 8;
      key.data = (void*)&ibuf;
      data.size = 8;
      sqlite3Randomness(data.size, data.data);
	  
      //err_debug1("= BEGIN PUT (%d,%d) =", *(int*)key.data,*(int*)data.data);
      
      start_timer();
	  rc = dbp->put(dbp, &key, &data, R_NOOVERWRITE);
	  totalTime += get_timer();
	  pr_times(config.BTrecordfile, totalTime);

        if(rc!=RET_SUCCESS){
            err_quit("db put error");
        }
#if 0        
        rkey.size=8;
        rkey.data=(void*)&ibuf;
        rdata.size=0;
        rdata.data=(void*)&rd;

        err_debug1("\n= BEGIN GET (%lld,?)",*(long long*)rkey.data);
        rc = dbp->get(dbp,&rkey,&rdata,0);
        if(rc==-1){
            err_quit("error while try to get ");
        }
        else if(rc==0){
            err_debug1("find the pair (%d,%d)", *(int*)rkey.data,*(int*)rdata.data);
        }
        else{
            //err_debug1("not in the database");
        }
#endif
    }

    __bt_stat(dbp);
	(void)dbp->close(dbp);


}
/*
void testSearch(){
	
	Btree* pBt;
	BtCursor* pCur;
	int iTable;
	int rc;
	
	void* notUsed;
	i64 nKey;
	//void * pData;
	//int nData;
	int Res;
	
	int i;
	double totalTime=0;

	FILE* fp=fopen(config.BTtestcase,"r");
	FILE* fpTable=fopen(config.BTmetafile,"r");


	//= Create a B-Tree file =
	//Open B-Tree
	rc = sqlite3BtreeOpen(config.BTdatfile,&pBt,BTREE_OMIT_JOURNAL);		
	errorHandle(rc,"can't open the data base file");

	fscanf(fpTable,"%d",&iTable);
	//Create a Cursor for the B-Tree
	sqlite3BtreeCursor(pBt, iTable, 0, NULL, notUsed, &pCur);
	errorHandle(rc,"can't create btree cursor");
	//= end create =

	//= Insert data into the file =
	for (i=0; i<config.BTrecordnum;i++) {
		printf("search record %d\n",i+1);
		
		nKey = i;
		start_timer();
		rc = sqlite3BtreeMoveto(pCur, notUsed, nKey, &Res);
		errorHandle(rc,"can't search btree");
		totalTime += get_timer();

		pr_times(config.BTrecordfile, totalTime);
		
	}
	
	fclose(fp);
    // generate random queries
	printf("pagesize = %d\n",sqlite3BtreeGetPageSize(pBt));
}
*/

