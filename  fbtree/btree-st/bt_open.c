/*-
 * See Licence File
 */

/*
 * Implementation of btree access method for 4.4BSD.
 *
 * The design here was originally based on that of the btree access method
 * used in the Postgres database system at UC Berkeley.  This implementation
 * is wholly independent of the Postgres code.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btree.h"

extern int mkstemp(char * path);
#ifdef DEBUG
#undef	MINPSIZE
#define	MINPSIZE	128
#endif

static int byteorder __P((void));
static int nroot __P((BTREE *));
static int tmp __P((void));

/*
 * configuration for the device 
 */
static device_config(BTREE* t)
{
    int N;
    t->cw = 100;
    t->cr = 100;
    t->node_size = 1;
    t->log_per_page = (t->bt_psize- BTDATAOFF) / (sizeof(BLOG) -1 + 4 ) ;
    N = (t->bt_psize-BTDATAOFF) / (sizeof(BINTERNAL)-1 + 4 );
    t->C = t->cw * ( t->node_size + (0.69*N+1) / t->log_per_page) ;
}
/*
 * __BT_OPEN -- Open a btree.
 *
 * Creates and fills a DB struct, and calls the routine that actually
 * opens the btree.
 *
 * Parameters:
 *	fname:	filename (NULL for in-memory trees)
 *	flags:	open flag bits
 *	mode:	open permission bits
 *	b:	BTREEINFO pointer
 *
 * Returns:
 *	NULL on failure, pointer to DB on success.
 *
 */
