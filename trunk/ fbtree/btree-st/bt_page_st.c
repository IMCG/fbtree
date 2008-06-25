/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)bt_page.c	8.3 (Berkeley) 7/14/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <stdio.h>


#include "btree.h"

/*
 * __bt_free --
 *	Put a page on the freelist.
 *
 * Parameters:
 *	t:	tree
 *	h:	page to free
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 *
 * Side-effect:
 *	Mpool_put's the page.
 */
int
__bt_free(t, h)
	BTREE *t;
	PAGE *h;
{
	/* Insert the page at the head of the free list. */
	h->prevpg = P_INVALID;
	h->nextpg = t->bt_free;
	t->bt_free = h->pgno;

#ifdef MPOOL_DEBUG
        err_debug(("free page %ud into the freelist",h->pgno));
#endif
	/* Make sure the page gets written back. */
	return (mpool_put(t->bt_mp, h, MPOOL_DIRTY));
}

/*
 * __bt_new --
 *	Get a new page, preferably from the freelist.
 *
 * Parameters:
 *	t:	tree
 *	npg:	storage for page number.
 *
 * Returns:
 *	Pointer to a page, NULL on error.
 * 
 */
PAGE *
__bt_new( BTREE *t, pgno_t *npg)
{
	PAGE *h;
#if 0
	if (t->bt_free != P_INVALID &&
	    (h = mpool_get(t->bt_mp, t->bt_free, 0)) != NULL) {
		*npg = t->bt_free;
		t->bt_free = h->nextpg;
#ifdef MPOOL_DEBUG
        err_debug(("new page %ud from the freelist",*npg));
#endif
	}
    else{
	    h= mpool_new(t->bt_mp, npg);
    }
#endif
	    
    h= mpool_new(t->bt_mp, npg);
    /* @mx 
     * In the original version, the function don't initialize h.
     * IMHO, it's not a good design, since some initial value can be default
     * Here we set it. It won't affect other code either since they'll reset it
     */
    assert(h!=NULL);
    h->nid = P_INVALID;
    h->pgno = *npg;
    h->prevpg = h->nextpg = P_INVALID;
	h->lower = BTDATAOFF;
	h->upper = t->bt_psize;
    err_debug(("new page %u:__bt_new",h->pgno)); 
	return (h);
}
