#include "btree.h"

/*
 * log list to collect entries of a node
 */
typedef struct _loglist{
    BLOG* log;
    struct list_head list;
}LogList;

/*
 * *Add* blog into the log list "logCollector"
 *
 * @copy: a bool variable to used to identify clone blog or keep page in memory
 *
 * If blog is cloned, you have to held the whole page (which blog belongs to) in memory
 */
static void _log_collect( LogList* logCollector, BLOG* blog, u_char copy)
{

    LogList* tmp = (LogList*)malloc(sizeof(LogList));
    if(!copy){
        tmp->log = blog;
    }
    else{
        BLOG * bi = (BLOG*)malloc(NBLOG(blog));
        char* dest = (char*)bi;
        WR_BLOG(dest,blog);
        tmp->log = bi;
    }
    list_add(&(tmp->list),&(logCollector->list));
}
/*
 * Free the memory of log list
 */
static void _log_free( LogList* logCollector)
{


}
/*
 * Rebuild Node from the LogList 'list'
 *
 * It construct the node by apply the logs in order to 'h'.
 * Return PAGE lister of the new node.
 */
static PAGE* _rebuild_node(PAGE* h, LogList* list)
{
    LogList* entry;
    BLOG* log;
    DBT a,b;
    DBT* key = &a;
    DBT* data = &b;
    u_int32_t nbytes;
    indx_t index;
    char * dest;
    const char* err_loc = "(_rebuild_node) in 'node.c'";

    /* TODO sort the log entry by seqnum
     * XXX
     * you must sort the list to make it in order
     * but if it's append only, the order is not important
     */
    // apply the log entries to construc a node
    list_for_each_entry(entry,&(list->list),list){
        log = entry->log;

        index = search_node(h,log->ksize,log->bytes);
        //err_debug1("index = %d", index); 
        //LOG ADD KEY
        if(log->flags & ADD_KEY){
            if(log->flags & LOG_LEAF){
                nbytes = NBLEAFDBT(log->ksize,log->u_dsize); 
                dest = makeroom(h,index,nbytes);
                key->size = log->ksize;
                key->data = log->bytes;
                data->size = log->u_dsize;
                data->data = log->bytes + log->ksize;
                WR_BLEAF(dest, key, data, 0);
            }else{
                assert(log->flags & LOG_INTERNAL);
                nbytes = NBINTERNAL(log->ksize);
                dest = makeroom(h,index,nbytes);
                key->size = log->ksize;
                key->data = log->bytes;
                WR_BINTERNAL(dest, key, log->u_pgno, 0);
            }
        }
        else if(log->flags & DELETE_KEY){
            err_quit("no delete key yet: %s",err_loc);
        }
        else{
            err_quit("flags=%x: unkown operation: %s",log->flags,err_loc);
        }
    }
    return h;
}
/*
 * new_node_mem - create a virtual node in memory with nid
 *
 * @t: btree
 * @nid: node id
 *
 * @return new memory page
 */
static PAGE* new_node_mem(BTREE* t, pgno_t nid, u_char type )
{
    PAGE* h = (PAGE*)malloc(t->bt_psize);
    h->nid  = nid;
    h->pgno = P_INVALID;
	h->nextpg = P_INVALID;
	h->prevpg = P_INVALID;
	h->lower  = BTDATAOFF;
	h->upper  = t->bt_psize;
    h->flags = type | P_LMEM;
    return h;
}

/**
 * read_node - read Node[x] from mp
 *
 * @t: btree
 * @x: id of the node
 *
 * Read a node x from NTT.
 * If it's in disk mode, read it dorectly.
 * Otherwise, it's in log mode, collect all logs then rebuild the node.
 */
