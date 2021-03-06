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
static char sccsid[] = "@(#)bt_split.c	8.9 (Berkeley) 7/26/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "btree.h"

static int	 bt_broot __P((BTREE *, PAGE *, PAGE *, PAGE *));
static PAGE	*bt_page
		    __P((BTREE *, PAGE *, PAGE **, PAGE **, indx_t *, size_t));
static int	 bt_preserve __P((BTREE *, pgno_t));
static PAGE	*bt_psplit
		    __P((BTREE *, PAGE *, PAGE *, PAGE *, indx_t *, size_t));
static PAGE	*bt_root
		    __P((BTREE *, PAGE *, PAGE **, PAGE **, indx_t *, size_t));

#ifdef STATISTICS
u_long	bt_rootsplit, bt_split, bt_sortsplit, bt_pfxsaved;
#endif

/*
 * __BT_SPLIT_ST -- Split the tree.
 *
 * Parameters:
 *	t:	tree
 *	sp:	page to split
 *	key:	key to insert
 *	data:	data to insert
 *	flags:	BIGKEY/BIGDATA flags
 *	ilen:	insert length
 *	skip:	index to leave open
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__bt_split_st(BTREE *t, PAGE *sp, const DBT *key, const DBT *data, int flags, size_t ilen, u_int32_t argskip)
{
	BINTERNAL *bi=NULL;
	BLEAF *bl=NULL;
	EPGNO *parent;
	PAGE *h, *l, *r, *lchild, *rchild;
	indx_t nxtindex;
	u_int16_t skip;
	u_int32_t nbytes;
	char *dest;

	skip = argskip; /* @mx why not use argskip directly ? */
	/* ----
     * = Step 1. create a new node 'r' =
     * ----
	 * Split the page into two pages, l and r.  The split routines return
	 * a pointer to the page into which the key should be inserted and with
	 * skip set to the offset which should be used.  Additionally, l and r
	 * are pinned.
	 */
    /* h is page to insert data */
    if(sp->nid == P_ROOT){
	    h = bt_root(t, sp, &l, &r, &skip, ilen);
    }else{
	    h = bt_page(t, sp, &l, &r, &skip, ilen);
        sp = l;//sp指向的指针变化了，太ugly了，逼得我用中文了
    }
	if (h == NULL)
		return (RET_ERROR);

	/* ----
     * = Step 2. insert new key/data pair into leaf =
     * ----
	 * Insert the new key/data pair into the leaf page.  (Key inserts
	 * always cause a leaf page to split first.)
     */
	h->linp[skip] = h->upper -= ilen;
	dest = (char *)h + h->upper;
	WR_BLEAF(dest, key, data, flags)
    if(h->flags & P_LMEM){
        if( h == l){
            if(sp->nid == P_ROOT)
                l = h = node_tolog(t,h);
            else
                sp= l = h = node_tolog(t,h);
        }
        else{
            assert(h==r);
            r = h = node_tolog(t,h); 
        }
    }

	/* If the root page was split, make it look right. */
	if (sp->nid == P_ROOT &&
	    bt_broot(t, sp, l, r) == RET_ERROR)
		goto err2;


	
    /**
     * ----
     * = Step 3. update parent node's pointer or split parent node if necessary =
     * ----
     * {{{
	 * Now we walk the parent page stack -- a LIFO stack of the pages that
	 * were traversed when we searched for the page that split.  Each stack
	 * entry is a page number and a page index offset.  The offset is for
	 * the page traversed on the search.  We've just split a page, so we
	 * have to insert a new key into the parent page.
	 *
	 * If the insert into the parent page causes it to split, may have to
	 * continue splitting all the way up the tree.  We stop if the root
	 * splits or the page inserted into didn't have to split to hold the
	 * new key.  Some algorithms replace the key for the old page as well
	 * as the new page.  We don't, as there's no reason to believe that the
	 * first key on the old page is any better than the key we have, and,
	 * in the case of a key being placed at index 0 causing the split, the
	 * key is unavailable.
	 *
	 * There are a maximum of 5 pages pinned at any time.  We keep the left
	 * and right pages pinned while working on the parent.   The 5 are the
	 * two children, left parent and right parent (when the parent splits)
	 * and the root page or the overflow key page when calling bt_preserve.
	 * This code must make sure that all pins are released other than the
	 * root page or overflow page which is unlocked elsewhere.
     * }}}
	 */
	while ((parent = BT_POP(t)) != NULL) {

        lchild = l;
		rchild = r; /* @mx use rchild to save value of r, because r is used to point to the new splited page,later. */
        
	    /* ----
         * == Step 3.1. get the parent page and calculate the space needed on the parent page. ==
         * ----
         */
        err_debug(("-^Update parent node %u", parent->pgno)); 
        err_debug(("--^Read node %u", parent->pgno)); 
        h = read_node(t,parent->pgno);
        err_debug(("--$End Read"));
        assert(h!=NULL);
        /*
         * The new key goes ONE AFTER the index, because the split
		 * was to the right.
		 */
		skip = parent->index + 1;

		/*
         * Calculate the space needed on the parent page. 
		 */
		switch (rchild->flags & P_TYPE) {
		case P_BINTERNAL:
			bi = GETBINTERNAL(rchild, 0);
			nbytes = NBINTERNAL(bi->ksize);
			break;
		case P_BLEAF:
			bl = GETBLEAF(rchild, 0);
			nbytes = NBINTERNAL(bl->ksize);
			break;
		default:
			abort();
		}
             
	    /* ----
         * == Step 3.2. insert new key to the parent page ==
         * ----
		 * Split the parent page if necessary or shift the indices. 
         */

        /* === Case 1. shift the indices. === */
		if (h->upper - h->lower >= nbytes + sizeof(indx_t)) {
            DBT tkey;
            err_debug(("shift the parent page"));    
            /* Insert the key into the parent page. */
            switch (rchild->flags & P_TYPE) {
            case P_BINTERNAL:
                tkey.size = bi->ksize;
                tkey.data = bi->bytes;
                break;
            case P_BLEAF:
                assert(!(bl->flags & P_BIGKEY));
                tkey.size = bl->ksize;
                tkey.data = bl->bytes;
                break;
            default:
                abort();
            }
            node_addkey(t,h,&tkey,NULL,rchild->nid,skip,nbytes);
			break;
		} 
        /* === Case 2. Split the parent page === */
        err_debug(("split the parent page"));    
        sp = h;
        
        if(sp->nid == P_ROOT){
            h = bt_root(t, sp, &l, &r, &skip, nbytes);
        }else{
            h = bt_page(t, sp, &l, &r, &skip, nbytes);
            sp = l;
        }
        if (h == NULL)
            goto err1;


		h->linp[skip] = h->upper -= nbytes;
		dest = (char *)h + h->linp[skip];
		/* Insert the key into the parent page. */
		switch (rchild->flags & P_TYPE) {
		case P_BINTERNAL:
			memmove(dest, bi, nbytes);
			((BINTERNAL *)dest)->pgno = rchild->nid;
			break;
		case P_BLEAF:
            assert(!(bl->flags & P_BIGKEY));
			WR_BINTERNAL_OLD(dest, bl->ksize,
			    rchild->nid, bl->flags & P_BIGKEY);
			memmove(dest, bl->bytes, bl->ksize);
			break;
		default:
			abort();
		}

        
        if(h->flags & P_LMEM){
            if( h == l){
                if(sp->nid == P_ROOT)
                    l = h = node_tolog(t,h); 
                else    
                    sp = l = h = node_tolog(t,h); 
            }
            else{
                assert(h==r);
                r = h = node_tolog(t,h); 
            }
        }
		/* If the root page was split, make it look right. */
		if (sp->nid == P_ROOT &&
		    bt_broot(t, sp, l, r) == RET_ERROR)
			goto err1;

		//Mpool_put(t->bt_mp, h, MPOOL_DIRTY);

		//Mpool_put(t->bt_mp, lchild, MPOOL_DIRTY);
		//Mpool_put(t->bt_mp, rchild, MPOOL_DIRTY);
		Mpool_put(t->bt_mp, lchild, MPOOL_DIRTY);
		Mpool_put(t->bt_mp, rchild, MPOOL_DIRTY);

        err_debug(("-^End update", parent->pgno)); 
	}

	/* Unpin the held pages. */
	Mpool_put(t->bt_mp, l, MPOOL_DIRTY);
	Mpool_put(t->bt_mp, r, MPOOL_DIRTY);

	/* Clear any pages left on the stack. */
    BT_CLR(t);
	return (RET_SUCCESS);

	/*
	 * If something fails in the above loop we were already walking back
	 * up the tree and the tree is now inconsistent.  Nothing much we can
	 * do about it but release any memory we're holding.
	 */
