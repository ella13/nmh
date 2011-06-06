
/*
 * whom.c -- report to whom a message would be sent
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <signal.h>

#ifndef CYRUS_SASL
# define SASLminc(a) (a)
#else /* CYRUS_SASL */
# define SASLminc(a)  0
#endif /* CYRUS_SASL */

#ifndef TLS_SUPPORT
# define TLSminc(a)  (a)
#else /* TLS_SUPPORT */
# define TLSminc(a)   0
#endif /* TLS_SUPPORT */

static struct swit switches[] = {
#define	ALIASW              0
    { "alias aliasfile", 0 },
#define	CHKSW               1
    { "check", 0 },
#define	NOCHKSW             2
    { "nocheck", 0 },
#define	DRAFTSW             3
    { "draft", 0 },
#define	DFOLDSW             4
    { "draftfolder +folder", 6 },
#define	DMSGSW              5
    { "draftmessage msg", 6 },
#define	NDFLDSW             6
    { "nodraftfolder", 0 },
#define VERSIONSW           7
    { "version", 0 },
#define	HELPSW              8
    { "help", 0 },
#define	CLIESW              9
    { "client host", -6 },
#define	SERVSW             10
    { "server host", -6 },
#define	SNOOPSW            11
    { "snoop", -5 },
#define SASLSW             12
    { "sasl", SASLminc(4) },
#define SASLMECHSW         13
    { "saslmech mechanism", SASLminc(-5) },
#define USERSW             14
    { "user username", SASLminc(-4) },
#define PORTSW		   15
    { "port server port name/number", 4 },
#define TLSSW		   16
    { "tls", TLSminc(-3) },
    { NULL, 0 }
};


int
main (int argc, char **argv)
{
    pid_t child_id;
    int i, status, isdf = 0;
    int distsw = 0, vecp = 0;
    char *cp, *dfolder = NULL, *dmsg = NULL;
    char *msg = NULL, **ap, **argp, backup[BUFSIZ];
    char buf[BUFSIZ], **arguments, *vec[MAXARGS];

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    vec[vecp++] = invo_name;
    vec[vecp++] = "-whom";
    vec[vecp++] = "-library";
    vec[vecp++] = getcpy (m_maildir (""));

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] [file]", invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case CHKSW: 
		case NOCHKSW: 
		case SNOOPSW:
		case SASLSW:
		    vec[vecp++] = --cp;
		    continue;

		case DRAFTSW:
		    msg = draft;
		    continue;

		case DFOLDSW: 
		    if (dfolder)
			adios (NULL, "only one draft folder at a time!");
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    dfolder = path (*cp == '+' || *cp == '@' ? cp + 1 : cp,
			    *cp != '@' ? TFOLDER : TSUBCWF);
		    continue;
		case DMSGSW: 
		    if (dmsg)
			adios (NULL, "only one draft message at a time!");
		    if (!(dmsg = *argp++) || *dmsg == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
		case NDFLDSW: 
		    dfolder = NULL;
		    isdf = NOTOK;
		    continue;

		case ALIASW: 
		case CLIESW: 
		case SERVSW: 
		case USERSW:
		case PORTSW:
		case SASLMECHSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    vec[vecp++] = cp;
		    continue;
	    }
	}
	if (msg)
	    adios (NULL, "only one draft at a time!");
	else
	    vec[vecp++] = msg = cp;
    }

    /* allow Aliasfile: profile entry */
    if ((cp = context_find ("Aliasfile"))) {
	char *dp = NULL;

	for (ap = brkstring(dp = getcpy(cp), " ", "\n"); ap && *ap; ap++) {
	    vec[vecp++] = "-alias";
	    vec[vecp++] = *ap;
	}
    }

    if (msg == NULL) {
#ifdef	WHATNOW
	if (dfolder || (cp = getenv ("mhdraft")) == NULL || *cp == '\0')
#endif	/* WHATNOW */
	    cp  = getcpy (m_draft (dfolder, dmsg, 1, &isdf));
	msg = vec[vecp++] = cp;
    }
    if ((cp = getenv ("mhdist"))
	    && *cp
	    && (distsw = atoi (cp))
	    && (cp = getenv ("mhaltmsg"))
	    && *cp) {
	if (distout (msg, cp, backup) == NOTOK)
	    done (1);
	vec[vecp++] = "-dist";
	distsw++;
    }
    vec[vecp] = NULL;

    closefds (3);

    if (distsw) {
	for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	    sleep (5);
    }

    switch (distsw ? child_id : OK) {
	case NOTOK:
    	    advise (NULL, "unable to fork, so checking directly...");
	case OK:
	    execvp (postproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (postproc);
	    _exit (-1);

	default:
	    SIGNAL (SIGHUP, SIG_IGN);
	    SIGNAL (SIGINT, SIG_IGN);
	    SIGNAL (SIGQUIT, SIG_IGN);
	    SIGNAL (SIGTERM, SIG_IGN);

	    status = pidwait(child_id, OK);

	    unlink (msg);
	    if (rename (backup, msg) == NOTOK)
		adios (msg, "unable to rename %s to", backup);
	    done (status);
    }

    return 0;  /* dead code to satisfy the compiler */
}
