/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)bt_utils.c	8.8 (Berkeley) 7/20/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "btree.h"

/*
 * __bt_ret --
 *	Build return key/data pair.
 *
 * Parameters:
 *	t:	tree
 *	e:	key/data pair to be returned
 *	key:	user's key structure (NULL if not to be filled in)
 *	rkey:	memory area to hold key
 *	data:	user's data structure (NULL if not to be filled in)
 *	rdata:	memory area to hold data
 *       copy:	always copy the key/data item
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
int
__bt_ret (BTREE *t, EPG *e, DBT *key, DBT *rkey, DBT*data, DBT*rdata, int copy)
{
	BLEAF *bl;
	void *p;

	bl = GETBLEAF(e->page, e->index);

	/*
	 * We must copy big keys/data to make them contigous.  Otherwise,
	 * leave the page pinned and don't copy unless the user specified
	 * concurrent access.
	 */
	if (key == NULL)
		goto dataonly;

    if (copy || F_ISSET(t, B_DB_LOCK)) {
		if (bl->ksize > rkey->size) {
			p = (void *)(rkey->data == NULL ?
			    malloc(bl->ksize) : realloc(rkey->data, bl->ksize));
			if (p == NULL)
				return (RET_ERROR);
			rkey->data = p;
			rkey->size = bl->ksize;
		}
		memmove(rkey->data, bl->bytes, bl->ksize);
		key->size = bl->ksize;
		key->data = rkey->data;
	} else {
		key->size = bl->ksize;
		key->data = bl->bytes;
	}

dataonly:
	if (data == NULL)
		return (RET_SUCCESS);

	if (copy || F_ISSET(t, B_DB_LOCK)) {
		/* Use +1 in case the first record retrieved is 0 length. */
		if (bl->dsize + 1 > rdata->size) {
			p = (void *)(rdata->data == NULL ?
			    malloc(bl->dsize + 1) :
			    realloc(rdata->data, bl->dsize + 1));
			if (p == NULL)
				return (RET_ERROR);
			rdata->data = p;
			rdata->size = bl->dsize + 1;
		}
		memmove(rdata->data, bl->bytes + bl->ksize, bl->dsize);
		data->size = bl->dsize;
		data->data = rdata->data;
	} else {
		data->size = bl->dsize;
		data->data = bl->bytes + bl->ksize;
	}

	return (RET_SUCCESS);
}

/*
 * __BT_CMP -- Compare a key to a given record.
 *
 * Parameters:
 *	t:	tree
 *	k1:	DBT pointer of first arg to comparison
 *	e:	pointer to EPG for comparison
 *
 * Returns:
 *	< 0 if k1 is < record
 *	= 0 if k1 is = record
 *	> 0 if k1 is > record
 */
int
__bt_cmp(t, k1, e)
	BTREE *t;
	const DBT *k1;
	EPG *e;
{
	BINTERNAL *bi;
	BLEAF *bl;
	DBT k2;
	PAGE *h;
	void *bigkey;

	/*
	 * The left-most key on internal pages, at any level of the tree, is
	 * guaranteed by the following code to be less than any user key.
	 * This saves us from having to update the leftmost key on an internal
	 * page when the user inserts a new key in the tree smaller than
	 * anything we've yet seen.
     *
     * ??? what does that mean?
	 */
	h = e->page;
	if (e->index == 0 && h->prevpg == P_INVALID && !(h->flags & P_BLEAF))
		return (1);

	bigkey = NULL;
	if (h->flags & P_BLEAF) {
		bl = GETBLEAF(h, e->index);
		if (bl->flags & P_BIGKEY)
			bigkey = bl->bytes;
		else {
			k2.data = bl->bytes;
			k2.size = bl->ksize;
		}
	} else {
		bi = GETBINTERNAL(h, e->index);
		if (bi->flags & P_BIGKEY)
			bigkey = bi->bytes;
		else {
			k2.data = bi->bytes;
			k2.size = bi->ksize;
		}
	}

	if (bigkey) {
        err_quit("not support big key yet");
	}
	return (__bt_defcmp(k1, &k2));
	//return ((*t->bt_cmp)(k1, &k2));
}

