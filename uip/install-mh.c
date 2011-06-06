/*
 * install-mh.c -- initialize the nmh environment of a new user
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>				/* mh internals */
#include <h/utils.h>
#include <pwd.h>				/* structure for getpwuid() results */

static struct swit switches[] = {
#define AUTOSW     0
    { "auto", 0 },
#define VERSIONSW  1
    { "version", 0 },
#define HELPSW     2
    { "help", 0 },
#define CHECKSW     3
    { "check", 1 },
    { NULL, 0 }
};

/*
 * static prototypes
 */
static char *geta(void);


int
main (int argc, char **argv)
{
    int autof = 0;
    char *cp, *pathname, buf[BUFSIZ];
    char *dp, **arguments, **argp;
    struct node *np;
    struct passwd *pw;
    struct stat st;
    FILE *in, *out;
    int		check;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');
    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    check = 0;

    while ((dp = *argp++)) {
	if (*dp == '-') {
	    switch (smatch (++dp, switches)) {
		case AMBIGSW:
		    ambigsw (dp, switches);
		    done (1);
		case UNKWNSW:
		    adios (NULL, "-%s unknown\n", dp);

		case HELPSW:
		    snprintf (buf, sizeof(buf), "%s [switches]", invo_name);
		    print_help (buf, switches, 0);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case AUTOSW:
		    autof++;
		    continue;

		case CHECKSW:
		    check = 1;
		    continue;
	    }
	} else {
	    adios (NULL, "%s is invalid argument", dp);
	}
    }

    /*
     *	Find user's home directory.  Try the HOME environment variable first,
     *	the home directory field in the password file if that's not found.
     */

    if ((mypath = getenv("HOME")) == (char *)0) {
	if ((pw = getpwuid(getuid())) == (struct passwd *)0 || *pw->pw_dir == '\0')
	    adios(NULL, "cannot determine your home directory");
	else
	    mypath = pw->pw_dir;
    }

    /*
     *	Find the user's profile.  Check for the existence of an MH environment
     *	variable first with non-empty contents.  Convert any relative path name
     *	found there to an absolute one.  Look for the profile in the user's home
     *	directory if the MH environment variable isn't set.
     */

    if ((cp = getenv("MH")) && *cp != '\0')
	defpath = path(cp, TFILE);
    else
	defpath = concat(mypath, "/", mh_profile, NULL);

    /*
     *	Check for the existence of the profile file.  It's an error if it exists and
     *	this isn't an installation check.  An installation check fails if it does not
     *	exist, succeeds if it does.
     */

    if (stat (defpath, &st) != NOTOK) {
	if (check)
	    done(0);

	else if (autof)
	    adios (NULL, "invocation error");
	else
	    adios (NULL, "You already have an nmh profile, use an editor to modify it");
    }
    else if (check) {
	done(1);
    }

    if (!autof && gans ("Do you want help? ", anoyes)) {
	(void)printf(
	 "\n"
	 "Prior to using nmh, it is necessary to have a file in your login\n"
	 "directory (%s) named %s which contains information\n"
	 "to direct certain nmh operations.  The only item which is required\n"
	 "is the path to use for all nmh folder operations.  The suggested nmh\n"
	 "path for you is %s/Mail...\n"
	 "\n", mypath, mh_profile, mypath);
    }

    cp = concat (mypath, "/", "Mail", NULL);
    if (stat (cp, &st) != NOTOK) {
	if (S_ISDIR(st.st_mode)) {
	    cp = concat ("You already have the standard nmh directory \"",
		    cp, "\".\nDo you want to use it for nmh? ", NULL);
	    if (gans (cp, anoyes))
		pathname = "Mail";
	    else
		goto query;
	} else {
	    goto query;
	}
    } else {
	if (autof)
	    printf ("I'm going to create the standard nmh path for you.\n");
	else
	    cp = concat ("Do you want the standard nmh path \"",
		    mypath, "/", "Mail\"? ", NULL);
	if (autof || gans (cp, anoyes))
	    pathname = "Mail";
	else {
query:
	    if (gans ("Do you want a path below your login directory? ",
			anoyes)) {
		printf ("What is the path?  %s/", mypath);
		pathname = geta ();
	    } else {
		printf ("What is the whole path?  /");
		pathname = concat ("/", geta (), NULL);
	    }
	}
    }

    chdir (mypath);
    if (chdir (pathname) == NOTOK) {
	cp = concat ("\"", pathname, "\" doesn't exist; Create it? ", NULL);
	if (autof || gans (cp, anoyes))
	    if (makedir (pathname) == 0)
		adios (NULL, "unable to create %s", pathname);
    } else {
	printf ("[Using existing directory]\n");
    }

    /*
     * Add some initial elements to the profile/context list
     */
    m_defs = (struct node *) mh_xmalloc (sizeof *np);
    np = m_defs;
    np->n_name = getcpy ("Path");
    np->n_field = getcpy (pathname);
    np->n_context = 0;
    np->n_next = NULL;

    /*
     * If there is a default profile file in the
     * nmh `etc' directory, then read it also.
     */
    if ((in = fopen (mh_defaults, "r"))) {
	readconfig (&np->n_next, in, mh_defaults, 0);
	fclose (in);
    }

    ctxpath = getcpy (m_maildir (context = "context"));

    /* Initialize current folder to default */
    context_replace (pfolder, defaultfolder);
    context_save ();

    /*
     * Now write out the initial .mh_profile
     */
    if ((out = fopen (defpath, "w")) == NULL)
	adios (defpath, "unable to write");
    for (np = m_defs; np; np = np->n_next) {
	if (!np->n_context)
	    fprintf (out, "%s: %s\n", np->n_name, np->n_field);
    }
    fclose (out);
    done (0);
    return 1;
}


static char *
geta (void)
{
    char *cp;
    static char line[BUFSIZ];

    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
	done (1);
    if ((cp = strchr(line, '\n')))
	*cp = 0;
    return line;
}
