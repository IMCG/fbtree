/* Our own header, to be included *after* all standard system headers */

#ifndef	__ourhdr_h
#define	__ourhdr_h
#include    "config.h"
#include	<sys/types.h>	/* required for some of our prototypes */
#include	<stdio.h>		/* for convenience */
#include	<stdlib.h>		/* for convenience */
#include	<string.h>		/* for convenience */
#include	<unistd.h>		/* for convenience */
#include	<string.h>

#define	MAXLINE	4096			/* max line length */

#define	FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
					/* default file access permissions for new files */
#define	DIR_MODE	(FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)
					/* default permissions for new directories */

typedef	void	Sigfunc(int);	/* for signal handlers */

					/* 4.3BSD Reno <signal.h> doesn't define SIG_ERR */
#if	defined(SIG_IGN) && !defined(SIG_ERR)
#define	SIG_ERR	((Sigfunc *)-1)
#endif

#define	min(a,b)	((a) < (b) ? (a) : (b))
#define	max(a,b)	((a) > (b) ? (a) : (b))

#ifdef TEST_DEBUG
#define	do_we_log_this(args) (0)
#define err_debug(args) do{ \
    if(do_we_log_this(__FILE__)){ \
        const char* name = strrchr(__FILE__,'/'); \
        err_debug0("%s:%d:\t", name ? name+1 : __FILE__, __LINE__); \
        err_debug1 args ; \
    }}while(0);
#else
#define err_debug(args) ((void)(0))
#endif

void	err_debug0(const char *, ...);
void	err_debug1(const char *, ...);
void	err_dump(const char *, ...);	/* {App misc_source} */
void	err_msg(const char *, ...);
void	err_quit(const char *, ...);
void	err_ret(const char *, ...);
void	err_sys(const char *, ...);

void	log_msg(const char *, ...);		/* {App misc_source} */
void	log_open(const char *, int, int);
void	log_quit(const char *, ...);
void	log_ret(const char *, ...);
void	log_sys(const char *, ...);
#endif	/* __ourhdr_h */