DB *
__bt_open(fname, flags, mode, openinfo, dflags)
	const char *fname;
	int flags, mode, dflags;
	const BTREEINFO *openinfo;
{
    struct stat sb;
	BTMETA m;
	BTREE *t;
	BTREEINFO b;
	DB *dbp;
	pgno_t ncache;
	ssize_t nr;
	int machine_lorder;

    b.prefix=NULL;
	t = NULL;

	/*
	 * Intention is to make sure all of the user's selections are okay
	 * here and then use them without checking.  Can't be complete, since
	 * we don't know the right page size, lorder or flags until the backing
	 * file is opened.  Also, the file's page size can cause the cachesize
	 * to change.
	 */
	machine_lorder = byteorder();
	if (openinfo) {
		b = *openinfo;

		/* Flags: R_DUP. */
		if (b.flags & ~(R_DUP))
			goto einval;

		/*
		 * Page size must be indx_t aligned and >= MINPSIZE.  Default
		 * page size is set farther on, based on the underlying file
		 * transfer size.
		 */
		if (b.psize &&
		    (b.psize < MINPSIZE || b.psize > MAX_PAGE_OFFSET + 1 ||
		    b.psize & sizeof(indx_t) - 1))
			goto einval;

		/* Minimum number of keys per page; absolute minimum is 2. */
		if (b.minkeypage) {
			if (b.minkeypage < 2)
				goto einval;
		} else
			b.minkeypage = DEFMINKEYPAGE;

		/* If no comparison, use default comparison and prefix. */
		if (b.compare == NULL) {
			b.compare = __bt_defcmp;
		}

		if (b.lorder == 0)
			b.lorder = machine_lorder;
	} else {
		b.compare = __bt_defcmp;
		b.cachesize = 0;
		b.flags = 0;
		b.lorder = machine_lorder;
		b.minkeypage = DEFMINKEYPAGE;
		b.psize = 0;
	}

	/* Check for the ubiquitous PDP-11. */
	if (b.lorder != BIG_ENDIAN && b.lorder != LITTLE_ENDIAN)
		goto einval;

	/* Allocate and initialize DB and BTREE structures. */
	if ((t = (BTREE *)malloc(sizeof(BTREE))) == NULL)
		goto err;
	memset(t, 0, sizeof(BTREE));
	t->bt_fd = -1;			/* Don't close unopened fd on error. */
	t->bt_lorder = b.lorder;
	t->bt_order = NOT;
	t->bt_cmp = b.compare;
    /* It seems OK to not use prefix function 
     *      t->bt_pfx = NULL;
     */
	t->bt_pfx = b.prefix;

	if ((t->bt_dbp = dbp = (DB *)malloc(sizeof(DB))) == NULL)
		goto err;
	memset(t->bt_dbp, 0, sizeof(DB));
	if (t->bt_lorder != machine_lorder)
		F_SET(t, B_NEEDSWAP);

	dbp->type = DB_BTREE;
	dbp->internal = t;
	dbp->close = __bt_close;
    /* XXX del,fd,seq,syn not implenmented yet */
#if 0
	dbp->del = __bt_delete;
	dbp->fd = __bt_fd;
	dbp->seq = __bt_seq;
	dbp->sync = __bt_sync;
#endif
    dbp->get = __bt_get_st;
	dbp->put = __bt_put_st;

	/*
	 * If no file name was supplied, this is an in-memory btree and we
	 * open a backing temporary file.  Otherwise, it's a disk-based tree.
	 */
	if (fname) {
		switch (flags & O_ACCMODE) {
		case O_RDONLY:
			F_SET(t, B_RDONLY);
			break;
		case O_RDWR:
			break;
		case O_WRONLY:
		default:
			goto einval;
		}
		
		if ((t->bt_fd = open(fname, flags, mode)) < 0)
			goto err;

	} else {
		if ((flags & O_ACCMODE) != O_RDWR)
			goto einval;
		if ((t->bt_fd = tmp()) == -1)
			goto err;
		F_SET(t, B_INMEM);
	}
	
    #if !defined(_WIN32)
	if (fcntl(t->bt_fd, F_SETFD, 1) == -1)
		goto err;
	#endif
	if (fstat(t->bt_fd, &sb))
		goto err;
	if (sb.st_size) {
		if ((nr = read(t->bt_fd, &m, sizeof(BTMETA))) < 0)
			goto err;
		if (nr != sizeof(BTMETA))
			goto eftype;

		/*
		 * Read in the meta-data.  This can change the notion of what
		 * the lorder, page size and flags are, and, when the page size
		 * changes, the cachesize value can change too.  If the user
		 * specified the wrong byte order for an existing database, we
		 * don't bother to return an error, we just clear the NEEDSWAP
		 * bit.
		 */
		if (m.magic == BTREEMAGIC)
			F_CLR(t, B_NEEDSWAP);
		else {
			F_SET(t, B_NEEDSWAP);
			M_32_SWAP(m.magic);
			M_32_SWAP(m.version);
			M_32_SWAP(m.psize);
			M_32_SWAP(m.free);
			M_32_SWAP(m.flags);
		}
		if (m.magic != BTREEMAGIC || m.version != BTREEVERSION)
			goto eftype;
		if (m.psize < MINPSIZE || m.psize > MAX_PAGE_OFFSET + 1 ||
		    m.psize & sizeof(indx_t) - 1)
			goto eftype;
		if (m.flags & ~SAVEMETA)
			goto eftype;
		b.psize = m.psize;
		F_SET(t, m.flags);
		t->bt_free = m.free;
	} else {
		/*
		 * Set the page size to the best value for I/O to this file.
		 * Don't overflow the page offset type.
		 */
		
		if (b.psize == 0) {
		#ifdef HAVE_ST_BLKSIZE
			b.psize = sb.st_blksize;
		#else
			b.psize = DB_DEF_IOSIZE;
		#endif
			if (b.psize < MINPSIZE)
				b.psize = MINPSIZE;
			if (b.psize > MAX_PAGE_OFFSET + 1)
				b.psize = MAX_PAGE_OFFSET + 1;
		}

		/* Set flag if duplicates permitted. */
		if (!(b.flags & R_DUP))
			F_SET(t, B_NODUPS);

		t->bt_free = P_INVALID;
		F_SET(t, B_METADIRTY);
	}

	t->bt_psize = b.psize;

	/* Set the cache size; must be a multiple of the page size. */
	if (b.cachesize && b.cachesize & b.psize - 1)
		b.cachesize += (~b.cachesize & b.psize - 1) + 1;
	if (b.cachesize < b.psize * MINCACHE)
		b.cachesize = b.psize * MINCACHE;

	/* Calculate number of pages to cache. */
	ncache = (b.cachesize + t->bt_psize - 1) / t->bt_psize;

	/*
	 * The btree data structure requires that at least two keys can fit on
	 * a page, but other than that there's no fixed requirement.  The user
	 * specified a minimum number per page, and we translated that into the
	 * number of bytes a key/data pair can use before being placed on an
	 * overflow page.  This calculation includes the page header, the size
	 * of the index referencing the leaf item and the size of the leaf item
	 * structure.  Also, don't let the user specify a minkeypage such that
	 * a key/data pair won't fit even if both key and data are on overflow
	 * pages.
	 */
	t->bt_ovflsize = (t->bt_psize - BTDATAOFF) / b.minkeypage -
	    (sizeof(indx_t) + NBLEAFDBT(0, 0));
	if (t->bt_ovflsize < NBLEAFDBT(NOVFLSIZE, NOVFLSIZE) + sizeof(indx_t))
		t->bt_ovflsize =
		    NBLEAFDBT(NOVFLSIZE, NOVFLSIZE) + sizeof(indx_t);

	/* Initialize the buffer pool. */
	if ((t->bt_mp =
	    mpool_open(NULL, t->bt_fd, t->bt_psize, ncache)) == NULL)
		goto err;
	if (!F_ISSET(t, B_INMEM))
		mpool_filter(t->bt_mp, __bt_pgin, __bt_pgout, t);
    
    device_config(t);
    NTT_init(); 
    /* Create a root page if new tree. */
	if (nroot(t) == RET_ERROR)
		goto err;

	/* Global flags. */
	if (dflags & DB_LOCK)
		F_SET(t, B_DB_LOCK);
	if (dflags & DB_SHMEM)
		F_SET(t, B_DB_SHMEM);
	if (dflags & DB_TXN)
		F_SET(t, B_DB_TXN);

    logpool_init(t);
    err_debug1("ncache = %d", ncache); 
	return (dbp);

einval:	errno = EINVAL;
	goto err;

eftype:	errno = EFTYPE;
	goto err;

err:	if (t) {
		if (t->bt_dbp)
			free(t->bt_dbp);
		if (t->bt_fd != -1)
			(void)close(t->bt_fd);
		free(t);
	}
	return (NULL);
}

