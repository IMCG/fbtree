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
    u_int32_t   logversion;
    u_int32_t   maxSeq;
    SectorList  list;
    u_int32_t        flags;        /* Note: clear P_NOTUSED when set others!!! */
}NTTEntry;


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
    u_int32_t logversion;   /* log version used in log compaction */
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
#define NBLOG_DBT(ksize, dsize)						\
	LALIGN(sizeof(u_int32_t) + 2*sizeof(pgno_t) + 2*sizeof(u_int32_t) + sizeof(u_char) + \
	    (ksize) + (dsize))

/* Get the number of bytes in the disk mode entry by the log mode entry 'bi' */
#define NB_DISK_FROM_LOG(blog)    \
    LALIGN( NBLOG(blog) -  sizeof(pgno_t) - 2*sizeof(u_int32_t))

#define WR_BLOG_DBT_BI(p,nid,key,pgno,seqnum,logversion,flags) { \
	*(u_int32_t *)p = key->size;		\
    p += sizeof(u_int32_t);				\
	*(pgno_t *)p = nid;		            \
	p += sizeof(pgno_t);				\
    *(u_int32_t*)p = pgno;     \
    p+=sizeof(u_int32_t);               \
	*(pgno_t *)p = seqnum;		\
	p += sizeof(u_int32_t);				\
	*(pgno_t *)p = logversion;	\
	p += sizeof(u_int32_t);				\
	*(u_char *)p = flags;			\
	p += sizeof(u_char);				\
    memmove((char*)p, key->data, key->size);    \
}

#define WR_BLOG_DBT_BL(p,nid,key,data,seqnum,logversion,flags) { \
	*(u_int32_t *)p = key->size;		\
    p += sizeof(u_int32_t);				\
	*(pgno_t *)p = nid;		            \
	p += sizeof(pgno_t);				\
    *(u_int32_t*)p = data->size;     \
    p+=sizeof(u_int32_t);               \
	*(u_int32_t *)p = seqnum;		\
	p += sizeof(u_int32_t);				\
	*(u_int32_t *)p = logversion;	\
	p += sizeof(u_int32_t);				\
	*(u_char *)p = flags;			\
	p += sizeof(u_char);				\
    memmove((char*)p, key->data, key->size);    \
    p+=key->size; \
    memmove((char*)p, data->data, data->size);    \
}

/* Copy a BLOG entry to the page.
 * Note: p should be the destination to insert. It can also used to copy a BLOG to another BLOG (i.e. p=BLOG)
 */
#define	WR_BLOG(p, blog) {				\
	*(u_int32_t *)p = blog->ksize;		\
    p += sizeof(u_int32_t);				\
	*(pgno_t *)p = blog->nodeID;		\
	p += sizeof(pgno_t);				\
    if(blog->flags & LOG_LEAF){         \
        *(u_int32_t*)p = blog->u_dsize; \
        p+=sizeof(u_int32_t);           \
    }else{ \
        *(pgno_t*)p = blog->u_pgno; \
        p+=sizeof(pgno_t);           \
    } \
	*(pgno_t *)p = blog->seqnum;		\
	p += sizeof(u_int32_t);				\
	*(pgno_t *)p = blog->logversion;	\
	p += sizeof(u_int32_t);				\
	*(u_char *)p = blog->flags;			\
	p += sizeof(u_char);				\
    memmove((char*)p, blog->bytes, blog->ksize);    \
    p+=blog->ksize; \
    if( (blog->flags & LOG_LEAF)) { \
        memmove((char*)p, blog->bytes+blog->ksize , blog->u_dsize);    \
    } \
}
/*
int CreateNode(pgno_t nodeID){

}
int ReadNode(pgno_t nodeID){


}

int UpdateNode(pgno_t nodeID){

}
*/
#endif
