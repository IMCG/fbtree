#include "btree.h"
/*
 * log list to collect entries of a node
 */
typedef struct _loglist{
    BINTERNAL_LOG* log;
    struct list_head list;
}LogList;

/* 
 * Copy bi_log into the log list "logCollector"
 *
 * bi_log is cloned otherwise you have to held the whole page (which bi_log belongs to) in memory
 *
 * XXX you should make a decision here, clone bi_log or keep page in memory 
 */
static void __log_collect( LogList* logCollector, BINTERNAL_LOG* bi_log){
    LogList* tmp = (LogList*)malloc(sizeof(LogList));
    BINTERNAL_LOG * bi = (BINTERNAL_LOG*)malloc(NBINTERNAL_LOG(bi_log->ksize));
    WR_BINTERNAL_LOG(bi,bi_log);
    tmp->log = bi;
    list_add(&(tmp->list),&(logCollector->list));
}
/*
 * Free the memory of log list
 */
static void __log_free( LogList* logCollector){


}
/* 
 * Rebuild Node from the LogList 'list' 
 *
 * It construct the node by apply the logs in order to 'h'.
 * Return PAGE lister of the new node.
 */
static PAGE* __rebuild_node(PAGE* h, LogList* list){
    pgno_t npg;
    LogList* entry;
    BINTERNAL_LOG* log;
    const char* err_loc = "function (__rebuild_node) in 'node.c'";

#ifdef NODE_DEBUG
    err_debug("rebuild node %d", h->pgno);
#endif


    /* TODO sort the log entry by seqnum
     * XXX 
     * you must sort the list to make it in order
     * but if it's append only, the order is not important
     */
    // apply the log entries to construc a node
    list_for_each_entry(entry,&(list->list),list){
        log = entry->log;
        assert(!( (log->flags & P_BIGKEY) | (log->flags & P_BIGDATA)));
        if(log->flags & ADD_KEY){
            addkey2node_log(h,log);
        }
        else if(log->flags & DELETE_KEY){
            /* TODO NO DELETE_KEY YET */
            err_quit("no delete key yet: %s",err_loc);
        }
        else if(log->flags & UPDATE_POINTER){
            /* ??? no UPDATE_POINTER seems strange */

        }
    }

    return h;
} 

/**
 * read_node - read Node[x] from mp
 *
 * @mp: buffer pool
 * @x: pgno of the node, i.e. id of the node
 * 
 * Read a node x from NTT.
 * If it's in disk mode, read it dorectly.
 * Otherwise, it's in log mode, collect all logs then rebuild the node. 
 */
PAGE* read_node(MPOOL*mp , pgno_t x){
    const char* err_loc = "function (read_node) in 'node.c'";
    PAGE *h;
    pgno_t pg;
    BINTERNAL_LOG * bi_log;
    LogList logCollector;
    int i;
    
    NTTEntry*   entry = NTT_get(x);
    int version;    /* latest version of current node */
    SectorList* head;/* head of the list */
    SectorList* slist;
    
    if( entry->flags & NODE_DISK){
        h = mpool_get(mp,x,0);
#ifdef NODE_DEBUG
        err_debug("%s node %ud in DISK mode : %s", (h->flags & P_BINTERNAL) ? "INTERNAL": "LEAF" ,x,err_loc);
#endif
        return h;
    }
    else if(entry->flags & NODE_LOG){
#ifdef NODE_DEBUG
        err_debug("%s node %ud in LOG mode : %s", (entry->flags & NODE_INTERNAL) ? "INTERNAL": "LEAF" ,x,err_loc);
#endif
        INIT_LIST_HEAD(&logCollector.list);
        version = entry->logVersion;
        head = &(entry->list);

        // iterate the list
        list_for_each_entry(slist , &(head->list) , list ){

            // get the PAGE(pgno)
            h = mpool_get(mp,slist->pgno,0);
            if( h==NULL ){
                err_quit("can't get page: %s", err_loc);
            }

            // iterate the page to collect entry in this page
            for(i =0 ; i<NEXTINDEX(h) ; i++){
                // get the log entry
                bi_log = GETBINTERNAL_LOG(h,i);
                // if it belongs to the node x , collect it
                if( bi_log->nodeID==x){
                    __log_collect(&logCollector,bi_log);
                }
            }
            mpool_put(mp,h,0);
        }
        h = mpool_get(mp,x,0);
        // construct the actual node of the page
        __rebuild_node(h,&logCollector);
        __log_free(&logCollector);
        return h;
    }
    else{
        err_quit("unknown mode of node: %s",err_loc);
    }

}


