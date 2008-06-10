#include "btree.h"

static void mpool_sync_page(MPOOL* mp, pgno_t pg);
/* ----
 * = LOG Entry =
 * ----
 */
/**
 * append_log_bi - Append a BLOG 'blog' to the PAGE p
 * @p: page header of the dest page
 * @blog: internal log entry to insert
 *
 * It computes the dest to append and then use WR_BLOG to copy the log entry to the PAGE
 */
void append_log(PAGE* p , BLOG* blog){
    indx_t index;
    char* dest;
    u_int32_t nbytes;

    assert(p!=NULL);

    nbytes = NBLOG(blog);
    index = NEXTINDEX(p);
    p->lower += sizeof(indx_t);
    p->linp[index] = p->upper-=nbytes;
    dest = (char*)p + p->upper;
    err_debug(("hap")); 
    log_dump(blog);
    WR_BLOG(dest, blog);
    log_dump((BLOG*)((char*)p+p->upper ));

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
BLOG* disk2log_bi(BINTERNAL* bi, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion){
    BLOG* blog = (BLOG*)malloc(NBINTERNAL_LOG_FROM_DISK(bi) );
    blog->ksize = bi->ksize;
    blog->u_pgno = bi->pgno;
    blog->nodeID = nodeID;
    blog->seqnum = seqnum;
    blog->logVersion = logVersion;
    blog->flags = ADD_KEY | LOG_INTERNAL;
    memcpy(blog->bytes,bi->bytes,bi->ksize);
    return blog;
}
/**
 * disk2log - convert a btree leaf entry of disk mode into log mode
 *
 * @bi: bleaf entry with disk mode
 * @seqnum: sequence number of the new log entry
 * @logVersion: version of the new log entry
 *
 * @return: bleaf entry with log mode
 *
 * TODO we only deal with ADD_KEY here
 */
BLOG* disk2log_bl(DBT* key, DBT* data, pgno_t nodeID, u_int32_t seqnum, u_int32_t logVersion){
    BLOG* blog = (BLOG*)malloc( NBLEAF_LOG_DBT(key->size,data->size) );
    blog->ksize = key->size;
    blog->u_dsize = data->size;
    blog->nodeID = nodeID;
    blog->seqnum = seqnum;
    blog->logVersion = logVersion;
    blog->flags = ADD_KEY | LOG_LEAF;
    memcpy(blog->bytes, key->data, key->size);
    memcpy( blog->bytes + key->size, data->data, data->size);
    return blog;
}


/**
 * disk2log - convert a btree internal's entry of disk mode into log mode
 *
 * @blog: binternal entry with log mode
 *
 * @return: new binternal entry with disk mode
 *
 * TODO we only deal with ADD_KEY here
 */
void* log2disk( BLOG* blog){
    void* b_entry;
    BINTERNAL* bi;
    BLEAF* bl;
    
    b_entry = malloc(NB_DISK_FROM_LOG(blog));

    if(blog->flags & LOG_INTERNAL){
        bi = (BINTERNAL*)b_entry;
        bi->ksize = blog->ksize;
        bi->pgno = blog->u_pgno;
        bi->flags = 0;
        memcpy(bi->bytes,blog->bytes,blog->ksize);
    }else{
        assert(blog->flags & LOG_LEAF);
        bl = (BLEAF*)b_entry;
        bl->ksize = blog->ksize;
        bl->dsize = blog->u_dsize;
        bl->flags = 0;
        memcpy(bl->bytes, blog->bytes, blog->ksize + blog->u_dsize);
    }

    return b_entry;
}
/**
 * log_dump - dump the log entry.
 * BINTERNAL currently.
 */
void log_dump(BLOG* bi){
  if(bi->flags & LOG_INTERNAL){
    err_debug(("[ ksize=%u, nodeID=%u, pgno=%u, seqnum=%u, logVersion=%u, flags=%x, key=%u ]",
                bi->ksize, bi->nodeID, bi->u_pgno, bi->seqnum, bi->logVersion, bi->flags, *(u_int32_t*)bi->bytes));
  }
  else{
    assert(bi->flags & LOG_LEAF);
    err_debug(("[ ksize=%u, nodeID=%u, dsize=%u, seqnum=%u, logVersion=%u, flags=%x, key=%u, data=%u]",
                bi->ksize, bi->nodeID, bi->u_dsize, bi->seqnum, bi->logVersion, bi->flags, *(u_int32_t*)bi->bytes, *(u_int32_t*)(bi->bytes+bi->ksize) ));
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
    const char* err_loc = "function (logpool_init) in 'log.c'";
    logbuf = NULL;
    pgno_logbuf = P_INVALID;

}
/**
 * logpool_put - put the log entry into the log buffer
 * @mp: buffer pool
 * @blog: log entry
 *
 * @return: pgno of the entry
 * XXX we only deal with BLOG currently
 */
pgno_t logpool_put(BTREE* t ,BLOG* blog){
    const char* err_loc = "(logpool_put) in 'log.c'";
    u_int32_t nbytes;
    indx_t index;
    
    nbytes = NBLOG(blog);
    // first call
    if (logbuf == NULL){
        err_debug(("open a new log buffer pool"));
        logbuf = __bt_new(t,&pgno_logbuf);
    }


	if (logbuf->upper - logbuf->lower < nbytes + sizeof(indx_t)) {

#ifdef LOG_DEBUG    
        err_debug1("flush the full log buffer");
#endif 

        //FIXME tmp design, it should be pinned first, though it is actually
        logbuf = mpool_get(t->bt_mp,pgno_logbuf,0);

        Mpool_put(t->bt_mp,logbuf,MPOOL_DIRTY);
        //mpool_sync_page(t->bt_mp,pgno_logbuf);
        logbuf = __bt_new(t,&pgno_logbuf);
        // ??? after sync, the page still be pinned?
        logbuf->pgno  = pgno_logbuf;
        logbuf->prevpg = logbuf->nextpg = P_INVALID;
	    logbuf->lower = BTDATAOFF;
	    logbuf->upper = t->bt_psize;
        //TODO logbuf->flags = 

    }

    append_log(logbuf, blog);
    NTT_add_pgno(blog->nodeID,pgno_logbuf);
    Mpool_put(t->bt_mp,logbuf,MPOOL_DIRTY);

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
    BLOG* blog;
    NTTEntry* e;
    pgno_t pgno,npgno;
    pgno = P_INVALID;
    
    err_debug(("~^Generate log entry from node %u",pg->pgno));

    assert(NEXTINDEX(pg)>0);
    e = NTT_get(pg->pgno);
    for (i=0; i<NEXTINDEX(pg); i++){
        bi = GETBINTERNAL(pg,i);
        assert(bi!=NULL);

        blog = disk2log_bi(bi,pg->pgno,e->maxSeq+1,e->logVersion+1);

        /* TODO XXX ensure atomic operation we should compute the size first */
        npgno = logpool_put(t,blog);
        e->maxSeq++;
        //XXX e is refecthed in NTT_add_pgno
        if(npgno!=pgno){
            NTT_add_pgno(pg->pgno,npgno);
            pgno = npgno;
        }
    }
    e->logVersion++;
    err_debug(("~$End Generate"));
}
/**
 * mpool_sync_page - Write the pg
 *
 */
static void mpool_sync_page(MPOOL* mp, pgno_t pgno){
    mpool_sync(mp);
}    

void logpage_dump(PAGE* h){
    BLOG* blog;
    int i;
    for(i =0 ; i<NEXTINDEX(h) ; i++){
      blog = GETBLOG(h,i);
      log_dump(blog);
    }
}

