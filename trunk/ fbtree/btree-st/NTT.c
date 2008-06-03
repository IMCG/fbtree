#include "btree.h"
/*use an arry to implement NTT first*/
#define NTT_MAXSIZE 256 
//static NTTEntry NTT[NTT_MAXSIZE];
static NTTEntry NTT[NTT_MAXSIZE];
/**
 * NTT_get - Get the NTTentry by pgno
 * @pgno - page no of the entry (i.e. node id)
 */
NTTEntry* NTT_get(pgno_t pgno){
    //const char* err_loc = "function (NTT_get) in NTT.c";
    //TODO-DEBUG
    err_debug("pgno = %ud\n", pgno);
    assert( pgno > 0 && pgno<= NTT_MAXSIZE);
    
    return &NTT[pgno];

}
/* It seems using NTTentry directly is better */
#if 0
u_int32_t NTT_getLogVersion(pgno_t pgno){
    if( pgno<=0 || pgno>NTT_MAXSIZE){
       err_quit("pgno > NTT_MAXSIZE OR pgno<=0"); 
    }
    return NTT[pgno].logVersion;
}
u_int32_t NTT_getMaxSeq(pgno_t pgno){
    if( pgno<=0 || pgno>NTT_MAXSIZE){
       err_quit("pgno > NTT_MAXSIZE OR pgno<=0"); 
    }
    return NTT[pgno].maxSeq;
}
#endif
/**
 * NTT_add - Add a new node to the NTT
 * @pg - page header of the new node
 *
 * For log mode node, we construct pg as an identifier of node
 */
void NTT_add(PAGE* pg){
    const char * err_loc = "function (NTT_add) in 'NTT.c'";
    pgno_t pgno = pg->pgno;
    NTTEntry* entry;

#ifdef NTT_DEBUG
    err_debug("add PAGE %ud to the NTT ",pgno); /* Just show a message, not *error* */
#endif
    entry = NTT_get(pgno);
    /* XXX free NTT's Sector List */
    if(pg->flags & P_BLEAF){
        entry->flags = NODE_DISK | NODE_LEAF;
    }
    else if(pg->flags & P_BINTERNAL){
        /* FIXME it should be ? */
        entry->flags = NODE_INTERNAL | NODE_LOG;
    }
    else{
        err_quit("unknown flags: %s",err_loc);
    }
    entry->logVersion = -1;
    entry->maxSeq= -1;
    INIT_LIST_HEAD(&(NTT[pgno].list.list));

}

/**
 * NTT_add_pgno - Add pgno to the sector list of NTT[nodeID]
 * @nodeID: page no of the node
 * @pgno: pgno to insert into
 *
 * if the pgno exist in the node, we abort or do nothing 
 *
 */
void NTT_add_pgno(pgno_t nodeID, pgno_t pgno){
    const char * err_loc = "function (NTT_add_pgno) in 'NTT.c'";
    NTTEntry* entry = NTT_get(nodeID);
    SectorList* slist = & entry->list;
    SectorList* nslist;
    
    /* Iterate Sector List to check whether there's duplicate pgno. 
     * It will be too slow if we check it every time. 
     */
#ifdef NTT_DEBUG
    list_for_each_entry(nslist,&slist->list,list){
        assert(nslist->pgno!=pgno);
    }
#endif

    nslist= malloc(sizeof(SectorList));
    if(nslist==NULL)    err_sys("error while malloc sector list: %s",err_loc);
    nslist->pgno = pgno;
    list_add(&(nslist->list),&(slist->list));
}

