
/*
 * mf.c -- mail filter subroutines
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mf.h>
#include <ctype.h>
#include <stdio.h>
#include <h/utils.h>

/*
 * static prototypes
 */
static char *getcpy (char *);
static void compress (char *, unsigned char *);
static int isat (char *);
static int parse_address (void);
static int phrase (char *);
static int route_addr (char *);
static int local_part (char *);
static int domain (char *);
static int route (char *);
static int my_lex (char *);


static char *
getcpy (char *s)
{
    register char *p;

    if (!s) {
/* causes compiles to blow up because the symbol _cleanup is undefined 
   where did this ever come from? */
	/* _cleanup(); */
	abort();
	for(;;)
	    pause();
    }
    p = mh_xmalloc ((size_t) (strlen (s) + 2));
    strcpy (p, s);
    return p;
}


int
isfrom(char *string)
{
    return (strncmp (string, "From ", 5) == 0
	    || strncmp (string, ">From ", 6) == 0);
}


int
lequal (unsigned char *a, unsigned char *b)
{
    for (; *a; a++, b++)
	if (*b == 0)
	    return FALSE;
	else {
	    char c1 = islower (*a) ? toupper (*a) : *a;
	    char c2 = islower (*b) ? toupper (*b) : *b;
	    if (c1 != c2)
		return FALSE;
	}

    return (*b == 0);
}


/* 
 * seekadrx() is tricky.  We want to cover both UUCP-style and ARPA-style
 * addresses, so for each list of addresses we see if we can find some
 * character to give us a hint.
 */


#define	CHKADR	0		/* undertermined address style */
#define	UNIXDR	1		/* UNIX-style address */
#define	ARPADR	2		/* ARPAnet-style address */


static char *punctuators = ";<>.()[]";
static char *vp = NULL;
static char *tp = NULL;

static struct adrx adrxs1;


struct adrx *
seekadrx (char *addrs)
{
    static int state = CHKADR;
    register char *cp;
    register struct adrx *adrxp;

    if (state == CHKADR)
	for (state = UNIXDR, cp = addrs; *cp; cp++)
	    if (strchr(punctuators, *cp)) {
		state = ARPADR;
		break;
	    }

    switch (state) {
	case UNIXDR: 
	    adrxp = uucpadrx (addrs);
	    break;

	case ARPADR: 
	default:
	    adrxp = getadrx (addrs);
	    break;
    }

    if (adrxp == NULL)
	state = CHKADR;

    return adrxp;
}


/*
 * uucpadrx() implements a partial UUCP-style address parser.  It's based
 * on the UUCP notion that addresses are separated by spaces or commas.
 */


struct adrx *
uucpadrx (char *addrs)
{
    register unsigned char *cp, *wp, *xp, *yp;
    register char *zp;
    register struct adrx *adrxp = &adrxs1;

    if (vp == NULL) {
	vp = tp = getcpy (addrs);
	compress (addrs, vp);
    }
    else
	if (tp == NULL) {
	    free (vp);
	    vp = NULL;
	    return NULL;
	}

    for (cp = tp; isspace (*cp); cp++)
	continue;
    if (*cp == 0) {
	free (vp);
	vp = tp = NULL;
	return NULL;
    }

    if ((wp = strchr(cp, ',')) == NULL) {
	if ((wp = strchr(cp, ' ')) != NULL) {
	    xp = wp;
	    while (isspace (*xp))
		xp++;
	    if (*xp != 0 && isat (--xp)) {
		yp = xp + 4;
		while (isspace (*yp))
		    yp++;
		if (*yp != 0) {
		    if ((zp = strchr(yp, ' ')) != NULL)
			*zp = 0, tp = ++zp;
		    else
			tp = NULL;
		}
		else
		    *wp = 0, tp = ++wp;
	    }
	    else
		*wp = 0, tp = ++wp;
	}
	else
	    tp = NULL;
    }
    else
	*wp = 0, tp = ++wp;

    if (adrxp->text)
	free (adrxp->text);
    adrxp->text = getcpy (cp);
    adrxp->mbox = cp;
    adrxp->host = adrxp->path = NULL;
    if ((wp = strrchr(cp, '@')) != NULL) {
	*wp++ = 0;
	adrxp->host = *wp ? wp : NULL;
    }
    else
	for (wp = cp + strlen (cp) - 4; wp >= cp; wp--)
	    if (isat (wp)) {
		*wp++ = 0;
		adrxp->host = wp + 3;
	    }

    adrxp->pers = adrxp->grp = adrxp->note = adrxp->err = NULL;
    adrxp->ingrp = 0;

    return adrxp;
}