/*
 * NROOT -- Create the root of a new tree.
 *
 * Parameters:
 *	t:	tree
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
static int
nroot(t)
	BTREE *t;
{
	PAGE *meta, *root;
	pgno_t npg;
	if ((meta = mpool_get(t->bt_mp, 0, 0)) != NULL) {
		Mpool_put(t->bt_mp, meta, 0);
		return (RET_SUCCESS);
	}

	if (errno != EINVAL)		/* It's OK to not exist. */
		return (RET_ERROR);
	errno = 0;
	if ((meta = mpool_new(t->bt_mp, &npg)) == NULL)
		return (RET_ERROR);

	if ((root = new_node(t, &npg, P_BLEAF)) == NULL)
		return (RET_ERROR);

	if (root->nid != P_ROOT){
        err_ret("error: root.nid = %u", root->nid);
		return (RET_ERROR);
    }
	memset(meta, 0, t->bt_psize);

	Mpool_put(t->bt_mp, meta, MPOOL_DIRTY);
	Mpool_put(t->bt_mp, root, MPOOL_DIRTY);
	return (RET_SUCCESS);
}

static int
tmp()
{
	#if !defined(_WIN32)
	sigset_t set, oset;
	#endif
	int fd;
	char *envtmp;
	char path[MAXPATHLEN];

	envtmp = getenv("TMPDIR");
	(void)snprintf(path,
	    sizeof(path), "%s/bt.XXXXXX", envtmp ? envtmp : "/tmp");
    #if !defined(_WIN32)
	  (void)sigfillset(&set);
	  (void)sigprocmask(SIG_BLOCK, &set, &oset);
    #endif
	if ((fd = mkstemp(path)) != -1)
		(void)unlink(path);

    #if !defined(_WIN32)
	  (void)sigprocmask(SIG_SETMASK, &oset, NULL);
    #endif
	return(fd);
}

static int
byteorder()
{
	u_int32_t x;
	u_char *p;

	x = 0x01020304;
	p = (u_char *)&x;
	switch (*p) {
	case 1:
		return (BIG_ENDIAN);
	case 4:
		return (LITTLE_ENDIAN);
	default:
		return (0);
	}
}

int
__bt_fd(dbp)
        const DB *dbp;
{
	BTREE *t;

	t = dbp->internal;

    bt_tosspinned(t);

	/* In-memory database can't have a file descriptor. */
	if (F_ISSET(t, B_INMEM)) {
		errno = ENOENT;
		return (-1);
	}
	return (t->bt_fd);
}
