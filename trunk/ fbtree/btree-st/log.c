#include "btree.h"

static void mpool_sync_page(MPOOL* mp, pgno_t pg);
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
 */
void logpool_init(BTREE* t){
    const char* err_loc = "function (logpool_init) in 'log.c'";
    PAGE* h = __bt_new(t,&pgno_logbuf);

    if(h==NULL)
        err_sys("can't initialize the log pool: %s", err_loc);
    h->pgno = pgno_logbuf;
    h->prevpg = P_INVALID; 
    h->nextpg = P_INVALID; 
    //TODO: maybe we should pin it from now
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

    nbytes = NBINTERNAL_DISK_FROM_LOG(bi_log); 
	if (logbuf->upper - logbuf->lower < nbytes + sizeof(indx_t)) {
        mpool_sync_page(t->bt_mp,pgno_logbuf);
        logbuf = __bt_new(t,&pgno_logbuf);
        // ??? after sync, the page still be pinned?
        logbuf->pgno  = pgno_logbuf;
        logbuf->prevpg = logbuf->nextpg = P_INVALID;
	    logbuf->lower = BTDATAOFF;
	    logbuf->upper = t->bt_psize;
    }
    WR_BINTERNAL_LOG(logbuf, bi_log);
//?    mpool_put(mp,pgno_logbuf,MPOOL_DIRTY);
    return pgno_logbuf;
}

/**
 * mpool_sync_page - Write the pg
 *
 */
static void mpool_sync_page(MPOOL* mp, pgno_t pgno){
    mpool_sync(mp);
}
