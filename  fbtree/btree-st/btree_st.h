#ifndef __BTREE_ST_H_
#define __BTREE_ST_H_
#include "list.h"
#include "errhandle.h"
#include <assert.h>

/* ----
 * = Section 1. Node Translation Table =
 * ----
 */
typedef struct _sectorlist{
    pgno_t pgno;
    struct list_head list;
}SectorList;
typedef struct _NTTentry{
    u_int32_t   logVersion;
    u_int32_t   maxSeq;
    SectorList  list;
#define NODE_LOG        0x01
#define NODE_DISK       0x02
#define NODE_LEAF       0x04
#define NODE_INTERNAL   0x08
#define NODE_INVALID    0x10    /* flags are also used to check whether is it valid node */
    u_char        flags;        /* Note: clear NODE_INVALID when set others!!! */
}NTTEntry;


NTTEntry* NTT_get(pgno_t pgno);
//u_int32_t NTT_getLogVersion(pgno_t pgno);
//u_int32_t NTT_getMaxSeq(pgno_t pgno);
void NTT_add(PAGE* pg);
void NTT_add_pgno(pgno_t nodeID, pgno_t pgno);
void NTT_dump();
/* ----
 * = Section 2. Log Entry =
 * ----
 */
typedef struct _binternal_log{
	u_int32_t ksize;		/* size of  key */
	//u_int32_t dsize;		/* size of  data, for aligment */
	pgno_t	nodeID;			/* pageno of log entry's owner */
    pgno_t  pgno;           /* page number stored on */
    u_int32_t seqnum;       /* sequence number of log entry (to identify its order) */
    u_int32_t logVersion;   /* log version used in log compaction */
/* P_BIGDATA,P_BIGKEY has been defined in BINTERNAL */
#define ADD_KEY         0x04        /* log entry for add key */
#define DELETE_KEY      0x08        /* log entry for delete key */
#define UPDATE_POINTER  0x10        /* log entry for update pointer */
	u_char	flags;
	char	bytes[1];		/* data */
}BINTERNAL_LOG;
/* Get the page's BINTERNAL_LOG structure at index indx. */
#define	GETBINTERNAL_LOG(pg, indx)						\
	((BINTERNAL_LOG *)((char *)(pg) + (pg)->linp[indx]))

/* Get the number of bytes in the entry. */
#define NBINTERNAL_LOG(len)							\
	LALIGN(sizeof(u_int32_t) + 2*sizeof(pgno_t) + 2*sizeof(u_int32_t) + sizeof(u_char) + (len))

/* Get the number of bytes in the log mode entry by the disk mode entry 'bi' */
#define NBINTERNAL_LOG_FROM_DISK(bi)    \
    LALIGN( NBINTERNAL(bi->ksize) +  sizeof(pgno_t) + 2*sizeof(u_int32_t))
/* Get the number of bytes in the disk mode entry by the log mode entry 'bi' */
#define NBINTERNAL_DISK_FROM_LOG(bi_log)    \
    LALIGN( NBINTERNAL_LOG(bi_log->ksize) -  sizeof(pgno_t) - 2*sizeof(u_int32_t))

/* Copy a BINTERNAL_LOG entry to the page.
 * Note: p should be the destination to insert. It can also used to copy a binternal_log to another binternal_log (i.e. p=binternal_log)
 */
#define	WR_BINTERNAL_LOG(p, binternal) {				\
	*(u_int32_t *)p = binternal->ksize;			\
    p += sizeof(u_int32_t);						\
	*(pgno_t *)p = binternal->nodeID;			\
	p += sizeof(pgno_t);					\
	*(pgno_t *)p = binternal->pgno;				\
	p += sizeof(pgno_t);						\
	*(pgno_t *)p = binternal->seqnum;			\
	p += sizeof(u_int32_t);						\
	*(pgno_t *)p = binternal->logVersion;		\
	p += sizeof(u_int32_t);						\
	*(u_char *)p = binternal->flags;						\
	p += sizeof(u_char);						\
    strncpy((char*)p, binternal->bytes, binternal->ksize);  \
}

void wr_binternal_log(BINTERNAL_LOG* dest,BINTERNAL_LOG* src);
void log_dump(BINTERNAL_LOG* log);
void append_log_bi(PAGE* p , BINTERNAL_LOG* bi_log);
BINTERNAL_LOG* disk2log_bi(BINTERNAL* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);
BINTERNAL* log2disk_bi( BINTERNAL_LOG* bi_log);
/* ----
 * = Section 3. Log Buffer =
 * ----
 */

void logpool_init(BTREE* t);
pgno_t logpool_put(BTREE* t,BINTERNAL_LOG* bi_log);

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
 * = Section 4. Node operation =
 * ----
 */

PAGE* read_node(BTREE* mp , pgno_t x);
void addkey2node_log(PAGE* h ,BINTERNAL_LOG* bi_log);
void addkey2node( PAGE* h, BINTERNAL* bi, indx_t skip);
indx_t search_node( PAGE * h, u_int32_t ksize, char bytes[]);
void genLogFromNode(BTREE* t, PAGE* pg);
/*
int CreateNode(pgno_t nodeID){

}
int ReadNode(pgno_t nodeID){


}

int UpdateNode(pgno_t nodeID){

}
*/
#endif
