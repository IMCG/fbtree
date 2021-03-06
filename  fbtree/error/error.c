#include	<errno.h>		/* for definition of errno */
#include	<stdarg.h>		/* ANSI C header file */
#include	"config.h"
#include	"errhandle.h"

static void	err_doit(int, const char *, va_list);
static void	err_doit0(int, const char *, va_list);

char	*pname = NULL;		/* caller can set this from argv[0] */

/* err_debug - Output the message to stderr for debug
 * Print a message and return
 */
void
err_debug1(const char *fmt, ...)
{
#ifdef TEST_DEBUG
	va_list		ap;

	va_start(ap, fmt);
	err_doit(0, fmt, ap);
	va_end(ap);
#endif
	return;
}
/* err_debug0 - err_debug without a new line  
 */
void
err_debug0(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit0(0, fmt, ap);
	va_end(ap);
	return;
}

/* Nonfatal error related to a system call.
 * Print a message and return. */

void
err_ret(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, fmt, ap);
	va_end(ap);
	return;
}

/* Fatal error related to a system call.
 * Print a message and terminate. */

void
err_sys(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, fmt, ap);
	va_end(ap);
	exit(1);
}

/* Fatal error related to a system call.
 * Print a message, dump core, and terminate. */

void
err_dump(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, fmt, ap);
	va_end(ap);
	abort();		/* dump core and terminate */
	exit(1);		/* shouldn't get here */
}

/* Nonfatal error unrelated to a system call.
 * Print a message and return. */

void
err_msg(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(0, fmt, ap);
	va_end(ap);
	return;
}

/* Fatal error unrelated to a system call.
 * Print a message and terminate. */

void
err_quit(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(0, fmt, ap);
	va_end(ap);
	exit(1);
}

/* Print a message and return to caller.
 * Caller specifies "errnoflag". */

static void
err_doit(int errnoflag, const char *fmt, va_list ap)
{
	int		errno_save;
	char	buf[MAXLINE];

	errno_save = errno;		/* value caller might want printed */
	vsprintf(buf, fmt, ap);
	if (errnoflag)
		sprintf(buf+strlen(buf), ": %s", strerror(errno_save));
	strcat(buf, "\n");
	fflush(stdout);		/* in case stdout and stderr are the same */
	fputs(buf, stderr);
	fflush(NULL);		/* flushes all stdio output streams */
	return;
}
/* err_doit0 - err_doit without a new line
 */
static void
err_doit0(int errnoflag, const char *fmt, va_list ap)
{
	int		errno_save;
	char	buf[MAXLINE];

	errno_save = errno;		/* value caller might want printed */
	vsprintf(buf, fmt, ap);
	if (errnoflag)
		sprintf(buf+strlen(buf), ": %s", strerror(errno_save));
	fflush(stdout);		/* in case stdout and stderr are the same */
	fputs(buf, stderr);
	fflush(NULL);		/* flushes all stdio output streams */
	return;
}
