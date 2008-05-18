#include <queue.h>
/* ----
 * = Section 1. Node Translation Table =
 * ----
 */
struct _listentry{
    pgno_t pgno;
    LIST_ENTRY(pgno_t) entries;
};

typedef struct _NTTentry{
    bool        isLeaf;
    u_int32_t   logVersion;
    LISTHEAD(_listhead,_listentry) sectorList; 
}NTTENTRY;

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

