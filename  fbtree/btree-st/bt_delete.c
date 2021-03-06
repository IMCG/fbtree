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
static char sccsid[] = "@(#)bt_delete.c	8.13 (Berkeley) 7/28/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <db.h>
#include "btree.h"

static int __bt_bdelete __P((BTREE *, const DBT *));
static int __bt_curdel __P((BTREE *, const DBT *, PAGE *, u_int));
static int __bt_pdelete __P((BTREE *, PAGE *));
static int __bt_relink __P((BTREE *, PAGE *));
static int __bt_stkacq __P((BTREE *, PAGE **, CURSOR *));

/*
 * __bt_delete
 *	Delete the item(s) referenced by a key.
 *
 * Return RET_SPECIAL if the key is not found.
 */
int
__bt_delete( const DB *dbp, const DBT *key, u_int flags)
{
	BTREE *t;
	int status;

	t = dbp->internal;

	/* Toss any page pinned across calls. */
    bt_tosspinned(t);     

	/* Check for change to a read-only tree. */
	if (F_ISSET(t, B_RDONLY)) {
		errno = EPERM;
		return (RET_ERROR);
	}
    assert(flags==0);
	status = __bt_bdelete(t, key);
	
    if (status == RET_SUCCESS)
		F_SET(t, B_MODIFIED);
	return (status);
}


/*
 * __bt_bdelete --
 *	Delete all key/data pairs matching the specified key.
 *
 * Parameters:
 *	  t:	tree
 *	key:	key to delete
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key not found.
 */
static int
__bt_bdelete( BTREE *t, const DBT *key)
{
	EPG *e;
	PAGE *h;
	int deleted, exact, redo;

	deleted = 0;

	/* Find any matching record; __bt_search pins the page. */
loop:	if ((e = __bt_search_st(t, key, &exact)) == NULL)
		return (deleted ? RET_SUCCESS : RET_ERROR);
	if (!exact) {
		Mpool_put(t->bt_mp, e->page, 0);
		return (deleted ? RET_SUCCESS : RET_SPECIAL);
	}

	/*
	 * Delete forward, then delete backward, from the found key.  If
	 * there are duplicates and we reach either side of the page, do
	 * the key search again, so that we get them all.
	 */
	redo = 0;
	h = e->page;
	do {
		if (__bt_dleaf(t, key, h, e->index)) {
			Mpool_put(t->bt_mp, h, 0);
			return (RET_ERROR);
		}
		if (F_ISSET(t, B_NODUPS)) {
			if (NEXTINDEX(h) == 0) {
				if (__bt_pdelete(t, h))
					return (RET_ERROR);
			} else
				Mpool_put(t->bt_mp, h, MPOOL_DIRTY);
			return (RET_SUCCESS);
		}
		deleted = 1;
	} while (e->index < NEXTINDEX(h) && __bt_cmp(t, key, e) == 0);

	/* Check for right-hand edge of the page. */
	if (e->index == NEXTINDEX(h))
		redo = 1;

	/* Delete from the key to the beginning of the page. */
	while (e->index-- > 0) {
		if (__bt_cmp(t, key, e) != 0)
			break;
		if (__bt_dleaf(t, key, h, e->index) == RET_ERROR) {
			Mpool_put(t->bt_mp, h, 0);
			return (RET_ERROR);
		}
		if (e->index == 0)
			redo = 1;
	}

	/* Check for an empty page. */
	if (NEXTINDEX(h) == 0) {
		if (__bt_pdelete(t, h))
			return (RET_ERROR);
		goto loop;
	}

	/* Put the page. */
	Mpool_put(t->bt_mp, h, MPOOL_DIRTY);

	if (redo)
		goto loop;
	return (RET_SUCCESS);
}

/*
 * __bt_pdelete --
 *	Delete a single page from the tree.
 *
 * Parameters:
 *	t:	tree
 *	h:	leaf page
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 *
 * Side-effects:
 *	Mpool_put's the page
 */
