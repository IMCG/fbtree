#include "btree.h"
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
 */
static pgno_t pgno_logbuf;
void logpool_put(BTREE* t ,BINTERNAL_LOG* bi_log){
    PAGE* logbuf;
    /* TODO if it's full, we should fulsh the buffer right now*/
    logbuf = mpool_get(t->bt_mp,pgno_logbuf,0);
    
    WR_BINTERNAL_LOG(logbuf, bi_log);
    
}