PAGE* read_node(BTREE* t , pgno_t x)
{
    const char* err_loc = "(read_node) in 'node.c'";
    PAGE *h=NULL;
    BLOG * blog=NULL;
    LogList logCollector;
    int i;

    NTTEntry*   entry=NULL;
    SectorList* head=NULL;/* head of the list */
    SectorList* slist=NULL;
    MPOOL* mp= t->bt_mp;

    entry = NTT_get(x);
    head = &(entry->list);
    if( entry->flags & P_DISK){
        h = mpool_get(mp,head->pgno,0);
    }
    else if(entry->flags & P_LOG){
        INIT_LIST_HEAD(&logCollector.list);

        // iterate the list
        list_for_each_entry(slist , &(head->list) , list ){

            // get the PAGE(pgno)
            h = mpool_get(mp,slist->pgno,0);
            if( h==NULL ){
                err_dump("can't get page: %s", err_loc);
            }
            
            // iterate the page to collect entry in this page
            for(i =0 ; i<NEXTINDEX(h) ; i++){
                // get the log entry
                blog = GETBLOG(h,i);
                // if it belongs to the node x , collect it
                if( blog->nodeID==x && blog->logversion==entry->logversion){
                    _log_collect(&logCollector,blog,1);
                }
            }
            Mpool_put(mp,h,0);
        }

        // construct the actual node of the page
        err_debug(("--^Rebuild node %u",x));
        h = new_node_mem(t,x,entry->flags & P_TYPE);
        _rebuild_node(h,&logCollector);
        _log_free(&logCollector);
        err_debug(("--$End Rebuild"));
    }
    else{
        err_quit("error: node[%d], flags=%x: unkown mode: %s", x, entry->flags, err_loc);
        h = NULL;
    }
    return h;

}
#if 0
double compute_delta(u_int32_t P, u_int32_t L, u_int32_t op )
{
    u_int32_t x; /* cost of serving O in mode S1, LOG mode */
    u_int32_t y; /* cost of serving O in mode S2, DISK mode */
    double cw; //cost of write per page
    double cr; //cost of read per page
    u_int32_t node_size; //size of a node 
    u_int32_t log_per_page; //max log entries a page can hold 
    
    if(op & READ){
        x = t->cr * P;
        y = t->cr * t->node_size; 
    }else{
        assert(op & WRITE);
        x = t->cw * L / t->log_per_page; 
        y = t->cw * t->node_size; 
    }

    return x-y;
}

void switch_node(BTREE* t,  PAGE* h, u_int32_t op){
    
    NTTEntry* entry = NTT_get(h->nid);
    u_int32_t L = 2; //default value of L, L - the number of log entrie a write operation generate
    u_int32_t P = 3; //default value of P, P - the number of pages a LOG mode node span

    /* current mode: DISK mode */
    if( mode & P_DISK){
        delta = compute_dealta(L,P,op);
        entry->f += delta;
        entry->f = (entry->f < 0) ? 0 : entry->f;
    }else{ /* current mode: LOG mode */
        assert(mode & P_LMEM);
        if(op & WRITE){
            L = op & WHOLE_NODE ? NEXTINDEX(h): 1; 
        }
        P = entry->pg_cnt;
        delta = compute_dealta(L,P,op);
        entry->f -= delta;
        entry->f = (entry->f < 0) ? 0 : entry->f;
    }

    /* migrate when > C */
    if( entry->f > C){
        diskmem2log();
    }else{
        mem2disk();
    }

}
#endif
/**
 * node_addkey - Add Key to  a node
 * 
 * @h: the node - ({P_LMEM|P_DISK},{P_BINTERNAL|P_BLEAF})
 * @key
 * @data: NULL for BINTERNAL node
 * @pgno: P_INVALID for BLEAF node
 * @index: not used by  P_LOG. insert position
 * @nbytes: not used by P_LOG. number of bytes of insert key/data
 * @flags: 
 */
void node_addkey(BTREE* t,PAGE* h, const DBT* key, const DBT* data, pgno_t pgno,
                 indx_t index, u_int32_t nbytes)
{
    char * dest=NULL;

    if(h->flags & P_BLEAF){
        assert(pgno==P_INVALID);
        if( h->flags & P_DISK){
            dest = makeroom(h,index,nbytes);
            WR_BLEAF(dest, key, data, 0);
            mpool_put(t->bt_mp,h,MPOOL_DIRTY);
        }
        else{
            assert(h->flags & P_LMEM);
            logpool_put(t,h->nid,key,data,P_INVALID, ADD_KEY | LOG_LEAF );
        }
    }else{
        assert(h->flags & P_BINTERNAL);
        assert(data==NULL);
        if( h->flags & P_DISK){
            dest = makeroom(h,index,nbytes);
            WR_BINTERNAL(dest, key, pgno, 0);
            mpool_put(t->bt_mp,h,MPOOL_DIRTY);
        }
        else{
            assert(h->flags & P_LMEM);
            logpool_put(t,h->nid,key,data,pgno, ADD_KEY | LOG_INTERNAL );
        }
    }

}

