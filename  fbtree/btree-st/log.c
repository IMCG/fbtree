#include "btree.h"

static void mpool_sync_page(MPOOL* mp, pgno_t pg);
/* ----
 * = LOG Entry =
 * ----
 */
/**
 * append_log_bi - Append a BINTERNAL_LOG 'bi_log' to the PAGE p
 * @p: page header of the dest page
 * @bi_log: internal log entry to insert
 *
 * It computes the dest to append and then use WR_BINTERNAL_LOG to copy the log entry to the PAGE
 */
void append_log_bi(PAGE* p , BINTERNAL_LOG* bi_log){
    indx_t index;
    char* dest;
    u_int32_t nbytes;

    assert(p!=NULL);

    nbytes = NBINTERNAL_LOG(bi_log->ksize);
    index = NEXTINDEX(p);
    p->lower += sizeof(indx_t);
    p->linp[index] = p->upper-=nbytes;
    dest = (char*)p + p->upper;
    WR_BINTERNAL_LOG(dest, bi_log);

}
/**
 * disk2log - convert a btree internal's entry of disk mode into log mode
 *
 * @bi: binternal entry with disk mode
 * @seqnum: sequence number of the new log entry
 * @logVersion: version of the new log entry
 *
 * @return: binternal entry with log mode
 *
 * TODO we only deal with ADD_KEY here
 */
BINTERNAL_LOG* disk2log_bi(BINTERNAL* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion){
    BINTERNAL_LOG* bi_log = (BINTERNAL_LOG*)malloc(NBINTERNAL_LOG_FROM_DISK(bi) );
    bi_log->ksize = bi->ksize;
    bi_log->pgno = bi->pgno;
    bi_log->nodeID = nodeID;
    bi_log->seqnum = seqnum;
    bi_log->logVersion = logVersion;
    bi_log->flags = ADD_KEY;
    memcpy(bi_log->bytes,bi->bytes,bi->ksize);
    return bi_log;
}

/**
 * disk2log - convert a btree internal's entry of disk mode into log mode
 *
 * @bi_log: binternal entry with log mode
 *
 * @return: new binternal entry with disk mode
 *
 * TODO we only deal with ADD_KEY here
 */
BINTERNAL* log2disk_bi( BINTERNAL_LOG* bi_log){

    BINTERNAL* bi = (BINTERNAL*)malloc(NBINTERNAL_DISK_FROM_LOG(bi_log) );
    bi->ksize = bi_log->ksize;
    bi->pgno = bi_log->pgno;
    bi->flags = ADD_KEY;
    memcpy(bi->bytes,bi_log->bytes,bi_log->ksize);
    return bi;
}
/**
 * log_dump - dump the log entry.
 * BINTERNAL currently.
 */
void log_dump(BINTERNAL_LOG* bi){
    err_debug("[ ksize=%ud, nodeID=%ud, pgno=%ud, seqnum=%ud, logVersion=%ud ]",
                bi->ksize, bi->nodeID, bi->pgno, bi->seqnum, bi->logVersion);

}

/* ----
 * = Section 3. Log Buffer =
 * ----
 * We pin the log buffer in the memory until the database is closed.
 * The size of log buffer is one page for the moment.
 */
static pgno_t pgno_logbuf;
static PAGE* logbuf;
/**
 * logpool_init - initialize the log buffer pool
 * @mp: buffer pool
 *
 * it should be empty
 */
void logpool_init(BTREE* t){
    const char* err_loc = "function (logpool_init) in 'log.c'";
    logbuf = NULL;
    pgno_logbuf = P_INVALID;

}
/**
 * logpool_put - put the log entry into the log buffer
 * @mp: buffer pool
 * @bi_log: log entry
 *
 * @return: pgno of the entry
 * XXX we only deal with BINTERNAL_LOG currently
 */
pgno_t logpool_put(BTREE* t ,BINTERNAL_LOG* bi_log){
    const char* err_loc = "function (logpool_put) in 'log.c'";
    u_int32_t nbytes;
    indx_t index;

    nbytes = NBINTERNAL_LOG(bi_log->ksize);
    // first call
    if (logbuf == NULL){
#ifdef LOG_DEBUG
        err_debug0("Open a new log buffer pool: ");
#endif
        logbuf = __bt_new(t,&pgno_logbuf);
    }

#ifdef LOG_DEBUG
    err_debug("Before put a log into the log buffer pool: size = %ud, left = %ud", nbytes,logbuf->upper-logbuf->lower);
#endif

	if (logbuf->upper - logbuf->lower < nbytes + sizeof(indx_t)) {

#ifdef LOG_DEBUG    //{{{
        err_debug0("Flush the full log buffer: ");
#endif //}}}

        //FIXME tmp design, it should be pinned first, though it is actually
        logbuf = mpool_get(t->bt_mp,pgno_logbuf,0);

        mpool_put(t->bt_mp,logbuf,MPOOL_DIRTY);
        mpool_sync_page(t->bt_mp,pgno_logbuf);
        logbuf = __bt_new(t,&pgno_logbuf);
        // ??? after sync, the page still be pinned?
        logbuf->pgno  = pgno_logbuf;
        logbuf->prevpg = logbuf->nextpg = P_INVALID;
	    logbuf->lower = BTDATAOFF;
	    logbuf->upper = t->bt_psize;
        //TODO: logbuf->flags =
    }

    append_log_bi(logbuf, bi_log);

    err_debug0("append log: ");
    log_dump(bi_log);
#ifdef LOG_DEBUG
    err_debug("After put a log into the log buffer pool: size = %ud, left = %ud", nbytes + sizeof(indx_t),logbuf->upper-logbuf->lower);
#endif

    return pgno_logbuf;
}
/**
 * genLogFromNode - generate log entries from a disk mode node AND put them into log buffer.
 * @pg: the node's page lister
 *
 * we convert the real node into a set of log entries
 */
void genLogFromNode(BTREE* t, PAGE* pg){
    unsigned int i;
    BINTERNAL* bi;
    BINTERNAL_LOG* bi_log;
    NTTEntry* e;
    pgno_t pgno,npgno;
    pgno = P_INVALID;
    for (i=0; i<NEXTINDEX(pg); i++){
        bi = GETBINTERNAL(pg,i);
        assert(bi!=NULL);
        e = NTT_get(pg->pgno);

        bi_log = disk2log_bi(bi,pg->pgno,e->maxSeq+1,e->logVersion+1);

        /* TODO XXX ensure atomic operation we should compute the size first */
        npgno = logpool_put(t,bi_log);
        e->maxSeq++;
        e->logVersion++;
        //XXX e is refecthed in NTT_add_pgno
        if(npgno!=pgno){
            NTT_add_pgno(pg->pgno,npgno);
            pgno = npgno;
        }
    }
}
/**
 * mpool_sync_page - Write the pg
 *
 */
static void mpool_sync_page(MPOOL* mp, pgno_t pgno){
    mpool_sync(mp);
}

void wr_binternal_log(BINTERNAL_LOG* dest,BINTERNAL_LOG* src){
    dest->ksize = src->ksize;
    dest->nodeID = src->nodeID;
    dest->pgno = src->pgno;
    dest->seqnum = src->seqnum;
    dest->logVersion = src->logVersion;
    dest->flags = src->flags;
    strncpy((char*)dest, src->bytes, src->ksize);

}

