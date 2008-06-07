#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "btree.h"

static EPG *bt_fast __P((BTREE *, const DBT *, const DBT *, int *));

/*
 * __BT_PUT_ST -- Add a btree item to the tree.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key
 *	data:	data
 *	flag:	R_NOOVERWRITE
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key is already in the
 *	tree and R_NOOVERWRITE specified.
 */
int
__bt_put_st(const DB *dbp,DBT *key,	const DBT *data, u_int flags)
{
    const char* err_loc = "(__bt_put_st) in 'bt_put_st.c'";
	BTREE *t;
	DBT tkey, tdata;
	EPG *e;
	PAGE *h;
	indx_t index, nxtindex;
	pgno_t pg;
	u_int32_t nbytes;
	int dflags, exact, status;
	char *dest, db[NOVFLSIZE], kb[NOVFLSIZE];

	t = dbp->internal;

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
#ifdef MPOOL_DEBUG
        err_debug(("Toss any page pinned across calls")); 
#endif  
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
#ifdef MPOOL_DEBUG
        err_debug(("End Toss\n")); 
#endif  
	}

	/* Check for change to a read-only tree. */
	if (F_ISSET(t, B_RDONLY)) {
		errno = EPERM;
        err_ret("it's a read only tree: %s",err_loc);
		return (RET_ERROR);
	}

	assert (!(key->size + data->size > t->bt_ovflsize));
	assert (!(flags == R_CURSOR));
    /* XXX deal with big data/key and R_CURSOR */
#if 0
//{{{
	switch (flags) {
	case 0:
	case R_NOOVERWRITE:
		break;
	case R_CURSOR:
		/*
		 * If flags is R_CURSOR, put the cursor.  Must already
		 * have started a scan and not have already deleted it.
		 */
		if (F_ISSET(&t->bt_cursor, CURS_INIT) &&
		    !F_ISSET(&t->bt_cursor,
		        CURS_ACQUIRE | CURS_AFTER | CURS_BEFORE))
			break;
		/* FALLTHROUGH */
	default:
		errno = EINVAL;
		return (RET_ERROR);
	}


	/*
	 * If the key/data pair won't fit on a page, store it on overflow
	 * pages.  Only put the key on the overflow page if the pair are
	 * still too big after moving the data to an overflow page.
	 *
	 * XXX
	 * If the insert fails later on, the overflow pages aren't recovered.
	 */
	dflags = 0; /* @mx data flag to indicate BIGKEY or BIGDATA */
	if (key->size + data->size > t->bt_ovflsize) {
		if (key->size > t->bt_ovflsize) {
storekey:		if (__ovfl_put(t, key, &pg) == RET_ERROR)
				return (RET_ERROR);
tkey.data = kb;
			tkey.size = NOVFLSIZE;
			memmove(kb, &pg, sizeof(pgno_t));
			memmove(kb + sizeof(pgno_t),
			    &key->size, sizeof(u_int32_t));
			dflags |= P_BIGKEY;
			key = &tkey;
		}
		if (key->size + data->size > t->bt_ovflsize) {
			if (__ovfl_put(t, data, &pg) == RET_ERROR)
				return (RET_ERROR);
			tdata.data = db;
			tdata.size = NOVFLSIZE;
			memmove(db, &pg, sizeof(pgno_t));
			memmove(db + sizeof(pgno_t),
			    &data->size, sizeof(u_int32_t));
			dflags |= P_BIGDATA;
			data = &tdata;
		}
		if (key->size + data->size > t->bt_ovflsize)
			goto storekey;
	}

	/* Replace the cursor. */
	if (flags == R_CURSOR) {
		if ((h = mpool_get(t->bt_mp, t->bt_cursor.pg.pgno, 0)) == NULL)
			return (RET_ERROR);
		index = t->bt_cursor.pg.index;
		goto delete;
	}
//}}}
#endif

	/* ----
     * = Step 1. find Key(index) =< key < Key(index+1) =
     * ----
	 * Find the key to delete, or, the location at which to insert.
	 * Bt_fast and __bt_search both pin the returned page.
     *
     * ??? bt_fast {FORWARD,BACK}
	 */
	if (t->bt_order == NOT || (e = bt_fast(t, key, data, &exact)) == NULL)
		if ((e = __bt_search_st(t, key, &exact)) == NULL){
            err_ret("error of __bt_search-st: %s",err_loc);
			return (RET_ERROR);
        }
	h = e->page;
	index = e->index;
#ifdef BT_PUT_DEBUG
    err_debug(("find index s.t. K(index) =< key < K(index+1) "));
