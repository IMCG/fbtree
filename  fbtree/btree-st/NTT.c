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
void NTT_add(PAGE* pg){
    pgno_t pgno = pg->pgno;
    /* XXX free NTT's Sector List */
    NTT[pgno].isLeaf = (pg->flags & P_BLEAF);
    NTT[pgno].logVersion = -1;
    INIT_LIST_HEAD(&(NTT[pgno].list.list));

}


