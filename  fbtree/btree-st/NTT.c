#include "btree.h"
#include "list.h"
/*use an arry to implement NTT first*/
#define NTT_MAXSIZE 102400 
//static NTTEntry NTT[NTT_MAXSIZE];
static NTTEntry NTT[NTT_MAXSIZE];

/**
 * NTT_init - Initialize the Node Translation Table
 *
 * @return: RET_SUCCESS if initialize successfully.
 *
 * NOTE: NTT[0] is not used
 */
int NTT_init(){
    int i;
    for(i=1; i<NTT_MAXSIZE; i++){
        NTT[i].flags = P_NOTUSED;
        NTT[i].logVersion = 0;
    }
}
/**
 * NTT_get - Get the NTTentry by pgno
 * @pgno - page no of the entry (i.e. node id)
 */
NTTEntry* NTT_get(pgno_t pgno){
    //const char* err_loc = "function (NTT_get) in NTT.c";
    //err_debug1("pgno = %u", pgno);
    assert( pgno > 0 && pgno<= NTT_MAXSIZE);
    
    return &NTT[pgno];

}
/**
 * NTT_add - Add a new node[nid] to the NTT
 * @nid - node id 
 * @pg - page header of the new node
 *
 * For log mode node, we construct pg as an identifier of node
 */
void NTT_add(pgno_t nid, PAGE* pg){
    const char * err_loc = "(NTT_add) in 'NTT.c'";
    NTTEntry* entry;

    entry = NTT_get(nid);
    
    /* XXX free NTT's Sector List */
    if(pg->flags & P_MEM){
        entry->flags = pg->flags | P_LOG ;
    }else{
        entry->flags = pg->flags | P_DISK ;
    }
    entry->logVersion = 0;
    entry->maxSeq= 0;
    INIT_LIST_HEAD(&(NTT[nid].list.list));

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
    const char * err_loc = "(NTT_add_pgno) in 'NTT.c'";
    NTTEntry* entry = NTT_get(nodeID);
    SectorList* slist = & entry->list;
    SectorList* nslist;
    
    /* Iterate Sector List to check whether there's duplicate pgno. 
     * XXX It will be too slow if we check it every time. 
     */
    list_for_each_entry(nslist,&slist->list,list){
        if(nslist->pgno==pgno) return;
    }
    err_debug(("Add pgno %ud to the sector list of NTT[%ud]",pgno,nodeID));

    nslist= malloc(sizeof(SectorList));
    if(nslist==NULL)    err_sys("error while malloc sector list: %s",err_loc);
    nslist->pgno = pgno;
    list_add(&(nslist->list),&(slist->list));
}
/**
 * delete sector list of the NTTEntry *entry*, and reset NTT
 */
void NTT_del_list(NTTEntry* entry){
    SectorList *slist_entry, *tmp;
    entry->logVersion = 0;
    entry->maxSeq = 0;
    INIT_LIST_HEAD(&(entry->list.list));
    //FIXME : real delete later
#if 0
    list_for_each_entry_safe(slist_entry,tmp,&(entry->list.list),list){
       list_del(slist_entry);
       free(tmp);
    }
#endif

}
void NTT_dump( ){
    NTTEntry* entry;
    SectorList* slist;
    SectorList* slist_entry;
    int i;

    for( i=1; i<NTT_MAXSIZE ; i++){
        entry = NTT_get(i);
        if(entry->flags & P_NOTUSED) continue;
        fprintf(stderr,"NTT[%ud]: ", i);
        fprintf(stderr," logVersion = %ud, ", entry->logVersion);
        fprintf(stderr," maxSeq = %ud, ", entry->maxSeq);
        fprintf(stderr," flags = %s |", (entry->flags & P_LOG) ? "P_LOG": "P_DISK");
        fprintf(stderr," %s, ", (entry->flags & P_BLEAF) ? "P_BLEAF": "P_BINTERNAL");

        slist = & entry->list;
        fprintf(stderr,"slist = (");
        list_for_each_entry(slist_entry,&slist->list,list){
            assert(slist_entry != NULL);
            fprintf(stderr," %ud,",slist_entry->pgno);
        }
        fprintf(stderr,")\n");
    }    
    

}