static void
compress (char *fp, unsigned char *tp)
{
    register char c;
    register unsigned char *cp;

    for (c = ' ', cp = tp; (*tp = *fp++) != 0;)
	if (isspace (*tp)) {
	    if (c != ' ')
		*tp++ = c = ' ';
	}
	else
	    c = *tp++;

    if (c == ' ' && cp < tp)
	*--tp = 0;
}


static int
isat (char *p)
{
    return (strncmp (p, " AT ", 4)
	    && strncmp (p, " At ", 4)
	    && strncmp (p, " aT ", 4)
	    && strncmp (p, " at ", 4) ? FALSE : TRUE);
}


/*
 *
 * getadrx() implements a partial 822-style address parser.  The parser
 * is neither complete nor correct.  It does however recognize nearly all
 * of the 822 address syntax.  In addition it handles the majority of the
 * 733 syntax as well.  Most problems arise from trying to accomodate both.
 *
 * In terms of 822, the route-specification in 
 *
 *		 "<" [route] local-part "@" domain ">"
 *
 * is parsed and returned unchanged.  Multiple at-signs are compressed
 * via source-routing.  Recursive groups are not allowed as per the 
 * standard.
 *
 * In terms of 733, " at " is recognized as equivalent to "@".
 *
 * In terms of both the parser will not complain about missing hosts.
 *
 * -----
 *
 * We should not allow addresses like	
 *
 *		Marshall T. Rose <MRose@UCI>
 *
 * but should insist on
 *
 *		"Marshall T. Rose" <MRose@UCI>
 *
 * Unfortunately, a lot of mailers stupidly let people get away with this.
 *
 * -----
 *
 * We should not allow addresses like
 *
 *		<MRose@UCI>
 *
 * but should insist on
 *
 *		MRose@UCI
 *
 * Unfortunately, a lot of mailers stupidly let people's UAs get away with
 * this.
 *
 * -----
 *
 * We should not allow addresses like
 *
 *		@UCI:MRose@UCI-750a
 *
 * but should insist on
 *
 *		Marshall Rose <@UCI:MRose@UCI-750a>
 *
 * Unfortunately, a lot of mailers stupidly do this.
 *
 */

#define	QUOTE	'\\'

#define	LX_END	 0
#define	LX_ERR	 1
#define	LX_ATOM	 2
#define	LX_QSTR	 3
#define	LX_DLIT	 4
#define	LX_SEMI	 5
#define	LX_COMA	 6
#define	LX_LBRK	 7
#define	LX_RBRK	 8
#define	LX_COLN	 9
#define	LX_DOT	10
#define	LX_AT	11

struct specials {
    char lx_chr;
    int  lx_val;
};

static struct specials special[] = {
    { ';',   LX_SEMI },
    { ',',   LX_COMA },
    { '<',   LX_LBRK },
    { '>',   LX_RBRK },
    { ':',   LX_COLN },
    { '.',   LX_DOT },
    { '@',   LX_AT },
    { '(',   LX_ERR },
    { ')',   LX_ERR },
    { QUOTE, LX_ERR },
    { '"',   LX_ERR },
    { '[',   LX_ERR },
    { ']',   LX_ERR },
    { 0,     0 }
};

static int glevel = 0;
static int ingrp = 0;
static int last_lex = LX_END;

static char *dp = NULL;
static unsigned char *cp = NULL;
static unsigned char *ap = NULL;
static char *pers = NULL;
static char *mbox = NULL;
static char *host = NULL;
static char *path = NULL;
static char *grp = NULL;
static char *note = NULL;
static char err[BUFSIZ];
static char adr[BUFSIZ];

static struct adrx  adrxs2;


