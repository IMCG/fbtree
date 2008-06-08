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
    const char* err_loc ="(log_collect) in 'log.c'";
    LogList* tmp = (LogList*)malloc(sizeof(LogList));
    BINTERNAL_LOG * bi = (BINTERNAL_LOG*)malloc(NBINTERNAL_LOG(bi_log->ksize));
    char* dest = (char*)bi;
    WR_BINTERNAL_LOG(dest,bi_log);
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
    const char* err_loc = "(__rebuild_node) in 'node.c'";

#ifdef NODE_DEBUG
    err_debug(("rebuild node %d", h->pgno));
#endif
    assert(h->flags & P_BINTERNAL);
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
 * @t: btree
 * @x: pgno of the node, i.e. id of the node
 *
 * Read a node x from NTT.
 * If it's in disk mode, read it dorectly.
 * Otherwise, it's in log mode, collect all logs then rebuild the node.
 */
PAGE* read_node(BTREE*t , pgno_t x){
    const char* err_loc = "(read_node)";
    PAGE *h;
    pgno_t pg;
    BINTERNAL_LOG * bi_log;
    LogList logCollector;
    int i;

    NTTEntry*   entry;
    SectorList* head;/* head of the list */
    SectorList* slist;
    MPOOL* mp= t->bt_mp;

    entry = NTT_get(x);
    if( entry->flags & NODE_DISK){
        h = mpool_get(mp,x,0);
#ifdef NODE_DEBUG
        err_debug(("node %u: DISK|%s : %s",x,(h->flags & P_BINTERNAL) ? "INTERNAL": "LEAF" ,err_loc));
        return h;
#endif
    }
    else if(entry->flags & NODE_LOG){
#ifdef NODE_DEBUG
        err_debug(("node %u: LOG|%s : %s",x,(entry->flags & NODE_INTERNAL) ? "INTERNAL": "LEAF" ,err_loc));
#endif
        INIT_LIST_HEAD(&logCollector.list);
        head = &(entry->list);

#ifdef NODE_DEBUG
        err_debug(("~~^\niterate sector list"));
#endif
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
                if( bi_log->nodeID==x && bi_log->logVersion==entry->logVersion){
                    __log_collect(&logCollector,bi_log);
                }
            }
            mpool_put(mp,h,0);
        }
#ifdef NODE_DEBUG
        err_debug(("~~$"));
#endif
        h = mpool_get(mp,x,0);

        // construct the actual node of the page
#ifdef NODE_DEBUG
        err_debug(("~~^\nrebuild node"));
#endif
        PAGE_INIT(t,h);
        __rebuild_node(h,&logCollector);
#ifdef NODE_DEBUG
        err_debug(("~~$"));
#endif
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

    BINTERNAL* bi;
    indx_t skip;
    assert(bi_log !=NULL);

    bi = log2disk_bi(bi_log);

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

    assert(bi!=NULL);
    nbytes = NBINTERNAL(bi->ksize) ;
    /* move to make room for the new (key,pointer) pair */
    if (skip < (nxtindex = NEXTINDEX(h))){
            memmove(h->linp + skip + 1, h->linp + skip,
                (nxtindex - skip) * sizeof(indx_t));
    }
    /* insert key into the skip */
    h->lower += sizeof(indx_t);
    h->linp[skip] = h->upper -= nbytes;
    dest = (char *)h + h->linp[skip];
    WR_BINTERNAL(dest,bi->ksize,bi->pgno,0);
	memmove(dest, bi->bytes, bi->ksize);
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

    const char err_loc[] = "(search_node) in 'node.c'";

    if(h->flags & P_BLEAF){
        err_quit("not support leaf search yet: %s", err_loc);
    }

    k1.size=ksize;
    k1.data=(char*)bytes;
    /* Do a binary search on the current page. */
    for (base = 0, lim = NEXTINDEX(h); lim; lim >>= 1) {
        index = base + (lim >> 1);
        /* FIXME: WORKED here? the behavior is different from __bt_cmp in bt_util.c  */
	    //if (index == 0 && h->prevpg == P_INVALID && !(h->flags & P_BLEAF)){
		//    cmp = 1;
        //}
        //else{
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
        //}
        if (cmp > 0) {
            base = index + 1;
            --lim;
        }
	}
	//index = base ? base - 1 : base;
    //NOTE It should be base here
    index=base;
    return index;
}


