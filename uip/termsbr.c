
/*
 * termsbr.c -- termcap support
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_TERMIO_H
#  include <termio.h>
# else
#  include <sgtty.h>
# endif
#endif

#ifdef HAVE_TERMCAP_H
# include <termcap.h>
#endif

/* <sys/ioctl.h> is need anyway for ioctl()
#ifdef GWINSZ_IN_SYS_IOCTL
*/
# include <sys/ioctl.h>
/*
#endif
*/

#ifdef WINSIZE_IN_PTEM
# include <sys/stream.h>
# include <sys/ptem.h>
#endif

#if BUFSIZ<2048
# define TXTSIZ	2048
#else
# define TXTSIZ BUFSIZ
#endif

/*
 * These variables are sometimes defined in,
 * and needed by the termcap library.
 */
#ifdef HAVE_OSPEED
# ifdef MUST_DEFINE_OSPEED
extern short ospeed;
extern char PC;
# endif
#else
short ospeed;
char PC;
#endif

static long speedcode;

static int initLI = 0;
static int initCO = 0;

static int HC = 0;       /* are we on a hardcopy terminal?        */
static int LI = 40;      /* number of lines                       */
static int CO = 80;      /* number of colums                      */
static char *CL = NULL;  /* termcap string to clear screen        */
static char *SE = NULL;  /* termcap string to end standout mode   */
static char *SO = NULL;  /* termcap string to begin standout mode */

static char termcap[TXTSIZ];


static void
read_termcap(void)
{
    char *bp, *cp;
    char *term;

#ifndef TGETENT_ACCEPTS_NULL
    char termbuf[TXTSIZ];
#endif

#ifdef HAVE_TERMIOS_H
    struct termios tio;
#else
# ifdef HAVE_TERMIO_H
    struct termio tio;
# else
    struct sgttyb tio;
# endif
#endif

    static int inited = 0;

    if (inited++)
	return;

    if (!(term = getenv ("TERM")))
	return;

/*
 * If possible, we let tgetent allocate its own termcap buffer
 */
#ifdef TGETENT_ACCEPTS_NULL
    if (tgetent (NULL, term) != TGETENT_SUCCESS)
	return;
#else
    if (tgetent (termbuf, term) != TGETENT_SUCCESS)
	return;
#endif

#ifdef HAVE_TERMIOS_H
    speedcode = cfgetospeed(&tio);
#else
# ifdef HAVE_TERMIO_H
    speedcode = ioctl(fileno(stdout), TCGETA, &tio) != NOTOK ? tio.c_cflag & CBAUD : 0;
# else
    speedcode = ioctl(fileno(stdout), TIOCGETP, (char *) &tio) != NOTOK ? tio.sg_ospeed : 0;
# endif
#endif

    HC = tgetflag ("hc");

    if (!initCO && (CO = tgetnum ("co")) <= 0)
	CO = 80;
    if (!initLI && (LI = tgetnum ("li")) <= 0)
	LI = 24;

    cp = termcap;
    CL = tgetstr ("cl", &cp);
    if ((bp = tgetstr ("pc", &cp)))
	PC = *bp;
    if (tgetnum ("sg") <= 0) {
	SE = tgetstr ("se", &cp);
	SO = tgetstr ("so", &cp);
    }
}


int
sc_width (void)
{
#ifdef TIOCGWINSZ
    struct winsize win;
    int width;

    if (ioctl (fileno (stderr), TIOCGWINSZ, &win) != NOTOK
	    && (width = win.ws_col) > 0) {
	CO = width;
	initCO++;
    } else
#endif /* TIOCGWINSZ */
	read_termcap();

    return CO;
}


int
sc_length (void)
{
#ifdef TIOCGWINSZ
    struct winsize win;

    if (ioctl (fileno (stderr), TIOCGWINSZ, &win) != NOTOK
	    && (LI = win.ws_row) > 0)
	initLI++;
    else
#endif /* TIOCGWINSZ */
	read_termcap();

    return LI;
}


static int
outc (int c)
{
    return putchar(c);
}


void
clear_screen (void)
{
    read_termcap ();

    if (CL && speedcode)
	tputs (CL, LI, outc);
    else {
	printf ("\f");
	if (speedcode)
	    printf ("\200");
    }

    fflush (stdout);
}


/*
 * print in standout mode
 */
int
SOprintf (char *fmt, ...)
{
    va_list ap;

    read_termcap ();
    if (!(SO && SE))
	return NOTOK;

    tputs (SO, 1, outc);

    va_start(ap, fmt);
    vprintf (fmt, ap);
    va_end(ap);

    tputs (SE, 1, outc);

    return OK;
}

/*
 * Is this a hardcopy terminal?
 */

int
sc_hardcopy(void)
{
    read_termcap();
    return HC;
}