err1:	Mpool_put(t->bt_mp, lchild, MPOOL_DIRTY);
	Mpool_put(t->bt_mp, rchild, MPOOL_DIRTY);

err2:	Mpool_put(t->bt_mp, l, 0);
	Mpool_put(t->bt_mp, r, 0);
	__dbpanic(t->bt_dbp);
	return (RET_ERROR);
}

/*
 * BT_PAGE -- Split a non-root page of a btree.
 *
 * Parameters:
 *	t:	tree
 *	h:	root page
 *	lp:	pointer to left page pointer
 *	rp:	pointer to right page pointer
 *	skip:	pointer to index to leave open
 *	ilen:	insert length
 *
 * Returns:
 *	Pointer to page in which to insert or NULL on error.
 * 
 * @mx split page h into (h,r), l is just for tmp use
 *	*lp = h;
 *	*rp = r;
 */
static PAGE *
bt_page ( BTREE *t,  PAGE *h, PAGE **lp, PAGE **rp,  indx_t *skip, size_t ilen)
{
	PAGE *l, *r, *tp;
	pgno_t npg;

#ifdef STATISTICS
	++bt_split;
#endif
    
	/* ----
     * = Step 1. create a new node  =
     * ----
     */
	/* Put the new right page for the split into place. */
	if ((r = new_node(t, &npg, h->flags & P_TYPE)) == NULL)
		return (NULL);
	r->nextpg = h->nextpg;
	r->prevpg = h->nid;
	/* ----
     * = Step 2. split node h =
     * ----
     */
    /*
     * ==  Case 1: rightmost page and rightmost index  ==
     * {{{
     * Log Mode:
     *  there's nothing to do here
     * Disk Mode:
	 *  If we're splitting the last page on a level because we're appending
	 * a key to it (skip is NEXTINDEX()), it's likely that the data is
	 * sorted.  Adding an empty page on the side of the level is less work
	 * and can push the fill factor much higher than normal.  If we're
	 * wrong it's no big deal, we'll just do the split the right way next
	 * time.  It may look like it's equally easy to do a similar hack for
	 * reverse sorted data, that is, split the tree left, but it's not.
	 * Don't even try.
	 */
	if (h->nextpg == P_INVALID && *skip == NEXTINDEX(h)) {
#ifdef STATISTICS
		++bt_sortsplit;
#endif
		h->nextpg = r->nid;
		r->lower = BTDATAOFF + sizeof(indx_t);
		*skip = 0;
		*lp = h;
		*rp = r;
		return (r);
	}
    //}}}

    /* 
     * == Case 2: normal case ==
     * @mx Note: l is just for tmp usage, so we don't use __bt_new, instead we just use malloc and then free it
     */
	/* Put the new left page for the split into place. */
	if ((l = (PAGE *)malloc(t->bt_psize)) == NULL) {
		Mpool_put(t->bt_mp, r, 0);
		return (NULL);
	}
#ifdef PURIFY
	memset(l, 0xff, t->bt_psize);
#endif
	l->nid = h->nid;
	l->pgno = h->pgno;
	l->nextpg = r->nid;
	l->prevpg = h->prevpg;
	l->lower = BTDATAOFF;
	l->upper = t->bt_psize;
	l->flags = h->flags;
	/* Fix up the previous pointer of the page after the split page. */
	if (h->nextpg != P_INVALID) {
		if ((tp = mpool_get(t->bt_mp, h->nextpg, 0)) == NULL) {
			free(l);
			/* XXX mpool_free(t->bt_mp, r->pgno); */
            return (NULL);
		}
		tp->prevpg = r->nid;
		Mpool_put(t->bt_mp, tp, MPOOL_DIRTY);
	}

	/*
	 * Split right.  The key/data pairs aren't sorted in the btree page so
	 * it's simpler to copy the data from the split page onto two new pages
	 * instead of copying half the data to the right page and compacting
	 * the left page in place.  Since the left page can't change, we have
	 * to swap the original and the allocated left page after the split.
     *
     * @mx tp = 'l' OR 'r'
	 */
    //XXX assume all internal node 
    
	tp = bt_psplit(t, h, l, r, skip, ilen);

	/* Move the new left page onto the old left page. */
	memmove(h, l, t->bt_psize);
	if (tp == l){
		tp = h;
        if(r->flags & P_LMEM){
            err_debug(("gen for left")); 
            r = node_tolog(t,r);
        }
    }
    else{  /* tp==r  */
        if(h->flags & P_LMEM){
            err_debug(("gen for left")); 
            h = node_tolog(t,h);
        }
    }
	free(l);

	*lp = h;
	*rp = r;
	return (tp);
}

