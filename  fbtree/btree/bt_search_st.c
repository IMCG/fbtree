
#include <sys/types.h>

#include <stdio.h>


#include "btree.h"

static int __bt_snext __P((BTREE *, PAGE *, const DBT *, int *));
static int __bt_sprev __P((BTREE *, PAGE *, const DBT *, int *));
typedef struct _loglist{
    BINTERNAL_LOG* log;
    struct list_head list;
}LogList;

/*
 * read Node[x]
 *
 */
PAGE* ReadNode(MPOOL*mp , pgno_t x){
    int mode = __NTT_mode(x);
    int version;    /* latest version of current node */
    
    PAGE *h;
    pgno_t pg;
    
    SectorList* head;
    SectorList* slist;
    
    BINTERNAL_LOG * bi_log;
    LogList logCollector;
    
    INIT_LIST_HEAD(&logCollector.head);
    if(mode==DISK){
        h = mpool_get(mp,x,0);
        return h;
    }
    else if(mode==LOG){

        version = __NTT_getVersion(x);
        head = __NTT_getSectorList(x);
        
        // iterate the list
        list_for_each_entry(slist , &(head->list) , list ){
            
            // get the PAGE(pgno)
            h = mpool_get(mp,slist->pgno,0);
            if( h==NULL ){
                // XXX error handling
                return NULL;
            }
            
            // iterate the page to collect entry in this page
            for(i =0 ; i<lim ; i++){
                // get the log entry
                bi_log = GETBINTERNAL_LOG(slist->pgno,i);
                // if it belongs to the node x , collect it
                if( bi_log->nodeID==x){
                    __log_collect(&logCollector,bi_log);
                }
            }
            mpool_put(mp,h,0);
        }
        // construct the actual node of the page
        return rebuildNode(&logCollector);
    }
    else{
       fprintf(stderr,"error , unknown mode\n");
       exit(-1);
    }



}
/*
 * add bi_log into the log list "logCollector"
 */
void __log_collect( LogList* logCollector, BINTERNAL_LOG* bi_log){
    LogList* tmp = (LogList*)malloc(sizeof(LogList));
    BINTERNAL_LOG * bi = (BINTERNAL_LOG*)malloc(NBINTERNAL_LOG(bi_log->ksize));
    WR_BINTERNAL_LOG(bi,bi_log);
    tmp->log = bi;
    list_add(&(tmp->list),&(logCollector->list));

}
/** 
 * rebuildNode - construct the real node from the log entry list
 * @logCollector: the log list of collect log entries
 */
PAGE* rebuildNode(LogList* head){
    pgno_t npg;
    LogList* entry;
    BINTERNAL_LOG* log;
    // sort the log entry by seqnum
    
    // create a new node
    PAGE* newNode=__bt_new(t,&npg);
    if(newNode==NULL){
        fprintf(stderr,"can't create new node.\n");
        return (NULL);
    }
	newNode->pgno = npg;
	newNode->nextpg = r->pgno;
	newNode->prevpg = h->prevpg;
	newNode->lower = BTDATAOFF;
	newNode->upper = t->bt_psize;
	newNode->flags = h->flags & P_TYPE;
    // apply the log entries to construc a node
    list_for_each_entry(entry,&(head->list),list){
        log = entry->log;
        assert(!( (log & BIGKEY) | (log & BIGDATA)));
        if(log & ADD_KEY){
           // TODO 
        }
        else if(log & DELETE_KEY){
        }
        else if(log & UPDATE_POINTER){
        }
    }

    
    return newNode;
} 

/*
 * __bt_search --
 *	Search a btree for a key.
 *
 * Parameters:
 *	t:	tree to search
 *	key:	key to find
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	The EPG for matching record, if any, or the EPG for the location
 *	of the key, if it were inserted into the tree, is entered into
 *	the bt_cur field of the tree.  A pointer to the field is returned.
 */