/**
 * search_node - search the node h to find proper position to insert
 * @h: node page lister
 * @ksize: size of key
 * @bytes: content of key
 *
 * @return: index s.t.[Ptr[index]] =< key < [Ptr[index+!]]
 */
indx_t search_node( PAGE * h, u_int32_t ksize, char bytes[])
{
	indx_t base,lim;
    indx_t index;
    DBT k1,k2;
    BINTERNAL* bi;
    BLEAF* bl;
    int cmp; /* result of compare */

    k1.size=ksize;
    k1.data=(char*)bytes;
    //err_debug1("k1 = (%d,%x)", k1.size ,*(int*)k1.data); 
    /* Do a binary search on the current page. */
    for (base = 0, lim = NEXTINDEX(h); lim; lim >>= 1) {
        index = base + (lim >> 1);
        /* FIXME: WORKED here? the behavior is different from __bt_cmp in bt_util.c  */
#if 0
        if (index == 0 && h->prevpg == P_INVALID && !(h->flags & P_BLEAF)){
		    cmp = 1;
        }
#endif
            if(h->flags & P_BINTERNAL){
                bi=GETBINTERNAL(h,index);
                k2.data = bi->bytes;
                k2.size = bi->ksize;
            }else{
                assert(h->flags & P_BLEAF);
                bl=GETBLEAF(h,index);
                k2.data = bl->bytes;
                k2.size = bl->ksize;
            }


            if ((cmp = __bt_defcmp(&k1,&k2)) == 0) {
                base = index;
                break;
                    //return index;
            }
        
//            err_debug1("index = %d, cmp = %d, k1 = (%d,%x), k2 =(%d,%x)", index, cmp, k1.size, *(int*)k1.data , k2.size ,*(int*)k2.data); 
        if (cmp > 0) {
            base = index + 1;
            --lim;
        }
	}
	//index = base ? base - 1 : base;
    //NOTE It should be base here
    index=base;
    assert( index>=0 && index<=NEXTINDEX(h));
    return index;
}


/**
 * new_node_id - allocate a new node id
 */
static pgno_t new_node_id(){
    static pgno_t maxpg = 0;
    return ++maxpg;
}
/**
 * new_node -- Create a new logical node with assigned flags  
 * 
 * @t:	tree
 * @nid: as a return value, the new node's node id
 * @type: P_BINTERNAL | P_BLEAF
 *
 * @return: pointer to a new node
 *
 * NOTE: it will add the new node to the NTT
 */
PAGE * new_node( BTREE *t, pgno_t* nid ,u_int32_t type)
{
	PAGE *h;
    pgno_t npg; 

    npg = P_INVALID;
    *nid = new_node_id();
#if 0
    if(flags & P_DISK){
        if (t->bt_free != P_INVALID &&
            (h = mpool_get(t->bt_mp, t->bt_free, 0)) != NULL) {
            npg = t->bt_free;
            t->bt_free = h->nextpg;
#ifdef MPOOL_DEBUG
            err_debug(("new page %ud from the freelist",*pg));
#endif
        }
        else{
	        h= mpool_new(t->bt_mp, &npg);
        }
    }
    else{
#endif
        //assert(flags & P_LMEM);
        h = (PAGE*)malloc(t->bt_psize);
        if(h==NULL){
            err_sys("can't malloc new node");
        }
        npg = *nid;
        h->flags = P_LMEM; 
//    }

    assert(h!=NULL);
    h->pgno = npg;
    h->nid = *nid;
	h->nextpg = h->prevpg = P_INVALID;
	h->lower = BTDATAOFF;
	h->upper = t->bt_psize;
    h->flags |= type;
    NTT_new(*nid,h->flags);
    if(h->flags & P_DISK){
        NTT_add_pgno(*nid,npg);
    }
	return (h);
}

/*
* free_node - free the logical node
*/
int free_node(BTREE* t, PAGE* h)
{
    NTT_free(h->nid);
    if(h->flags & P_DISK){
        __bt_free(t,h);
    }else{
        assert(h->flags & P_LMEM);
        free(h);
    }

}