struct adrx *
getadrx (char *addrs)
{
    register char *bp;
    register struct adrx *adrxp = &adrxs2;

    if (pers)
	free (pers);
    if (mbox)
	free (mbox);
    if (host)
	free (host);
    if (path)
	free (path);
    if (grp)
	free (grp);
    if (note)
	free (note);
    pers = mbox = host = path = grp = note = NULL;
    err[0] = 0;

    if (dp == NULL) {
	dp = cp = getcpy (addrs ? addrs : "");
	glevel = 0;
    }
    else
	if (cp == NULL) {
	    free (dp);
	    dp = NULL;
	    return NULL;
	}

    switch (parse_address ()) {
	case DONE:
	    free (dp);
	    dp = cp = NULL;
	    return NULL;

	case OK:
	    switch (last_lex) {
		case LX_COMA:
		case LX_END:
		    break;

		default:	/* catch trailing comments */
		    bp = cp;
		    my_lex (adr);
		    cp = bp;
		    break;
	    }
	    break;

	default:
	    break;
	}

    if (err[0])
	for (;;) {
	    switch (last_lex) {
		case LX_COMA: 
		case LX_END: 
		    break;

		default: 
		    my_lex (adr);
		    continue;
	    }
	    break;
	}
    while (isspace (*ap))
	ap++;
    if (cp)
	sprintf (adr, "%.*s", (int)(cp - ap), ap);
    else
	strcpy (adr, ap);
    bp = adr + strlen (adr) - 1;
    if (*bp == ',' || *bp == ';' || *bp == '\n')
	*bp = 0;

    adrxp->text = adr;
    adrxp->pers = pers;
    adrxp->mbox = mbox;
    adrxp->host = host;
    adrxp->path = path;
    adrxp->grp = grp;
    adrxp->ingrp = ingrp;
    adrxp->note = note;
    adrxp->err = err[0] ? err : NULL;

    return adrxp;
}


static int
parse_address (void)
{
    char buffer[BUFSIZ];

again: ;
    ap = cp;
    switch (my_lex (buffer)) {
	case LX_ATOM: 
	case LX_QSTR: 
	    pers = getcpy (buffer);
	    break;

	case LX_SEMI: 
	    if (glevel-- <= 0) {
		strcpy (err, "extraneous semi-colon");
		return NOTOK;
	    }
	case LX_COMA: 
	    if (note) {
		free (note);
		note = NULL;
	    }
	    goto again;

	case LX_END: 
	    return DONE;

	case LX_LBRK: 		/* sigh (2) */
	    goto get_addr;

	case LX_AT:		/* sigh (3) */
	    cp = ap;
	    if (route_addr (buffer) == NOTOK)
		return NOTOK;
	    return OK;		/* why be choosy? */

	default: 
	    sprintf (err, "illegal address construct (%s)", buffer);
	    return NOTOK;
    }

    switch (my_lex (buffer)) {
	case LX_ATOM: 
	case LX_QSTR: 
	    pers = add (buffer, add (" ", pers));
    more_phrase: ;		/* sigh (1) */
	    if (phrase (buffer) == NOTOK)
		return NOTOK;

	    switch (last_lex) {
		case LX_LBRK: 
	    get_addr: ;
		    if (route_addr (buffer) == NOTOK)
			return NOTOK;
		    if (last_lex == LX_RBRK)
			return OK;
		    sprintf (err, "missing right-bracket (%s)", buffer);
		    return NOTOK;

		case LX_COLN: 
	    get_group: ;
		    if (glevel++ > 0) {
			sprintf (err, "nested groups not allowed (%s)", pers);
			return NOTOK;
		    }
		    grp = add (": ", pers);
		    pers = NULL;
		    {
			char   *pp = cp;

			for (;;)
			    switch (my_lex (buffer)) {
				case LX_SEMI: 
				case LX_END: /* tsk, tsk */
				    glevel--;
				    return OK;

				case LX_COMA: 
				    continue;

				default: 
				    cp = pp;
				    return parse_address ();
			    }
		    }

		case LX_DOT: 	/* sigh (1) */
		    pers = add (".", pers);
		    goto more_phrase;

		default: 
		    sprintf (err, "no mailbox in address, only a phrase (%s%s)",
			    pers, buffer);
		    return NOTOK;
	    }

	case LX_LBRK: 
	    goto get_addr;

	case LX_COLN: 
	    goto get_group;

	case LX_DOT: 
	    mbox = add (buffer, pers);
	    pers = NULL;
	    if (route_addr (buffer) == NOTOK)
		return NOTOK;
	    goto check_end;

	case LX_AT: 
	    ingrp = glevel;
	    mbox = pers;
	    pers = NULL;
	    if (domain (buffer) == NOTOK)
		return NOTOK;
    check_end: ;
	    switch (last_lex) {
		case LX_SEMI: 
		    if (glevel-- <= 0) {
			strcpy (err, "extraneous semi-colon");
			return NOTOK;
		    }
		case LX_COMA: 
		case LX_END: 
		    return OK;

		default: 
		    sprintf (err, "junk after local@domain (%s)", buffer);
		    return NOTOK;
	    }

	case LX_SEMI: 		/* no host */
	case LX_COMA: 
	case LX_END: 
	    ingrp = glevel;
	    if (last_lex == LX_SEMI && glevel-- <= 0) {
		strcpy (err, "extraneous semi-colon");
		return NOTOK;
	    }
	    mbox = pers;
	    pers = NULL;
	    return OK;

	default: 
	    sprintf (err, "missing mailbox (%s)", buffer);
	    return NOTOK;
    }
}