EPG *
__bt_search_st(BTREE *t,const DBT *key,int *exactp)
{

    /* 
     * 1.Read from root of the btree in NTT
     * 2.reconstruct the node
     */	
    // read node from ROOT
    // loop until we find the right node

    PAGE *h;
	indx_t base, index, lim;
	pgno_t pg;
	int cmp;

	BT_CLR(t);  /* @mx it initializes t->bt_sp  */
	for (pg = P_ROOT;;) {
        h = readNode(t->bt_mp,pg);
        if(h==NULL)
			return (NULL);

		/* Do a binary search on the current page. */
		t->bt_cur.page = h;
		for (base = 0, lim = NEXTINDEX(h); lim; lim >>= 1) {
			t->bt_cur.index = index = base + (lim >> 1);
			if ((cmp = __bt_cmp(t, key, &t->bt_cur)) == 0) {
				if (h->flags & P_BLEAF) {
					*exactp = 1;
					return (&t->bt_cur);
				}
				goto next;
			}
			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		/*
		 * If it's a leaf page, we're almost done.  If no duplicates
		 * are allowed, or we have an exact match, we're done.  Else,
		 * it's possible that there were matching keys on this page,
		 * which later deleted, and we're on a page with no matches
		 * while there are matches on other pages.  If at the start or
		 * end of a page, check the adjacent page.
		 */
		if (h->flags & P_BLEAF) {
			if (!F_ISSET(t, B_NODUPS)) {
				if (base == 0 &&
				    h->prevpg != P_INVALID &&
				    __bt_sprev(t, h, key, exactp))
					return (&t->bt_cur);
				if (base == NEXTINDEX(h) &&
				    h->nextpg != P_INVALID &&
				    __bt_snext(t, h, key, exactp))
					return (&t->bt_cur);
			}
			*exactp = 0;
			t->bt_cur.index = base;
			return (&t->bt_cur);
		}

		/*
		 * No match found.  Base is the smallest index greater than
		 * key and may be zero or a last + 1 index.  If it's non-zero,
		 * decrement by one, and record the internal page which should
		 * be a parent page for the key.  If a split later occurs, the
		 * inserted page will be to the right of the saved page.
		 */
		index = base ? base - 1 : base;

next:		BT_PUSH(t, h->pgno, index);
		pg = GETBINTERNAL(h, index)->pgno;
		mpool_put(t->bt_mp, h, 0);
	}
}

/*
 * __bt_snext --
 *	Check for an exact match after the key.
 *
 * Parameters:
 *	t:	tree
 *	h:	current page
 *	key:	key
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	If an exact match found.
 */
static int
__bt_snext(t, h, key, exactp)
	BTREE *t;
	PAGE *h;
	const DBT *key;
	int *exactp;
{
	EPG e;

	/*
	 * Get the next page.  The key is either an exact
	 * match, or not as good as the one we already have.
	 */
	if ((e.page = mpool_get(t->bt_mp, h->nextpg, 0)) == NULL)
		return (0);
	e.index = 0;
	if (__bt_cmp(t, key, &e) == 0) {
		mpool_put(t->bt_mp, h, 0);
		t->bt_cur = e;
		*exactp = 1;
		return (1);
	}
	mpool_put(t->bt_mp, e.page, 0);
	return (0);
}

/*
 * __bt_sprev --
 *	Check for an exact match before the key.
 *
 * Parameters:
 *	t:	tree
 *	h:	current page
 *	key:	key
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	If an exact match found.
 */
static int
__bt_sprev(t, h, key, exactp)
	BTREE *t;
	PAGE *h;
	const DBT *key;
	int *exactp;
{
	EPG e;

	/*
	 * Get the previous page.  The key is either an exact
	 * match, or not as good as the one we already have.
	 */
	if ((e.page = mpool_get(t->bt_mp, h->prevpg, 0)) == NULL)
		return (0);
	e.index = NEXTINDEX(e.page) - 1;
	if (__bt_cmp(t, key, &e) == 0) {
		mpool_put(t->bt_mp, h, 0);
		t->bt_cur = e;
		*exactp = 1;
		return (1);
	}
	mpool_put(t->bt_mp, e.page, 0);
	return (0);
}