/** 
 * addkey2node_log - Apply the internal log entry add to the *internel* node 'h'.
 * 
 * @h: node to insert
 * @bi_log: log entry
 *
 * XXX We first construct a binternal entry from the log entry, it's not essential. 
 * We should apply the log directly for efficiency.
 */
void addkey2node_log(PAGE* h ,BINTERNAL_LOG* bi_log){
    
    BINTERNAL* bi = log2disk_bi(bi_log);
    indx_t skip;
    assert(bi!=NULL);

    skip = search_node(h, bi_log->ksize, bi_log->bytes);
    addkey2node(h,bi,skip);
    free(bi);
}

/**
 * addkey2node - Insert the key to the new node
 *  
 * @h: node to insert
 * @bi: BINTERNAL entry
 * @skip: insert position
 * 
 * TODO currently internal only
 */
void addkey2node( PAGE* h, BINTERNAL* bi, indx_t skip){
    indx_t nxtindex;
    char * dest;
	u_int32_t n, nbytes;

    /* insert key into the skip */
    if (skip < (nxtindex = NEXTINDEX(h)))
            memmove(h->linp + skip + 1, h->linp + skip,
                (nxtindex - skip) * sizeof(indx_t));
    h->lower += sizeof(indx_t);
        

    h->linp[skip] = h->upper -= nbytes;
    dest = (char *)h + h->linp[skip];
    memcpy(dest, bi, nbytes);
    ((BINTERNAL *)dest)->pgno =bi->pgno;

    assert(bi!=NULL);
    /* TODO maybe we can use it later */    
#if 0
    switch (rchild->flags & P_TYPE) {
    case P_BINTERNAL:
        ...
    case P_BLEAF:
        h->linp[skip] = h->upper -= nbytes;
        dest = (char *)h + h->linp[skip];
        WR_BINTERNAL(dest, nksize ? nksize : bl->ksize,
            rchild->pgno, bl->flags & P_BIGKEY);
        memmove(dest, bl->bytes, nksize ? nksize : bl->ksize);
        if (bl->flags & P_BIGKEY &&
            bt_preserve(t, *(pgno_t *)bl->bytes) == RET_ERROR)
            goto err1;
        break;
    }
#endif
}

/**
 * search_node - search the node h to find proper position to insert
 * @h: node page lister
 * @ksize: size of key
 * @bytes: content of key
 *
 * @return: index s.t.[Ptr[index]] =< key < [Ptr[index+!]]
 * TODO internal node only here
 * XXX  no big key
 */
indx_t search_node( PAGE * h, u_int32_t ksize, char bytes[]){
	indx_t base,lim;
    indx_t index;
    DBT k1,k2;
    BINTERNAL* bi;
    int cmp; /* result of compare */

    const char err_loc[] = "function (search_node) in 'node.c'"; 

    if(h->flags & P_BLEAF){
        err_quit("not support leaf search yet: %s", err_loc);
    }
	
    k1.size=ksize;
    k1.data=(void*)bytes;
	

    /* Do a binary search on the current page. */
    for (base = 0, lim = NEXTINDEX(h); lim; lim >>= 1) {
        index = base + (lim >> 1);
        /* FIXME: WORKED here? the behavior is different from __bt_cmp in bt_util.c  */
#if 0
	    if (index == 0 && h->prevpg == P_INVALID && !(h->flags & P_BLEAF)){
		    cmp = 1;
        }
#endif
        bi=GETBINTERNAL(h,index);
		if (bi->flags & P_BIGKEY){
            err_quit("not deal with big key yet: %s",err_loc);
        }
		else {
			k2.data = bi->bytes;
			k2.size = bi->ksize;
		}

        if ((cmp = __bt_defcmp(&k1,&k2)) == 0) {
            return index;
        }
        if (cmp > 0) {
            base = index + 1;
            --lim;
        }
	}

	index = base ? base - 1 : base;
    return index;
}

