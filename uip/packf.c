
/*
 * packf.c -- pack a nmh folder into a file
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/dropsbr.h>
#include <h/utils.h>
#include <errno.h>

static struct swit switches[] = {
#define FILESW         0
    { "file name", 0 },
#define MBOXSW         1
    { "mbox", 0 },
#define MMDFSW         2
    { "mmdf", 0 },
#define VERSIONSW      3
    { "version", 0 },
#define	HELPSW         4
    { "help", 0 },
    { NULL, 0 }
};

static int md = NOTOK;
static int mbx_style = MBOX_FORMAT;
static int mapping = 0;

static void mbxclose_done(int) NORETURN;

char *file = NULL;


int
main (int argc, char **argv)
{
    int fd, msgnum;
    char *cp, *maildir, *msgnam, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;
    struct stat st;

    done=mbxclose_done;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case FILESW: 
		    if (file)
			adios (NULL, "only one file at a time!");
		    if (!(file = *argp++) || *file == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case MBOXSW:
		    mbx_style = MBOX_FORMAT;
		    mapping = 0;
		    continue;
		case MMDFSW:
		    mbx_style = MMDF_FORMAT;
		    mapping = 1;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    folder = pluspath (cp);
	} else
		app_msgarg(&msgs, cp);
    }

    if (!file)
	file = "./msgbox";
    file = path (file, TFILE);

    /*
     * Check if file to be created (or appended to)
     * exists.  If not, ask for confirmation.
     */
    if (stat (file, &st) == NOTOK) {
	if (errno != ENOENT)
	    adios (file, "error on file");
	cp = concat ("Create file \"", file, "\"? ", NULL);
	if (!getanswer (cp))
	    done (1);
	free (cp);
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /* default is to pack whole folder */
    if (!msgs.size)
	app_msgarg(&msgs, "all");

    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to ");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	adios (NULL, "no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    /* open and lock new maildrop file */
    if ((md = mbx_open(file, mbx_style, getuid(), getgid(), m_gmprot())) == NOTOK)
	adios (file, "unable to open");

    /* copy all the SELECTED messages to the file */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected(mp, msgnum)) {
	    if ((fd = open (msgnam = m_name (msgnum), O_RDONLY)) == NOTOK) {
		admonish (msgnam, "unable to read message");
		break;
	    }

	    if (mbx_copy (file, mbx_style, md, fd, mapping, NULL, 1) == NOTOK)
		adios (file, "error writing to file");

	    close (fd);
	}

    /* close and unlock maildrop file */
    mbx_close (file, md);

    context_replace (pfolder, folder);	/* update current folder         */
    if (mp->hghsel != mp->curmsg)
	seq_setcur (mp, mp->lowsel);
    seq_save (mp);
    context_save ();			/* save the context file         */
    folder_free (mp);			/* free folder/message structure */
    done (0);
    return 1;
}

static void
mbxclose_done (int status)
{
    mbx_close (file, md);
    exit (status);
}
