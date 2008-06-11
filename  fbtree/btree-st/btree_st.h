#ifndef __BTREE_ST_H_
#define __BTREE_ST_H_
#include "list.h"
#include "errhandle.h"
#include <assert.h>

/* ----
 * = Section 1. Node Translation Table =
 * ----
 */

/* convert the nodeID in NTT into disk pgno */
#define nid2pgno(nid)   nid
/* convert disk pgno into the nodeID in NTT */
#define pgno2nid(pgno) pgno

typedef struct _sectorlist{
    pgno_t pgno;
    struct list_head list;
}SectorList;
typedef struct _NTTentry{
    u_int32_t   logVersion;
    u_int32_t   maxSeq;
    SectorList  list;
/* The Same with defintion in PAGE
#define P_BINTERNAL   0x01
#define P_BLEAF       0x02
*/
#define P_LOG        0x80
#define P_DISK       0x100
#define P_NOTUSED    0x200    /* flags are also used to check whether is it valid node */
    u_int32_t        flags;        /* Note: clear P_NOTUSED when set others!!! */
}NTTEntry;


NTTEntry* NTT_get(pgno_t pgno);
//u_int32_t NTT_getLogVersion(pgno_t pgno);
//u_int32_t NTT_getMaxSeq(pgno_t pgno);
void NTT_add(pgno_t nid, PAGE* pg);
void NTT_add_pgno(pgno_t nodeID, pgno_t pgno);
void NTT_del_list(NTTEntry* entry);
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

/* Get the number of bytes in the user's key/data pair. */
#define NBLEAF_LOG_DBT(ksize, dsize)						\
	LALIGN(sizeof(u_int32_t) + 2*sizeof(pgno_t) + 2*sizeof(u_int32_t) + sizeof(u_char) + \
	    (ksize) + (dsize))

/* Get the number of bytes in the disk mode entry by the log mode entry 'bi' */
#define NB_DISK_FROM_LOG(blog)    \
    LALIGN( NBLOG(blog) -  sizeof(pgno_t) - 2*sizeof(u_int32_t))

/* Copy a BLOG entry to the page.
 * Note: p should be the destination to insert. It can also used to copy a BLOG to another BLOG (i.e. p=BLOG)
 */
#define	WR_BLOG(p, blog) {				\
	*(u_int32_t *)p = blog->ksize;			\
    p += sizeof(u_int32_t);						\
	*(pgno_t *)p = blog->nodeID;			\
	p += sizeof(pgno_t);					\
	*(pgno_t *)p = (blog->flags & LOG_INTERNAL) ? blog->u_pgno : blog->u_dsize ;   \
	p += sizeof(pgno_t);						\
	*(pgno_t *)p = blog->seqnum;			\
	p += sizeof(u_int32_t);						\
	*(pgno_t *)p = blog->logVersion;		\
	p += sizeof(u_int32_t);						\
	*(u_char *)p = blog->flags;						\
	p += sizeof(u_char);						\
    strncpy((char*)p, blog->bytes, blog->ksize);    \
    p+=blog->ksize; \
    if( (blog->flags & LOG_LEAF)) strncpy((char*)p, blog->bytes+blog->ksize , blog->u_dsize);    \
}
//not euqle. why? 
//strncpy((char*)p, blog->bytes, blog->ksize + ((blog->flags & LOG_INTERNAL) ? 0 : blog->u_dsize) );    
void log_dump(BLOG* log);

void disk_entry_dump(void* entry, u_int32_t flags);

void append_log(PAGE* p , BLOG* blog);
BLOG* disk2log_bi(BINTERNAL* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);
BLOG* disk2log_bl(BLEAF* bl, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);
BLOG* disk2log_bl_dbt(DBT* key, DBT* data, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion);
void* log2disk( BLOG* blog);
/* ----
 * = Section 3. Log Buffer =
 * ----
 */

void logpool_init(BTREE* t);
pgno_t logpool_put(BTREE* t,BLOG* blog);



/* ----
 * = Section 4. Node operation =
 * ----
 */

int Mpool_put( MPOOL *mp, void *page, u_int flags);
PAGE* read_node(BTREE* mp , pgno_t x);
void addkey2node_log(PAGE* h ,BLOG* blog);
void addkey2node( PAGE* h, void* bi, indx_t skip);

PAGE * new_node( BTREE *t, pgno_t* nid ,u_int32_t flags);
indx_t search_node( PAGE * h, u_int32_t ksize, char bytes[]);
PAGE* init_node_mem(BTREE* t, pgno_t nid, u_char flags );

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
