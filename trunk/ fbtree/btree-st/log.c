#include "btree.h"

static void mpool_sync_page(MPOOL* mp, pgno_t pg);
/**
 * log_dump - dump the log entry.
 * BINTERNAL currently.
 */
void log_dump(BLOG* bi){
  if(bi->flags & LOG_INTERNAL){
    err_debug(("[ ksize=%u, nodeID=%u, pgno=%u, seqnum=%u, logversion=%u, flags=%x, key=%u ]",
                bi->ksize, bi->nodeID, bi->u_pgno, bi->seqnum, bi->logversion, bi->flags, *(u_int32_t*)bi->bytes));
  }
  else{
    assert(bi->flags & LOG_LEAF);
    err_debug(("[ ksize=%u, nodeID=%u, dsize=%u, seqnum=%u, logversion=%u, flags=%x, key=%u, data=%u]",
                bi->ksize, bi->nodeID, bi->u_dsize, bi->seqnum, bi->logversion, bi->flags, *(u_int32_t*)bi->bytes, *(u_int32_t*)(bi->bytes+bi->ksize) ));
  }
}

/**
 * disk mode dump 
 */
void disk_entry_dump(void* entry, u_int32_t flags){
  BINTERNAL * bi;
  BLEAF* bl;
  if(flags & P_BINTERNAL){
      bi  = (BINTERNAL*)entry;
      err_debug(("[ ksize=%u, pgno=%u, flags=%x, key=%u ]",
                bi->ksize, bi->pgno, bi->flags, *(u_int32_t*)bi->bytes));
  }else{
      assert(flags & P_BLEAF);
      bl = (BLEAF*)entry;
      err_debug(("[ ksize=%u, dsize=%u,flags=%x, key=%u, data=%u ]",
                bl->ksize, bl->dsize, bl->flags, *(u_int32_t*)bl->bytes, *(u_int32_t*)(bl->bytes + bl->ksize) ));
  }
  
  
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
    logbuf = __bt_new(t,&pgno_logbuf);
    logbuf->flags = P_LOG;
    Mpool_put(t->bt_mp,logbuf,0);
}

/**
 * logpool_put - put the log entry into the log buffer
 * @op: {LOG_LEAF,LOG_INTERNAL}|{ADD_KEY,DELETE_KEY}
 * @return: pgno of the entry
 */
pgno_t logpool_put(BTREE* t, pgno_t nid, const DBT* key,const DBT* data, pgno_t pgno, u_int32_t op)
{
    u_int32_t nbytes;
    NTTEntry* entry;
    char * dest;

    entry = NTT_get(nid);

    nbytes = data ? NBLOG_DBT(key->size,data->size): NBLOG_DBT(key->size,0);

    //assert(logbuf->pgno == pgno_logbuf);
    logbuf = mpool_get(t->bt_mp,pgno_logbuf,0);
    assert(logbuf!=NULL);
    if(!is_enough_room(logbuf,nbytes)){
        Mpool_put(t->bt_mp,logbuf,MPOOL_DIRTY);
        //mpool_sync_page(t->bt_mp,pgno_logbuf);
        logbuf = __bt_new(t,&pgno_logbuf);
        logbuf->flags = P_LOG;
    }
    dest = makeroom(logbuf,NEXTINDEX(logbuf),nbytes);
    if(op & LOG_LEAF){
        assert((op & LOG_LEAF) && (pgno==P_INVALID)); 
        WR_BLOG_DBT_BL(dest,nid,key,data,entry->maxSeq+1,entry->logversion,op);
        entry->maxSeq++;
    }else{
        assert( (op & LOG_INTERNAL) && (data==NULL)); 
        WR_BLOG_DBT_BI(dest,nid,key,pgno,entry->maxSeq+1,entry->logversion,op);
        entry->maxSeq++;
    }

    logbuf = mpool_put(t->bt_mp,logbuf,MPOOL_DIRTY);

    NTT_add_pgno(nid,pgno_logbuf);

    return pgno_logbuf;
}
/**
 * mpool_sync_page - Write the pg
 *
 */
static void mpool_sync_page(MPOOL* mp, pgno_t pgno){
    mpool_sync(mp);
}    

void logpage_dump(PAGE* h)
{
    BLOG* blog;
    int i;
    assert( (h->flags & P_LOG) && (h->nid == P_INVALID) );
    err_debug(("---- Log Page %u Dump ----",h->pgno)); 
    for(i =0 ; i<NEXTINDEX(h) ; i++){
      blog = GETBLOG(h,i);
      log_dump(blog);
    }
    err_debug(("-----------------------")); 
}