#endif
	/* ----
     * = Step 2. Insert key/data into the tree =
     * ----
	 * Add the key/data pair to the tree.  If an identical key is already
	 * in the tree, and R_NOOVERWRITE is set, an error is returned.  If
	 * R_NOOVERWRITE is not set, the key is either added (if duplicates are
	 * permitted) or an error is returned.
	 */
	switch (flags) {
	case R_NOOVERWRITE:
		if (!exact)
			break;
		mpool_put(t->bt_mp, h, 0);
		return (RET_SPECIAL);
	default:
		if (!exact || !F_ISSET(t, B_NODUPS))
			break;
#if 0
//{{{
		/*
		 * !!!
		 * Note, the delete may empty the page, so we need to put a
		 * new entry into the page immediately.
		 */
delete:		if (__bt_dleaf(t, key, h, index) == RET_ERROR) {
			mpool_put(t->bt_mp, h, 0);
			return (RET_ERROR);
		}
		break;
//}}}
#endif
	}

	/* == Case 1. split if not enough room == 
	 * If not enough room, or the user has put a ceiling on the number of
	 * keys permitted in the page, split the page.  The split code will
	 * insert the key and data and unpin the current page.  If inserting
	 * into the offset array, shift the pointers up.
	 */
	nbytes = NBLEAFDBT(key->size, data->size);
	if (h->upper - h->lower < nbytes + sizeof(indx_t)) {
#ifdef BT_PUT_DEBUG
    err_debug(("leaf room is not enough, split"));
#endif
		if ((status = __bt_split_st(t, h, key,
		    data, dflags, nbytes, index)) != RET_SUCCESS){
			return (status);
        }

		goto success;
	}
    /* == Case 2. directly insert if enough room == 
     * FIXME leaf is always in disk mode here
     */
#ifdef BT_PUT_DEBUG
    err_debug(("leaf room is enough, insert"));
#endif
	if (index < (nxtindex = NEXTINDEX(h)))
		memmove(h->linp + index + 1, h->linp + index,
		    (nxtindex - index) * sizeof(indx_t));
	h->lower += sizeof(indx_t);

	h->linp[index] = h->upper -= nbytes;
	dest = (char *)h + h->upper;
	WR_BLEAF(dest, key, data, dflags);

#if 0
//{{{
    /* @mx XXX cursor is not used currently */
	/* If the cursor is on this page, adjust it as necessary. */
	if (F_ISSET(&t->bt_cursor, CURS_INIT) &&
	    !F_ISSET(&t->bt_cursor, CURS_ACQUIRE) &&
	    t->bt_cursor.pg.pgno == h->pgno && t->bt_cursor.pg.index >= index)
		++t->bt_cursor.pg.index;
//}}}
#endif
    
    // ??? not so clear about it
	if (t->bt_order == NOT)
		if (h->nextpg == P_INVALID) {
			if (index == NEXTINDEX(h) - 1) {
				t->bt_order = FORWARD;
				t->bt_last.index = index;
				t->bt_last.pgno = h->pgno;
			}
		} else if (h->prevpg == P_INVALID) {
			if (index == 0) {
				t->bt_order = BACK;
				t->bt_last.index = 0;
				t->bt_last.pgno = h->pgno;
			}
		}

	mpool_put(t->bt_mp, h, MPOOL_DIRTY);

success:
#if 0
	if (flags == R_SETCURSOR)
		__bt_setcur(t, e->page->pgno, e->index);
#endif

	F_SET(t, B_MODIFIED);
	return (RET_SUCCESS);
}

#ifdef STATISTICS
u_long bt_cache_hit, bt_cache_miss;
#endif

/*
 * BT_FAST -- Do a quick check for sorted data.
 *
 * Parameters:
 *	t:	tree
 *	key:	key to insert
 *
 * Returns:
 * 	EPG for new record or NULL if not found.
 */
static EPG *
bt_fast(t, key, data, exactp)
	BTREE *t;
	const DBT *key, *data;
	int *exactp;
{
    const char* err_loc = "(bt_fast) in 'bt_put_st.c'";
	PAGE *h;
	u_int32_t nbytes;
	int cmp;

    // @mx bt_last : last insert's pgno
	if ((h = mpool_get(t->bt_mp, t->bt_last.pgno, 0)) == NULL) {
		t->bt_order = NOT;
		return (NULL);
	}
	t->bt_cur.page = h;
	t->bt_cur.index = t->bt_last.index;

	/*
	 * If won't fit in this page or have too many keys in this page,
	 * have to search to get split stack.
	 */
	nbytes = NBLEAFDBT(key->size, data->size);
	if (h->upper - h->lower < nbytes + sizeof(indx_t))
		goto miss;

	if (t->bt_order == FORWARD) {
		if (t->bt_cur.page->nextpg != P_INVALID)
			goto miss;
		if (t->bt_cur.index != NEXTINDEX(h) - 1)
			goto miss;
		if ((cmp = __bt_cmp(t, key, &t->bt_cur)) < 0)
			goto miss;
		t->bt_last.index = cmp ? ++t->bt_cur.index : t->bt_cur.index;
	} else {
		if (t->bt_cur.page->prevpg != P_INVALID)
			goto miss;
		if (t->bt_cur.index != 0)
			goto miss;
		if ((cmp = __bt_cmp(t, key, &t->bt_cur)) > 0)
			goto miss;
		t->bt_last.index = 0;
	}
	*exactp = cmp == 0;
#ifdef STATISTICS
	++bt_cache_hit;
#endif
    err_debug(("HIT cache: %s", err_loc));
	return (&t->bt_cur);

miss:
#ifdef STATISTICS
	++bt_cache_miss;
#endif
	t->bt_order = NOT;
	mpool_put(t->bt_mp, h, 0);
	return (NULL);
}
