#include "list.h"
#include "errorhandle.h"
#define BT_DISK 0
#define BT_LOG 1

#define ERR_PGNO 1

/* ----
 * = Section 1. Node Translation Table =
 * ----
 */
typedef struct _sectorlist{
    pgno_t pgno;
    struct list_head list;
}SectorList;
typedef struct _NTTentry{
    bool        isLeaf;
    u_int32_t   logVersion;
    u_int32_t   maxSeq;
    SectorList  list;
}NTTEntry;

/*use an arry to implement NTT first*/
#define NTT_MAXSIZE 128 
static NTTEntry NTT[NTT_MAXSIZE];

NTTEntry* NTT_get(pgno_t pgno){
    if( pgno<=0 || pgno>NTT_MAXSIZE){
       err_exit(ERR_PGNO,"pgno > NTT_MAXSIZE OR pgno<=0"); 
    }
    return &NTT[pgno];

}
u_int32_t NTT_getLogVersion(pgno_t pgno){
    if( pgno<=0 || pgno>NTT_MAXSIZE){
       err_exit(ERR_PGNO,"pgno > NTT_MAXSIZE OR pgno<=0"); 
    }
    return NTT[pgno].logVersion;
}
u_int32_t NTT_getMaxSeq(pgno_t pgno){
    if( pgno<=0 || pgno>NTT_MAXSIZE){
       err_exit(ERR_PGNO,"pgno > NTT_MAXSIZE OR pgno<=0"); 
    }
    return NTT[pgno].maxSeq;
}
void NTT_add(PAGE* pg){
    pgno_t pgno = pg->pgno;
    /* XXX free NTT's Sector List */
    NTT[pgno].isLeaf = (pg.flags & P_BLEAF);
    NTT[pgno].logVersion = -1;
    INIT_LIST_HEAD(&(NTT[pgno].list.list));

}
/*
 * Interface to Access NTT
 */


/* ----
 * = Section 2. Log Entry =
 * ----
 */
typedef struct _binternal_log{
	u_int32_t ksize;		/* size of  */
	pgno_t	nodeID;			/* pageno of log entry's owner */
    pgno_t  pgno;           /* page number stored on */
    u_int32_t seqnum;       /* sequence number of log entry (to identify its order) */
    u_int32_t logVersion;   /* log version used in log compaction */
/* P_BIGDATA,P_BIGKEY has been defined in BINTERNAL */
#define ADD_KEY         0x04        /* log entry for add key */
#define DELETE_KEY      0x08        /* log entry for delete key */
#define UPDATE_POINTER  0x10        /* log entry for update pointer */
	u_int32_t	flags;
	char	bytes[1];		/* data */
}BINTERNAL_LOG;
/* Get the page's BINTERNAL_LOG structure at index indx. */
#define	GETBINTERNAL_LOG(pg, indx)						\
	((BINTERNAL_LOG *)((char *)(pg) + (pg)->linp[indx]))

#define NBINTERNAL(len)							\
	LALIGN(sizeof(u_int32_t) + sizeof(pgno_t) + sizeof(u_char) + (len))
/* Get the number of bytes in the entry. */
#define NBINTERNAL_LOG(len)							\
	LALIGN(sizeof(u_int32_t) + 2*sizeof(pgno_t) + 3*sizeof(u_int32_t) + (len))
/* Get the number of bytes in the entry by the disk mode entry 'bi' */
#define NBINTERNAL_LOG_FROM_DISK(bi)    \
    LALIGN( NBINTERNAL(bi->ksize) +  sizeof(pgno_t) + 3*sizeof(u_int32_t) -sizeof(u_char))

/* Copy a BINTERNAL_LOG entry to the page. */
#define	WR_BINTERNAL_LOG(p, binternal) {				\
	*(u_int32_t *)p = binternal->ksize;			\
	p += sizeof(u_int32_t);						\
	*(pgno_t *)p = binternal->nodeID;			\
	p += sizeof(pgno_t);						\
	*(pgno_t *)p = binternal->pgno;				\
	p += sizeof(pgno_t);						\
	*(pgno_t *)p = binternal->seqnum;			\
	p += sizeof(u_int32_t);						\
	*(pgno_t *)p = binternal->logVersion;		\
	p += sizeof(u_int32_t);						\
	*(u_char *)p = flags;						\
	p += sizeof(u_int32_t);						\
    strncpy((char*)p, binternal->bytes, binternal->ksize);  \
}
/*
 * disk2log - convert a btree internal's entry of disk mode into log mode
 * @bi: binternal entry with disk mode
 * @seqnum: sequence number of the new log entry 
 * @logVersion: version of the new log entry 
 * return:
 * binternal entry with log mode
 */
BINTERNAL_LOG* disk2log_bi(BINTERNAL* bi, u_int32_t seqnum, u_int32_t logVersion){
    BINTERNAL_LOG* bi_log = (BINTERNAL_LOG*)malloc(NBINTERNAL_LOG_FROM_DISK(bi) ); 
    bi_log->ksize = bi->ksize;
    bi_log->nodeID = bi->pgno;
    bi_log->seqnum = seqnum;
    bi_log->logVersion = logVersion;
    bi_log->flags = ADD_KEY;
    memcpy(bi_log->bytes,bi->bytes,bi->ksize);
    return bi_log;    
}
/*
 * genLogFromNode - generate log entries from a disk mode node AND put them into log buffer,
 *                  i.e. convert the real node into a set of log entries
 * @pg: the node's page header
 */
void genLogFromNode( PAGE* pg){
    unsigned int i;
    BINTERNAL* bi;
    BINTERNAL_LOG* bi_log;
    NTTEntry* e;
    for (i=0; i<NEXTINDEX(pg); i++){
        bi = GETBINTERNAL(pg,i);
        assert(bi!=NULL);
        e = NTT_get(pg->pgno);
        disk2log_bi(bi,e->maxSeq+1,e->logVersion+1);
        e->maxSeq++;
        e->logVersion++;
    }
}

/* ----
 * = Section 3. Log Buffer =
 * ----
 */
static pgno_t pgno_logbuf;
void logpool_put(BTREE* t ,BINTERNAL_LOG* bi){
    PAGE* logbuf;
    logbuf = mpool_get(t->bt_mp,pgno_logbuf,0);
    
    WR_BINTERNAL_LOG(logbuf, bi);

    assert(bi!=NULL);
    free(bi);

}
/* NOT used at current time */
#if 0 
typedef struct _bleaf_log{
	u_int32_t ksize;		/* key size */
	u_int32_t	ksize;		/* size of key */
	u_int32_t	dsize;		/* size of data */
	u_char	flags;			/* P_BIGDATA, P_BIGKEY */
	char	bytes[1];		/* data */
}LEAF_LOG;
#endif

/* ----
 * = Section 4. Log Buffer =
 * ----
 */


/* ----
 * = Section 5. Node operation =
 * ----
 */
int CreateNode(pgno_t nodeID){

}
int ReadNode(pgno_t nodeID){


}

int UpdateNode(pgno_t nodeID){

}

