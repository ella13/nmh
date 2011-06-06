
/*
 * gans.c -- get an answer from the user
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
gans (char *prompt, struct swit *ansp)
{
    register int i;
    register char *cp;
    register struct swit *ap;
    char ansbuf[BUFSIZ];

    for (;;) {
	printf ("%s", prompt);
	fflush (stdout);
	cp = ansbuf;
	while ((i = getchar ()) != '\n') {
	    if (i == EOF)
		return 0;
	    if (cp < &ansbuf[sizeof ansbuf - 1]) {
#ifdef LOCALE
		i = (isalpha(i) && isupper(i)) ? tolower(i) : i;
#else
		if (i >= 'A' && i <= 'Z')
		    i += 'a' - 'A';
#endif
		*cp++ = i;
	    }
	}
	*cp = '\0';
	if (ansbuf[0] == '?' || cp == ansbuf) {
	    printf ("Options are:\n");
	    for (ap = ansp; ap->sw; ap++)
		printf ("  %s\n", ap->sw);
	    continue;
	}
	if ((i = smatch (ansbuf, ansp)) < 0) {
	    printf ("%s: %s.\n", ansbuf, i == -1 ? "unknown" : "ambiguous");
	    continue;
	}
	return i;
    }
}
