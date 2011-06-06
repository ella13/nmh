
/*
 * mhcachesbr.c -- routines to manipulate the MIME content cache
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/mhcachesbr.h>
#include <h/utils.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef TM_IN_SYS_TIME
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

extern int debugsw;

extern pid_t xpid;	/* mhshowsbr.c or mhbuildsbr.c */

/* cache policies */
int rcachesw = CACHE_ASK;
int wcachesw = CACHE_ASK;

/*
 * Location of public and private cache.  These must
 * be set before these routines are called.
 */
char *cache_public;
char *cache_private;


/* mhparse.c (OR) mhbuildsbr.c */
int pidcheck (int);

/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
int make_intermediates (char *);
void content_error (char *, CT, char *, ...);
void flush_errors (void);

/*
 * prototypes
 */
void cache_all_messages (CT *);
int find_cache (CT, int, int *, char *, char *, int);

/*
 * static prototypes
 */
static void cache_content (CT);
static int find_cache_aux (int, char *, char *, char *, int);
static int find_cache_aux2 (char *, char *, char *, int);


/*
 * Top level entry point to cache content
 * from a group of messages
 */

void
cache_all_messages (CT *cts)
{
    CT ct, *ctp;

    for (ctp = cts; *ctp; ctp++) {
	ct = *ctp;
	if (type_ok (ct, 1)) {
	    cache_content (ct);
	    if (ct->c_fp) {
		fclose (ct->c_fp);
		ct->c_fp = NULL;
	    }
	    if (ct->c_ceclosefnx)
		(*ct->c_ceclosefnx) (ct);
	}
    }
    flush_errors ();
}


/*
 * Entry point to cache content from external sources.
 */

static void
cache_content (CT ct)
{
    int	cachetype;
    char *file, cachefile[BUFSIZ];
    CE ce = ct->c_cefile;

    if (!ct->c_id) {
	advise (NULL, "no %s: field in %s", ID_FIELD, ct->c_file);
	return;
    }

    if (!ce) {
	advise (NULL, "unable to decode %s", ct->c_file);
	return;
    }

/* THIS NEEDS TO BE FIXED */
#if 0
    if (ct->c_ceopenfnx == openMail) {
	advise (NULL, "a radish may no know Greek, but I do...");
	return;
    }
#endif

    if (find_cache (NULL, wcachesw != CACHE_NEVER ? wcachesw : CACHE_ASK,
		    &cachetype, ct->c_id, cachefile, sizeof(cachefile))
	    == NOTOK) {
	advise (NULL, "unable to cache %s's contents", ct->c_file);
	return;
    }
    if (wcachesw != CACHE_NEVER && wcachesw != CACHE_ASK) {
	fflush (stdout);
	fprintf (stderr, "caching message %s as file %s\n", ct->c_file,
		 cachefile);
    }

    if (ce->ce_file) {
	int mask = umask (cachetype ? ~m_gmprot () : 0222);
	FILE *fp;

	if (debugsw)
	    fprintf (stderr, "caching by copying %s...\n", ce->ce_file);

	file = NULL;
	if ((*ct->c_ceopenfnx) (ct, &file) == NOTOK)
	    goto reset_umask;

	if ((fp = fopen (cachefile, "w"))) {
	    int	cc;
	    char buffer[BUFSIZ];
	    FILE *gp = ce->ce_fp;

	    fseek (gp, 0L, SEEK_SET);

	    while ((cc = fread (buffer, sizeof(*buffer), sizeof(buffer), gp))
		       > 0)
		fwrite (buffer, sizeof(*buffer), cc, fp);
	    fflush (fp);

	    if (ferror (gp)) {
		admonish (ce->ce_file, "error reading");
		unlink (cachefile);
	    } else {
		if (ferror (fp)) {
		    admonish (cachefile, "error writing");
		    unlink (cachefile);
		}
	    }
	    fclose (fp);
	} else
	    content_error (cachefile, ct, "unable to fopen for writing");
reset_umask:
	umask (mask);
    } else {
	if (debugsw)
	    fprintf (stderr, "in place caching...\n");

	file = cachefile;
	if ((*ct->c_ceopenfnx) (ct, &file) != NOTOK)
	    chmod (cachefile, cachetype ? m_gmprot () : 0444);
    }
}