static int
phrase (char *buffer)
{
    for (;;)
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_QSTR: 
		pers = add (buffer, add (" ", pers));
		continue;

	    default: 
		return OK;
	}
}


static int
route_addr (char *buffer)
{
    register char *pp = cp;

    if (my_lex (buffer) == LX_AT) {
	if (route (buffer) == NOTOK)
	    return NOTOK;
    }
    else
	cp = pp;

    if (local_part (buffer) == NOTOK)
	return NOTOK;

    switch (last_lex) {
	case LX_AT: 
	    return domain (buffer);

	case LX_SEMI:	/* if in group */
	case LX_RBRK: 		/* no host */
	case LX_COMA:
	case LX_END: 
	    return OK;

	default: 
	    sprintf (err, "no at-sign after local-part (%s)", buffer);
	    return NOTOK;
    }
}


static int
local_part (char *buffer)
{
    ingrp = glevel;

    for (;;) {
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_QSTR: 
		mbox = add (buffer, mbox);
		break;

	    default: 
		sprintf (err, "no mailbox in local-part (%s)", buffer);
		return NOTOK;
	}

	switch (my_lex (buffer)) {
	    case LX_DOT: 
		mbox = add (buffer, mbox);
		continue;

	    default: 
		return OK;
	}
    }
}


static int
domain (char *buffer)
{
    for (;;) {
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_DLIT: 
		host = add (buffer, host);
		break;

	    default: 
		sprintf (err, "no sub-domain in domain-part of address (%s)", buffer);
		return NOTOK;
	}

	switch (my_lex (buffer)) {
	    case LX_DOT: 
		host = add (buffer, host);
		continue;

	    case LX_AT: 	/* sigh (0) */
		mbox = add (host, add ("%", mbox));
		free (host);
		host = NULL;
		continue;

	    default: 
		return OK;
	}
    }
}


static int
route (char *buffer)
{
    path = getcpy ("@");

    for (;;) {
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_DLIT: 
		path = add (buffer, path);
		break;

	    default: 
		sprintf (err, "no sub-domain in domain-part of address (%s)", buffer);
		return NOTOK;
	}
	switch (my_lex (buffer)) {
	    case LX_COMA: 
		path = add (buffer, path);
		for (;;) {
		    switch (my_lex (buffer)) {
			case LX_COMA: 
			    continue;

			case LX_AT: 
			    path = add (buffer, path);
			    break;

			default: 
			    sprintf (err, "no at-sign found for next domain in route (%s)",
			             buffer);
		    }
		    break;
		}
		continue;

	    case LX_AT:		/* XXX */
	    case LX_DOT: 
		path = add (buffer, path);
		continue;

	    case LX_COLN: 
		path = add (buffer, path);
		return OK;

	    default: 
		sprintf (err, "no colon found to terminate route (%s)", buffer);
		return NOTOK;
	}
    }
}