static int
__bt_pdelete( BTREE *t, PAGE *h)
{
	BINTERNAL *bi;
	PAGE *pg;
	EPGNO *parent;
    DBT key;
	indx_t index;
	u_int32_t nksize;

	/*
	 * Walk the parent page stack -- a LIFO stack of the pages that were
	 * traversed when we searched for the page where the delete occurred.
	 * Each stack entry is a page number and a page index offset.  The
	 * offset is for the page traversed on the search.  We've just deleted
	 * a page, so we have to delete the key from the parent page.
	 *
	 * If the delete from the parent page makes it empty, this process may
	 * continue all the way up the tree.  We stop if we reach the root page
	 * (which is never deleted, it's just not worth the effort) or if the
	 * delete does not empty the page.
	 */
	while ((parent = BT_POP(t)) != NULL) {
		/* Get the parent page. */
        pg = read_node(t,parent->pgno);
		
		index = parent->index;
		bi = GETBINTERNAL(pg, index);

        assert(!(bi->flags & P_BIGKEY));

		/*
		 * Free the parent if it has only the one key and it's not the
		 * root page. If it's the rootpage, turn it back into an empty
		 * leaf page.
		 */
		if (NEXTINDEX(pg) == 1){
			if (pg->pgno == P_ROOT) {
				pg->lower = BTDATAOFF;
				pg->upper = t->bt_psize;
				pg->flags = P_BLEAF;
			} else {
				if (__bt_relink(t, pg) || free_node(t, pg))
					return (RET_ERROR);
				continue;
			}
        }
		else {
			/* Pack remaining key items at the end of the page. */
			nksize = NBINTERNAL(bi->ksize);
            key.size = bi->ksize;
            key.data = bi->bytes;
            /* FIXME: what about the same key ? */
            if(pg->flags & P_LMEM)
                logpool_put(t,pg->nid, &key ,NULL, bi->pgno ,DELETE_KEY| P_BINTERNAL);
            shrinkroom(pg, index, nksize);
		}

		Mpool_put(t->bt_mp, pg, MPOOL_DIRTY);
		break;
	}

	/* Free the leaf page, as long as it wasn't the root. */
	if (h->pgno == P_ROOT) {
		Mpool_put(t->bt_mp, h, MPOOL_DIRTY);
		return (RET_SUCCESS);
	}
	return (__bt_relink(t, h) || free_node(t, h));
}

/*
 * __bt_dleaf --
 *	Delete a single record from a leaf page.
 *
 * Parameters:
 *	t:	tree
 *    key:	referenced key
 *	h:	page
 *	index:	index on page to delete
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 *
 * int node_delkey(BTREE* t,PAGE* h, const DBT* key, indx_t index)
 */
int
__bt_dleaf( BTREE *t, const DBT *key, PAGE *h, u_int index)
{
	BLEAF *bl;
	u_int32_t nbytes;

	/* If the entry uses overflow pages, make them available for reuse. */
    assert( !(bl->flags & P_BIGKEY) && !(bl->flags & P_BIGDATA));

	/* Pack the remaining key/data items at the end of the page. */
	nbytes = NBLEAF(bl);
    shrinkroom(h,index,nbytes);

    if(h->flags & P_LMEM){
        logpool_put(t,h->nid,key,NULL,P_INVALID,DELETE_KEY| P_BLEAF);
    }

	return (RET_SUCCESS);
}


/*
 * __bt_relink --
 *	Link around a deleted page.
 *
 * Parameters:
 *	t:	tree
 *	h:	page to be deleted
 */
static int
__bt_relink( BTREE *t, PAGE *h)
{
	PAGE *pg;

	if (h->nextpg != P_INVALID) {
		if ((pg = mpool_get(t->bt_mp, h->nextpg, 0)) == NULL)
			return (RET_ERROR);
		pg->prevpg = h->prevpg;
		Mpool_put(t->bt_mp, pg, MPOOL_DIRTY);
	}
	if (h->prevpg != P_INVALID) {
		if ((pg = mpool_get(t->bt_mp, h->prevpg, 0)) == NULL)
			return (RET_ERROR);
		pg->nextpg = h->nextpg;
		Mpool_put(t->bt_mp, pg, MPOOL_DIRTY);
	}
	return (0);
}