/*
 * BT_ROOT -- Split the root page of a btree.
 *
 * Parameters:
 *	t:	tree
 *	h:	root page
 *	lp:	pointer to left page pointer
 *	rp:	pointer to right page pointer
 *	skip:	pointer to index to leave open
 *	ilen:	insert length
 *
 * Returns:
 *	Pointer to page in which to insert or NULL on error.
 * 
 */
static PAGE *
bt_root(t, h, lp, rp, skip, ilen)
	BTREE *t;
	PAGE *h, **lp, **rp;
	indx_t *skip;
	size_t ilen;
{
	PAGE *l, *r, *tp;
	pgno_t lnpg, rnpg;

#ifdef STATISTICS
	++bt_split;
	++bt_rootsplit;
#endif
    /* Put the new left and right pager s for the split into place. */
	if ((l = new_node(t, &lnpg, h->flags & P_TYPE)) == NULL ||
	    (r = new_node(t, &rnpg, h->flags & P_TYPE)) == NULL)
		return (NULL);
	l->nextpg = r->nid;
	r->prevpg = l->nid;

	/* Split the root page. */
	tp = bt_psplit(t, h, l, r, skip, ilen);


	if (tp == l){
        if(r->flags & P_LMEM) r = node_tolog(t,r);
    }
    else{  /* tp==r  */
        assert(tp==r);
        if(l->flags & P_LMEM) l = node_tolog(t,l);
    }

	*lp = l;
	*rp = r;
	return (tp);
}


