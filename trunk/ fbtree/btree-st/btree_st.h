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
typedef struct _BLOG{
	u_int32_t ksize;		/* size of  key */
	pgno_t	nodeID;			/* pageno of log entry's owner */

    union{
	    u_int32_t u_dsize;		/* leaf node: size of  data */
        pgno_t    u_pgno;       /* internal node: pointer */
    }log_u;
#define u_dsize log_u.u_dsize
#define u_pgno log_u.u_pgno

    u_int32_t seqnum;       /* sequence number of log entry (to identify its order) */
    u_int32_t logVersion;   /* log version used in log compaction */
/* P_BIGDATA,P_BIGKEY has been defined in BINTERNAL */
#define ADD_KEY         0x04        /* log entry for add key */
#define DELETE_KEY      0x08        /* log entry for delete key */
#define UPDATE_POINTER  0x10        /* log entry for update pointer */
#define LOG_INTERNAL    0x20        /* log entry for update pointer */
#define LOG_LEAF        0x40        /* log entry for update pointer */
	u_char	flags;
	char	bytes[1];		/* data */
}BLOG;  /* wonderful name, isn't it? */
/* Get the page's BLOG structure at index indx. */
#define	GETBLOG(pg, indx)						\
	((BLOG *)((char *)(pg) + (pg)->linp[indx]))

/* Get the number of bytes in the entry. */
#define NBLOG(blog)							\
	LALIGN( sizeof(u_int32_t) + 2*sizeof(pgno_t) + 2*sizeof(u_int32_t) + sizeof(u_char) + blog->ksize \
        + ((blog->flags & LOG_LEAF) ? (blog->u_dsize) : 0))/* Get the number of bytes in the log mode entry by the disk mode entry 'bi' */
#define NBINTERNAL_LOG_FROM_DISK(bi)    \
    LALIGN( NBINTERNAL(bi->ksize) +  sizeof(pgno_t) + 2*sizeof(u_int32_t))

/* Get the number of bytes in the log mode entry by the disk mode leaf entry 'bl' */
#define NBLEAF_LOG_FROM_DISK(bl)    \
    LALIGN( NBLEAF(bl) +  sizeof(pgno_t) + 2*sizeof(u_int32_t))

/* Get the number of bytes in the disk mode entry by the log mode entry 'bi' */
#define NB_DISK_FROM_LOG(bi_log)    \
    LALIGN( NBLOG(bi_log) -  sizeof(pgno_t) - 2*sizeof(u_int32_t))

/* Copy a BLOG entry to the page.
 * Note: p should be the destination to insert. It can also used to copy a BLOG to another BLOG (i.e. p=BLOG)
 */
#define	WR_BLOG(p, binternal) {				\
	*(u_int32_t *)p = binternal->ksize;			\
    p += sizeof(u_int32_t);						\
	*(pgno_t *)p = binternal->nodeID;			\
	p += sizeof(pgno_t);					\
	*(pgno_t *)p = (binternal->flags & LOG_INTERNAL) ? binternal->u_pgno : binternal->u_dsize ;   \
	p += sizeof(pgno_t);						\
	*(pgno_t *)p = binternal->seqnum;			\
	p += sizeof(u_int32_t);						\
	*(pgno_t *)p = binternal->logVersion;		\
	p += sizeof(u_int32_t);						\
	*(u_char *)p = binternal->flags;						\
	p += sizeof(u_char);						\
    strncpy((char*)p, binternal->bytes, binternal->ksize + ((binternal->flags & LOG_INTERNAL) ? 0 : binternal->u_dsize) );    \
}

void log_dump(BLOG* log);
void append_log(PAGE* p , BLOG* bi_log);
BLOG* disk2log_bi(BINTERNAL* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);
BLOG* disk2log_bl(BLEAF* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);

void* log2disk_bi( BLOG* bi_log);
/* ----
 * = Section 3. Log Buffer =
 * ----
 */

void logpool_init(BTREE* t);
pgno_t logpool_put(BTREE* t,BLOG* bi_log);



/* ----
 * = Section 4. Node operation =
 * ----
 */

PAGE* read_node(BTREE* mp , pgno_t x);
void addkey2node_log(PAGE* h ,BLOG* bi_log);
void addkey2node( PAGE* h, void* bi, indx_t skip);
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
