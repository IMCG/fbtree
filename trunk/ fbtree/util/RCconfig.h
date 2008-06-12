/*
 * read a runtime control file .rc and set the value in struct 'config'
 * e.g.
 * # start .rc file
 * datfile      test.dat        # path of data file
 * recordfile   record          # path of record file
 * pagesize     512             # UnitB
 * operation    1               # 0=READ , 1=OVERWRITE , 2=APPEND
 * pagesize     1024            # later one's value will overwrite the early ones 
 * # end .rc file
 *
 *
 */
#ifndef RC_CONFIG_H_
#define RC_CONFIG_H_

#define READ  0
#define OVERWRITE 1
#define APPEND 2

#define BT_INSERT 0
#define BT_SEARCH 1
struct config
{
  /* configuration for testOS */
  char *datfile;       /* data file name */
  char *recordfile;   /* record file name */ 
  int operation;	/* operation of files */
  int pagenum;	/* amount of pages */
  int pagesize;	/* size of one page */

  /* configuration for testBT */
  char * BTtestcase;
  char * BTdatfile;
  char * BTmetafile;
  char * BTrecordfile;
  int BTpagesize;
  int BTrecordnum;
  int BTop; 
}config;

/* Todo: !!!Attention: you must set profile , the interface is not so good , can it set auto matically?*/
void loadRCconfig ();
void show_profile ();

#endif
