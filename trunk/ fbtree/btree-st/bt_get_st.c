#include <sys/types.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>


#include "btree.h"

/*
 * __BT_GET -- Get a record from the btree.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key to find
 *	data:	data to return
 *	flag:	currently unused
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key not found.
 */
int
__bt_get_st(const DB* dbp, const DBT *key, DBT *data, u_int flags)
{
    
    BTREE *t;
	EPG *e;
	int exact, status;

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

	/* Get currently doesn't take any flags. */
	if (flags) {
		errno = EINVAL;
		return (RET_ERROR);
	}

	if ((e = __bt_search_st(t, key, &exact)) == NULL)
		return (RET_ERROR);
	if (!exact) {
		mpool_put(t->bt_mp, e->page, 0);
		return (RET_SPECIAL);
	}

	status = __bt_ret(t, e, NULL, NULL, data, &t->bt_rdata, 0);


	/*
	 * If the user is doing concurrent access, we copied the
	 * key/data, toss the page.
	 */
	if (F_ISSET(t, B_DB_LOCK))
		mpool_put(t->bt_mp, e->page, 0);
	else
		t->bt_pinned = e->page;
	return (status);
}