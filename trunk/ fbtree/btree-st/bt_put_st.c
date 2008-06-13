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
	EPG *e;
	PAGE *h;
	indx_t index;
	u_int32_t nbytes;
	int dflags, exact, status;
    u_int32_t mode; 
	char *dest;
    
    dflags = 0 ; //NO BIGKEY or BIGDATA
	t = dbp->internal;
    mode = t->bt_mode;

    bt_tosspinned(t);

	/* Check for change to a read-only tree. */
	if (F_ISSET(t, B_RDONLY)) {
		errno = EPERM;
        err_ret("it's a read only tree: %s",err_loc);
		return (RET_ERROR);
	}

	assert (!(key->size + data->size > t->bt_ovflsize));
	assert (!(flags == R_CURSOR));

	/* ----
     * = Step 1. find Key(index) =< key < Key(index+1) =
     * ----
	 * Find the key to delete, or, the location at which to insert.
	 * Bt_fast and __bt_search both pin the returned page.
     *
	 */
	if (t->bt_order == NOT || (e = bt_fast(t, key, data, &exact)) == NULL)
		if ((e = __bt_search_st(t, key, &exact)) == NULL){
            err_ret("error of __bt_search-st: %s",err_loc);
			return (RET_ERROR);
        }
	h = e->page;
	index = e->index;
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
		Mpool_put(t->bt_mp, h, 0);
		return (RET_SPECIAL);
	default:
		if (!exact || !F_ISSET(t, B_NODUPS))
			break;
	}

	/* == Case 1. split if not enough room == 
	 * If not enough room, or the user has put a ceiling on the number of
	 * keys permitted in the page, split the page.  The split code will
	 * insert the key and data and unpin the current page.  If inserting
	 * into the offset array, shift the pointers up.
	 */
	nbytes = NBLEAFDBT(key->size, data->size);
	if (h->upper - h->lower < nbytes + sizeof(indx_t)) {
        err_debug(("leaf room is not enough, split"));
		if ((status = __bt_split_st(t, h, key,
		    data, dflags, nbytes, index)) != RET_SUCCESS){
			return (status);
        }

		goto success;
	}
    /* == Case 2. directly insert if enough room == 
     * FIXME leaf is always in disk mode here
     */
    err_debug(("leaf room is enough, insert"));
    if( h->flags & P_DISK){
        dest = makeroom(h,index,nbytes);
        WR_BLEAF(dest, key, data, dflags);
    }
    else{
        NTTEntry* entry;
        BLOG* bl_log;
        assert( (mode & P_LOG) && (h->flags & (P_MEM|P_BLEAF )) );
        entry =  NTT_get(h->pgno);
        bl_log = disk2log_bl_dbt(key, data, h->pgno, entry->maxSeq++, entry->logVersion);
        logpool_put(t,bl_log);
    }

    // if the insert position is the leftmost/rightmost, set them   
	if (t->bt_order == NOT){
		if (h->nextpg == P_INVALID){
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
    }

	Mpool_put(t->bt_mp, h, MPOOL_DIRTY);

success:
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
 *
 * @ only check the leftmost leaf and rightmost leaf
 *
 * TODO:XXX: disable the function for test 
 */
static EPG *
bt_fast( BTREE *t, const DBT *key, const DBT *data, int *exactp )
{
#if 0
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
	Mpool_put(t->bt_mp, h, 0);
#endif
	return (NULL);
}