/*
 * BT_BROOT -- Fix up the btree root page after it has been split.
 *
 * Parameters:
 *	t:	tree
 *	h:	root page
 *	l:	left page
 *	r:	right page
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 * 
 * @mx the reason why we need to fix it is that you need to rebuild the root's content
 * it's differnt from normal situation that we only need to update parent node
 */
static int
bt_broot(t, h, l, r)
	BTREE *t;
	PAGE *h, *l, *r;
{
	BINTERNAL *bi;
	BLEAF *bl;
	u_int32_t nbytes;
	char *dest;
    NTTEntry* entry;
    entry = NTT_get(h->nid);
	/*
	 * If the root page was a leaf page, change it into an internal page.
	 * We copy the key we split on (but not the key's data, in the case of
	 * a leaf page) to the new root page.
     *
	 * @mx It means we have to modify the mode of NTT
     *
	 * The btree comparison code guarantees that the left-most key on any
	 * level of the tree is never used, so it doesn't need to be filled in.
	 */
	nbytes = NBINTERNAL(0);
	h->linp[0] = h->upper = t->bt_psize - nbytes;
	dest = (char *)h + h->upper;
	WR_BINTERNAL_OLD(dest, 0, l->nid, 0);

	switch (h->flags & P_TYPE) {
	case P_BLEAF:
		bl = GETBLEAF(r, 0);
		nbytes = NBINTERNAL(bl->ksize);
		h->linp[1] = h->upper -= nbytes;
		dest = (char *)h + h->upper;
		WR_BINTERNAL_OLD(dest, bl->ksize, r->nid, 0);
		memmove(dest, bl->bytes, bl->ksize);
        /* XXX we suppose all the internal node is log mode */
        /* TODO: NTT sector list should be changed */
#ifdef NTT_DEBUG
        err_debug(("Change root into (INTERNAL|LOG)"));
#endif
        NTT_del_list(entry);
        entry->flags = P_BINTERNAL | P_LOG;
        entry->logversion ++;
        entry->maxSeq = 0;
        entry->pg_cnt = 0;

        assert(!(bl->flags & P_BIGKEY));
	case P_BINTERNAL:
		bi = GETBINTERNAL(r, 0);
		nbytes = NBINTERNAL(bi->ksize);
		h->linp[1] = h->upper -= nbytes;
		dest = (char *)h + h->upper;
		memmove(dest, bi, nbytes);
		((BINTERNAL *)dest)->pgno = r->nid;
		break;
	default:
		abort();
	}

	/* There are two keys on the page. */
	h->lower = BTDATAOFF + 2 * sizeof(indx_t);

	/* Unpin the root page, set to btree internal page. */
	h->flags &= ~P_TYPE;
	h->flags |= P_BINTERNAL;
    if(h->flags & P_LMEM){
        h = node_tolog(t,h);
    }
	Mpool_put(t->bt_mp, h, MPOOL_DIRTY);
	return (RET_SUCCESS);
}

