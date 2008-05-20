#include <queue.h>
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
    SectorList  list;
}NTTEntry;

/*use an arry to implement NTT first*/
static NTTEntry[] NTT;
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
    u_int32_t logversion;   /* log version used in log compaction */
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

/* Get the number of bytes in the entry. */
#define NBINTERNAL_LOG(len)							\
	LALIGN(sizeof(u_int32_t) + 2*sizeof(pgno_t) + 3*sizeof(u_int32_t) + (len))

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
 * = Section 3. Node operation =
 * ----
 */
int CreateNode(pgno_t nodeID){

}
int ReadNode(pgno_t nodeID){


}

int UpdateNode(pgno_t nodeID){

}

