#include "btree.h"
/*use an arry to implement NTT first*/
#define NTT_MAXSIZE 128 
static NTTEntry NTT[NTT_MAXSIZE];

NTTEntry* NTT_get(pgno_t pgno){
    if( pgno<=0 || pgno>NTT_MAXSIZE){
       err_quit("pgno > NTT_MAXSIZE OR pgno<=0"); 
    }
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
//TODO:
void NTT_add(PAGE* pg){
    const char * err_loc = "function (NTT_add) in 'node.c'";
    pgno_t pgno = pg->pgno;
    NTTEntry* entry = NTT_get(pgno);
    /* XXX free NTT's Sector List */
    if(pg->flags & P_BLEAF){
        entry->flags = NODE_DISK | NODE_LEAF;
    }
    else if(pg->flags & P_BINTERNAL){
        /* FIXME it should be ? */
        entry->flags = NODE_INTERNAL | NODE_DISK;
    }
    else{
        err_quit("unknown flags: %s",err_loc);
    }
    entry->logVersion = -1;
    entry->maxSeq= -1;
    INIT_LIST_HEAD(&(NTT[pgno].list.list));

}

