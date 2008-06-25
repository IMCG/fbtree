#include "btree.h"
/**
 * mem2disk - convert a MEM mode into DISK mode
 * @pg: the node's page header
 *
 * we convert the real node into a disk mode mode
 */
PAGE* mem2disk(BTREE* t, PAGE* pg)
{
    PAGE* npg;
    u_int32_t npgno;
	assert(pg->flags & P_LMEM);

    npg = __bt_new(t,&npgno);
    memmove(npg,pg, t->bt_psize);
    npg->pgno = npgno;
    npg->flags = (pg->flags & P_TYPE) | P_DISK;
    
    NTT_switch(npg->nid);
    NTT_add_pgno(npg->nid,npg->pgno);
    free(pg);
    Mpool_put(t->bt_mp,npg,MPOOL_DIRTY);
    npg = mpool_get(t->bt_mp,npgno,0);
    
    return npg;

}
/**
 * mem2log - generate log entries from a MEM mode node AND put them into log buffer.
 * @pg: the node's page header
 *
 * we convert the real node into a set of log entries
 */
PAGE* mem2log(BTREE* t, PAGE* pg)
{
    unsigned int i;
    BINTERNAL* bi=NULL;
    BLEAF* bl=NULL;
    NTTEntry* e;
    DBT key,data;
    indx_t nxtidx;
	PAGE* npg;

    assert( (pg->flags & P_LMEM) || (pg->flags & P_DISK) );
    if( pg->flags & P_DISK ){
		
        npg = (PAGE*)malloc(t->bt_psize);
		memmove(npg,pg, t->bt_psize);
		npg->pgno = P_INVALID;
        npg->flags = (pg->flags & P_TYPE) | P_LMEM;

		NTT_switch(npg->nid);
		__bt_free(t,pg); // NOTE: bt_free won't free pg if pg is set to P_LMEM
        pg = npg;
    }
    nxtidx = NEXTINDEX(pg);
    if( nxtidx==0 ){
        return pg;
    }
    err_debug(("~^Generate log entry from node %u",pg->nid));

    e = NTT_get(pg->nid);
    e->logversion++;
    if(pg->flags & P_BINTERNAL){
        for (i=0; i<nxtidx; i++){
            bi = GETBINTERNAL(pg,i);
            assert(bi!=NULL);
            key.size = bi->ksize;
            key.data = bi->bytes;
            logpool_put(t,pg->nid,&key,NULL, bi->pgno,ADD_KEY|LOG_INTERNAL);
        }
    }else{
        assert(pg->flags & P_BLEAF);
        for (i=0; i<nxtidx; i++){
            bl = GETBLEAF(pg,i);
            assert(bl!=NULL);
            key.size = bl->ksize;
            key.data = bl->bytes;
            data.size = bl->dsize;
            data.data = bl->bytes + bl->ksize;
            logpool_put(t,pg->nid,&key,&data,P_INVALID,ADD_KEY|LOG_LEAF);
        }
    }

    err_debug(("~$End Generate"));
    return pg;
}