/*
 * BT_PSPLIT -- Do the real work of splitting the page.
 *
 * Parameters:
 *	t:	tree
 *	h:	page to be split
 *	l:	page to put lower half of data
 *	r:	page to put upper half of data
 *	pskip:	pointer to index to leave open
 *	ilen:	insert length
 *
 * Returns:
 *	Pointer to page in which to insert.
 */
static PAGE *
bt_psplit(t, h, l, r, pskip, ilen)
	BTREE *t;
	PAGE *h, *l, *r;
	indx_t *pskip;
	size_t ilen;
{
	BINTERNAL *bi=NULL;
	BLEAF *bl=NULL;
	PAGE *rval;
	void *src=NULL;
	indx_t full, half, nxt, off, skip, top, used;
	u_int32_t nbytes;
	int bigkeycnt, isbigkey;


	/*
	 * Split the data to the left and right pages.  Leave the skip index
	 * open.  Additionally, make some effort not to split on an overflow
	 * key.  This makes internal page processing faster and can save
	 * space as overflow keys used by internal pages are never deleted.
	 */
	bigkeycnt = 0;
	skip = *pskip;
	full = t->bt_psize - BTDATAOFF;
	half = full / 2;
	used = 0;
	for (nxt = off = 0, top = NEXTINDEX(h); nxt < top; ++off) {
		if (skip == off) {
			nbytes = ilen;
			isbigkey = 0;		/* XXX: not really known. */
		} else{
			switch (h->flags & P_TYPE) {
			case P_BINTERNAL:
				src = bi = GETBINTERNAL(h, nxt);
				nbytes = NBINTERNAL(bi->ksize);
				isbigkey = bi->flags & P_BIGKEY;
				break;
			case P_BLEAF:
				src = bl = GETBLEAF(h, nxt);
				nbytes = NBLEAF(bl);
				isbigkey = bl->flags & P_BIGKEY;
				break;
			default:
				abort();
			}
        }

		/*
		 * If the key/data pairs are substantial fractions of the max
		 * possible size for the page, it's possible to get situations
		 * where we decide to try and copy too much onto the left page.
		 * Make sure that doesn't happen.
		 */
		if (skip <= off && used + nbytes >= full) {
			--off;
			break;
		}

		/* Copy the key/data pair, if not the skipped index. */
		if (skip != off) {
			++nxt;

			l->linp[off] = l->upper -= nbytes;
			memmove((char *)l + l->upper, src, nbytes);
		}

		used += nbytes;
		if (used >= half) {
			if (!isbigkey || bigkeycnt == 3)
				break;
			else
				++bigkeycnt;
		}
	}

	/*
	 * Off is the last offset that's valid for the left page.
	 * Nxt is the first offset to be placed on the right page.
	 */
	l->lower += (off + 1) * sizeof(indx_t);

	/*
	 * If the skipped index was on the left page, just return that page.
	 * Otherwise, adjust the skip index to reflect the new position on
	 * the right page.
	 */
	if (skip <= off) {
		skip = 0;
		rval = l;
	} else {
		rval = r;
		*pskip -= nxt;
	}

	for (off = 0; nxt < top; ++off) {
		if (skip == nxt) {
			++off;
			skip = 0;
		}
		switch (h->flags & P_TYPE) {
		case P_BINTERNAL:
			src = bi = GETBINTERNAL(h, nxt);
			nbytes = NBINTERNAL(bi->ksize);
			break;
		case P_BLEAF:
			src = bl = GETBLEAF(h, nxt);
			nbytes = NBLEAF(bl);
			break;
		default:
			abort();
		}
		++nxt;
		r->linp[off] = r->upper -= nbytes;
		memmove((char *)r + r->upper, src, nbytes);
	}
	r->lower += off * sizeof(indx_t);

	/* If the key is being appended to the page, adjust the index. */
	if (skip == top)
		r->lower += sizeof(indx_t);

	return (rval);
}

/*
 * BT_PRESERVE -- Mark a chain of pages as used by an internal node.
 *
 * Chains of indirect blocks pointed to by leaf nodes get reclaimed when the
 * record that references them gets deleted.  Chains pointed to by internal
 * pages never get deleted.  This routine marks a chain as pointed to by an
 * internal page.
 *
 * Parameters:
 *	t:	tree
 *	pg:	page number of first page in the chain.
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
static int
bt_preserve(t, pg)
	BTREE *t;
	pgno_t pg;
{
	PAGE *h;

	if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
		return (RET_ERROR);
	h->flags |= P_PRESERVE;
	Mpool_put(t->bt_mp, h, MPOOL_DIRTY);
	return (RET_SUCCESS);
}