int
find_cache (CT ct, int policy, int *writing, char *id,
	char *buffer, int buflen)
{
    int	status = NOTOK;

    if (id == NULL)
	return NOTOK;
    id = trimcpy (id);

    if (debugsw)
	fprintf (stderr, "find_cache %s(%d) %s %s\n", caches[policy].sw,
		 policy, writing ? "writing" : "reading", id);

    switch (policy) {
	case CACHE_NEVER:
	default:
	    break;

	case CACHE_ASK:
	case CACHE_PUBLIC:
	    if (cache_private
		    && !writing
		    && find_cache_aux (writing ? 2 : 0, cache_private, id,
				       buffer, buflen) == OK) {
		if (access (buffer, R_OK) != NOTOK) {
got_private:
		    if (writing)
			*writing = 1;
got_it:
		    status = OK;
		    break;
		}
	    }
	    if (cache_public
		    && find_cache_aux (writing ? 1 : 0, cache_public, id,
				       buffer, buflen) == OK) {
		if (writing || access (buffer, R_OK) != NOTOK) {
		    if (writing)
			*writing = 0;
		    goto got_it;
		}
	    }
	    break;

	case CACHE_PRIVATE:
	    if (cache_private
		    && find_cache_aux (writing ? 2 : 0, cache_private, id,
				       buffer, buflen) == OK) {
		if (writing || access (buffer, R_OK) != NOTOK)
		    goto got_private;
	    }
	    break;

    }

    if (status == OK && policy == CACHE_ASK) {
	int len, buflen;
	char *bp, query[BUFSIZ];

	if (xpid) {
	    if (xpid < 0)
		xpid = -xpid;
	    pidcheck (pidwait (xpid, NOTOK));
	    xpid = 0;
	}

	/* Get buffer ready to go */
	bp = query;
	buflen = sizeof(query);

	/* Now, construct query */
	if (writing) {
	    snprintf (bp, buflen, "Make cached, publically-accessible copy");
	} else {
	    struct stat st;

	    snprintf (bp, buflen, "Use cached copy");
	    len = strlen (bp);
	    bp += len;
	    buflen -= len;

	    if (ct->c_partno) {
		snprintf (bp, buflen, " of content %s", ct->c_partno);
		len = strlen (bp);
		bp += len;
		buflen -= len;
	    }

	    stat (buffer, &st);
	    snprintf (bp, buflen, " (size %lu octets)",
			    (unsigned long) st.st_size);
	}
	len = strlen (bp);
	bp += len;
	buflen -= len;

	snprintf (bp, buflen, "\n    in file %s? ", buffer);

	/* Now, check answer */
	if (!getanswer (query))
	    status = NOTOK;
    }

    if (status == OK && writing) {
	if (*writing && strchr(buffer, '/'))
	    make_intermediates (buffer);
	unlink (buffer);
    }

    free (id);
    return status;
}


static int
find_cache_aux (int writing, char *directory, char *id,
	char *buffer, int buflen)
{
    int	mask, usemap;
    char mapfile[BUFSIZ], mapname[BUFSIZ];
    FILE *fp;
    static int partno, pid;
    static time_t clock = 0;

#ifdef BSD42
    usemap = strchr (id, '/') ? 1 : 0;
#else
    usemap = 1;
#endif

    if (debugsw)
	fprintf (stderr, "find_cache_aux %s usemap=%d\n", directory, usemap);

    snprintf (mapfile, sizeof(mapfile), "%s/cache.map", directory);
    if (find_cache_aux2 (mapfile, id, mapname, sizeof(mapname)) == OK)
	goto done_map;

    if (!writing) {
	if (usemap)
	    return NOTOK;

use_raw:
	snprintf (buffer, buflen, "%s/%s", directory, id);
	return OK;
    }

    if (!usemap && access (mapfile, W_OK) == NOTOK)
	goto use_raw;

    if (clock != 0) {
	time_t now;
	
	time (&now);
	if (now > clock)
	    clock = 0;
    } else {
	pid = getpid ();
    }

    if (clock == 0) {
	time (&clock);
	partno = 0;
    } else {
	if (partno > 0xff) {
	    clock++;
	    partno = 0;
	}
    }

    snprintf (mapname, sizeof(mapname), "%08x%04x%02x",
		(unsigned int) (clock & 0xffffffff),
		(unsigned int) (pid & 0xffff),
		(unsigned int) (partno++ & 0xff));

    if (debugsw)
	fprintf (stderr, "creating mapping %s->%s\n", mapname, id);

    make_intermediates (mapfile);
    mask = umask (writing == 2 ? 0077 : 0);
    if (!(fp = lkfopen (mapfile, "a")) && errno == ENOENT) {
	int fd;

	if ((fd = creat (mapfile, 0666)) != NOTOK) {
	    close (fd);
	    fp = lkfopen (mapfile, "a");
	}
    }
    umask (mask);
    if (!fp)
	return NOTOK;
    fprintf (fp, "%s: %s\n", mapname, id);
    lkfclose (fp, mapfile);

done_map:
    if (*mapname == '/')
	strncpy (buffer, mapname, buflen);
    else
	snprintf (buffer, buflen, "%s/%s", directory, mapname);
    if (debugsw)
	fprintf (stderr, "use %s\n", buffer);

    return OK;
}


static int
find_cache_aux2 (char *mapfile, char *id, char *mapname, int namelen)
{
    int	state;
    char buf[BUFSIZ], name[NAMESZ];
    FILE *fp;

    if (!(fp = lkfopen (mapfile, "r")))
	return NOTOK;

    for (state = FLD;;) {
	int result;
	char *cp, *dp;

	switch (state = m_getfld (state, name, buf, sizeof(buf), fp)) {
	    case FLD:
	    case FLDPLUS:
	    case FLDEOF:
	        strncpy (mapname, name, namelen);
		if (state != FLDPLUS)
		    cp = buf;
		else {
		    cp = add (buf, NULL);
		    while (state == FLDPLUS) {
			state = m_getfld (state, name, buf, sizeof(buf), fp);
			cp = add (buf, cp);
		    }
		}
		dp = trimcpy (cp);
		if (cp != buf)
		    free (cp);
		if (debugsw)
		    fprintf (stderr, "compare %s to %s <- %s\n", id, dp,
			     mapname);
		result = strcmp (id, dp);
		free (dp);
		if (result == 0) {
		    lkfclose (fp, mapfile);
		    return OK;
		}
		if (state != FLDEOF)
		    continue;
		/* else fall... */

	    case BODY:
	    case BODYEOF:
	    case FILEEOF:
	    default:
		break;
	}
	break;
    }

    lkfclose (fp, mapfile);
    return NOTOK;
}
