
#include <sys/types.h>

#include <stdio.h>


#include "btree.h"

static int __bt_snext __P((BTREE *, PAGE *, const DBT *, int *));
static int __bt_sprev __P((BTREE *, PAGE *, const DBT *, int *));

/*
 * __bt_search --
 *	Search a btree for a key.
 *
 * Parameters:
 *	t:	tree to search
 *	key:	key to find
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	The EPG for matching record, if any, or the EPG for the location
 *	of the key, if it were inserted into the tree, is entered into
 *	the bt_cur field of the tree.  A pointer to the field is returned.
 */
EPG *
__bt_search_st(BTREE *t,const DBT *key,int *exactp)
{

    /*
     * 1.Read from root of the btree in NTT
     * 2.reconstruct the node
     */
    // read node from ROOT
    // loop until we find the right node

    PAGE *h;
	indx_t base, index, lim;
	pgno_t pg;
	int cmp;

	BT_CLR(t);  /* @mx it initializes t->bt_sp  */
    err_debug1("Searh Btree");
	for (pg = P_ROOT;;) {
        err_debug(("~^"));
        err_debug(("Read Node %ud",pg));
        h = read_node(t,pg);
        //__bt_dpage(h);
        err_debug(("~$End Read"));
        if(h==NULL)
			return (NULL);
        /* ??? not so clear about the binary search */
		/* Do a binary search on the current page. */
		t->bt_cur.page = h;
		for (base = 0, lim = NEXTINDEX(h); lim; lim >>= 1) {
			t->bt_cur.index = index = base + (lim >> 1);
			if ((cmp = __bt_cmp(t, key, &t->bt_cur)) == 0) {
				if (h->flags & P_BLEAF) {
					*exactp = 1;
                    err_debug1("End Search\n");
					return (&t->bt_cur);
				}
				goto next;
			}
			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		/*
		 * If it's a leaf page, we're almost done.  If no duplicates
		 * are allowed, or we have an exact match, we're done.  Else,
		 * it's possible that there were matching keys on this page,
		 * which later deleted, and we're on a page with no matches
		 * while there are matches on other pages.  If at the start or
		 * end of a page, check the adjacent page.
		 */
		if (h->flags & P_BLEAF) {
			if (!F_ISSET(t, B_NODUPS)) {
				if (base == 0 &&
				    h->prevpg != P_INVALID &&
				    __bt_sprev(t, h, key, exactp))
                    err_debug1("End Search\n");
					return (&t->bt_cur);
				if (base == NEXTINDEX(h) &&
				    h->nextpg != P_INVALID &&
				    __bt_snext(t, h, key, exactp))
                    err_debug1("End Search\n");
					return (&t->bt_cur);
			}
			*exactp = 0;
			t->bt_cur.index = base;
            err_debug1("End Search\n");
			return (&t->bt_cur);
		}

		/*
		 * No match found.  Base is the smallest index greater than
		 * key and may be zero or a last + 1 index.  If it's non-zero,
		 * decrement by one, and record the internal page which should
		 * be a parent page for the key.  If a split later occurs, the
		 * inserted page will be to the right of the saved page.
		 */
		index = base ? base - 1 : base;

next:		BT_PUSH(t, h->pgno, index);
        pg = GETBINTERNAL(h, index)->pgno;
		mpool_put(t->bt_mp, h, 0);
	}
}

/*
 * __bt_snext --
 *	Check for an exact match after the key.
 *
 * Parameters:
 *	t:	tree
 *	h:	current page
 *	key:	key
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	If an exact match found.
 */
static int
__bt_snext(t, h, key, exactp)
	BTREE *t;
	PAGE *h;
	const DBT *key;
	int *exactp;
{
	EPG e;

	/*
	 * Get the next page.  The key is either an exact
	 * match, or not as good as the one we already have.
	 */
	if ((e.page = mpool_get(t->bt_mp, h->nextpg, 0)) == NULL)
		return (0);
	e.index = 0;
	if (__bt_cmp(t, key, &e) == 0) {
		mpool_put(t->bt_mp, h, 0);
		t->bt_cur = e;
		*exactp = 1;
		return (1);
	}
	mpool_put(t->bt_mp, e.page, 0);
	return (0);
}

/*
 * __bt_sprev --
 *	Check for an exact match before the key.
 *
 * Parameters:
 *	t:	tree
 *	h:	current page
 *	key:	key
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	If an exact match found.
 */
static int
__bt_sprev(t, h, key, exactp)
	BTREE *t;
	PAGE *h;
	const DBT *key;
	int *exactp;
{
	EPG e;

	/*
	 * Get the previous page.  The key is either an exact
	 * match, or not as good as the one we already have.
	 */
	if ((e.page = mpool_get(t->bt_mp, h->prevpg, 0)) == NULL)
		return (0);
	e.index = NEXTINDEX(e.page) - 1;
	if (__bt_cmp(t, key, &e) == 0) {
		mpool_put(t->bt_mp, h, 0);
		t->bt_cur = e;
		*exactp = 1;
		return (1);
	}
	mpool_put(t->bt_mp, e.page, 0);
	return (0);
}