/*
 * __BT_DEFCMP -- Default comparison routine.
 *
 * Parameters:
 *	a:	DBT #1
 *	b: 	DBT #2
 *
 * Returns:
 *	< 0 if a is < b
 *	= 0 if a is = b
 *	> 0 if a is > b
 *
 * XXX suppose the machine is  little-endian
 */
int
__bt_defcmp(a, b)
	const DBT *a, *b;
{
	register size_t len;
	register u_char *p1, *p2;
    int i;

	//len = MIN(a->size, b->size);
    p1 = (u_char*) a->data + a->size -1 ;
    p2 = (u_char*) b->data + b->size -1 ;
    len = a->size;
    i = a->size - b->size;
    if(i>0){
        len = b->size;
        for ( ; i>0; i--,p1-- )
            if( *(u_char*)p1 > 0 ) return 1;
    
    }else if(i<0){
        i=-i;
        for( ; i>0; i--,p2-- ) 
            if( *(u_char*)p2 > 0 ) return -1;
    
    }

	for ( ; len--; --p1, --p2)
		if (*p1 != *p2)
			return ((int)*p1 - (int)*p2);
	return ((int)a->size - (int)b->size);
}

/* Mpool_put - the same with Mpool_put except that it will check wheter page is just P_LMEM first */
int Mpool_put( MPOOL *mp, void *page, u_int flags)
{

    if( ((PAGE*)page)->flags &  P_LMEM ){
	    return (RET_SUCCESS);
    }
    return mpool_put( mp, page, flags);
}

/* Toss any page pinned across calls. */
void bt_tosspinned(BTREE* t){
	if (t->bt_pinned != NULL) {
		Mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}
    return;
}

/* Is there enough room to insert data with nbytes */
int is_enough_room(PAGE* h , u_int32_t nbytes){
    return (h->upper - h->lower) >= (nbytes + sizeof(indx_t));
}
/** 
 * makeroom - move from postion skip in the and make room for new entry
 *
 * @h: page
 * @skip: postion to insert
 * @nbytes: number of bytes of insert key/data 
 *
 * @return: pointer to the insert position of key/data
 */
char * makeroom(PAGE*h, indx_t skip, u_int32_t nbytes){
    indx_t nxtindex;
    char* dest;
    /* move to make room for the new (key,pointer) pair */
    if (skip < (nxtindex = NEXTINDEX(h))){
            memmove(h->linp + skip + 1, h->linp + skip,
                (nxtindex - skip) * sizeof(indx_t));
    }
    /* insert key into the skip */
    h->lower += sizeof(indx_t);
    h->linp[skip] = h->upper -= nbytes;
    dest = (char *)h + h->linp[skip];
    return dest;
}

/**
 * shrinkroom - shrink the room to delete the key
 *
 * @h: page
 * @skip:
 * @nbytes: number of bytes of delete key/data
 */

void shrinkroom(PAGE*h, indx_t index, u_int32_t nbytes){
	char *from;
	indx_t cnt, *ip, offset;
	
    /* Pack the remaining key/data items at the end of the page. */
    from = (char *)h + h->upper;
	memmove(from + nbytes, from, h->linp[index] - h->upper);
	h->upper += nbytes;

	/* Adjust the indices' offsets, shift the indices down. */
	offset = h->linp[index];
	for (cnt = index, ip = &h->linp[0]; cnt--; ++ip)
		if (ip[0] < offset)
			ip[0] += nbytes;
	for (cnt = NEXTINDEX(h) - index; --cnt; ++ip)
		ip[0] = ip[1] < offset ? ip[1] + nbytes : ip[1];
	h->lower -= sizeof(indx_t);
}
    
