#include "db.h"
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h>
#include "RCconfig.h"
#include "timer.h"

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

	FILE* fp=fopen(config.BTtestcase,"r");

	DB *dbp;
	DBT key, data;
    char buf[64];
    long long ibuf;
    if(!fp){
		printf("can't open file %s\n", config.BTtestcase);
		exit(-1);
    }
	/* Create the database. */
    dbp=dbopen(config.BTdatfile,O_CREAT|O_RDWR,0600, DB_BTREE ,NULL);
	if(!dbp){
		printf("can't open file %s\n", config.BTdatfile);
		exit(-1);
	}
	/*
	 * Insert records into the database, where the key is the user
	 * input and the data is the user input in reverse order.
	 */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
   
    data.data=buf;

	for (i=0; i<config.BTrecordnum;i++) {
	  printf("insert record %d\n",i+1);
	  fscanf(fp,"%lld",&ibuf);

      key.size = 8;
      key.data = (void*)&ibuf;
  //    key.data = "fa";

      data.size = 8;
      sqlite3Randomness(data.size, data.data);
	  
      start_timer();
	  rc = dbp->put(dbp, &key, &data, R_NOOVERWRITE);
	  totalTime += get_timer();
	  pr_times(config.BTrecordfile, totalTime);
    }

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