static int
my_lex (char *buffer)
{
    /* buffer should be at least BUFSIZ bytes long */
    int i, gotat = 0;
    register unsigned char c;
    register char *bp;

/* Add C to the buffer bp. After use of this macro *bp is guaranteed to be within the buffer. */
#define ADDCHR(C) do { *bp++ = (C); if ((bp - buffer) == (BUFSIZ-1)) goto my_lex_buffull; } while (0)

    bp = buffer;
    *bp = 0;
    if (!cp)
	return (last_lex = LX_END);

    gotat = isat (cp);
    c = *cp++;
    while (isspace (c))
	c = *cp++;
    if (c == 0) {
	cp = NULL;
	return (last_lex = LX_END);
    }

    if (c == '(') {
	ADDCHR(c);
	for (i = 0;;)
	    switch (c = *cp++) {
		case 0: 
		    cp = NULL;
		    return (last_lex = LX_ERR);
		case QUOTE: 
		    ADDCHR(c);
		    if ((c = *cp++) == 0) {
			cp = NULL;
			return (last_lex = LX_ERR);
		    }
		    ADDCHR(c);
		    continue;
		case '(': 
		    i++;
		default: 
		    ADDCHR(c);
		    continue;
		case ')': 
		    ADDCHR(c);
		    if (--i < 0) {
			*bp = 0;
			note = note ? add (buffer, add (" ", note))
			    : getcpy (buffer);
			return my_lex (buffer);
		    }
	    }
    }

    if (c == '"') {
	ADDCHR(c);
	for (;;)
	    switch (c = *cp++) {
		case 0: 
		    cp = NULL;
		    return (last_lex = LX_ERR);
		case QUOTE: 
		    ADDCHR(c);
		    if ((c = *cp++) == 0) {
			cp = NULL;
			return (last_lex = LX_ERR);
		    }
		default: 
		    ADDCHR(c);
		    continue;
		case '"': 
		    ADDCHR(c);
		    *bp = 0;
		    return (last_lex = LX_QSTR);
	    }
    }
    
    if (c == '[') {
	ADDCHR(c);
	for (;;)
	    switch (c = *cp++) {
		case 0: 
		    cp = NULL;
		    return (last_lex = LX_ERR);
		case QUOTE: 
		    ADDCHR(c);
		    if ((c = *cp++) == 0) {
			cp = NULL;
			return (last_lex = LX_ERR);
		    }
		default: 
		    ADDCHR(c);
		    continue;
		case ']': 
		    ADDCHR(c);
		    *bp = 0;
		    return (last_lex = LX_DLIT);
	    }
    }
    
    ADDCHR(c);
    *bp = 0;
    for (i = 0; special[i].lx_chr != 0; i++)
	if (c == special[i].lx_chr)
	    return (last_lex = special[i].lx_val);

    if (iscntrl (c))
	return (last_lex = LX_ERR);

    for (;;) {
	if ((c = *cp++) == 0)
	    break;
	for (i = 0; special[i].lx_chr != 0; i++)
	    if (c == special[i].lx_chr)
		goto got_atom;
	if (iscntrl (c) || isspace (c))
	    break;
	ADDCHR(c);
    }
got_atom: ;
    if (c == 0)
	cp = NULL;
    else
	cp--;
    *bp = 0;
    last_lex = !gotat || cp == NULL || strchr(cp, '<') != NULL
	? LX_ATOM : LX_AT;
    return last_lex;

 my_lex_buffull:
    /* Out of buffer space. *bp is the last byte in the buffer */
    *bp = 0;
    return (last_lex = LX_ERR);
}


char *
legal_person (char *p)
{
    int i;
    register char *cp;
    static char buffer[BUFSIZ];

    if (*p == '"')
	return p;
    for (cp = p; *cp; cp++)
	for (i = 0; special[i].lx_chr; i++)
	    if (*cp == special[i].lx_chr) {
		sprintf (buffer, "\"%s\"", p);
		return buffer;
	    }

    return p;
}


int
mfgets (FILE *in, char **bp)
{
    int i;
    register char *cp, *dp, *ep;
    static int len = 0;
    static char *pp = NULL;

    if (pp == NULL)
	pp = mh_xmalloc ((size_t) (len = BUFSIZ));

    for (ep = (cp = pp) + len - 2;;) {
	switch (i = getc (in)) {
	    case EOF: 
	eol: 	;
		if (cp != pp) {
		    *cp = 0;
		    *bp = pp;
		    return OK;
		}
	eoh:	;
		*bp = NULL;
		free (pp);
		pp = NULL;
		return DONE;

	    case 0: 
		continue;

	    case '\n': 
		if (cp == pp)	/* end of headers, gobble it */
		    goto eoh;
		switch (i = getc (in)) {
		    default: 	/* end of line */
		    case '\n': 	/* end of headers, save for next call */
			ungetc (i, in);
			goto eol;

		    case ' ': 	/* continue headers */
		    case '\t': 
			*cp++ = '\n';
			break;
		}		/* fall into default case */

	    default: 
		*cp++ = i;
		break;
	}
	if (cp >= ep) {
	    dp = mh_xrealloc (pp, (size_t) (len += BUFSIZ));
	    cp += dp - pp, ep = (pp = cp) + len - 2;
	}
    }
}
