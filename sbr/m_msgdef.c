
/*
 * m_msgdef.c -- some defines for sbr/m_getfld.c
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/*
 * disgusting hack for "inc" so it can know how many characters
 * were stuffed in the buffer on the last call (see comments
 * in uip/scansbr.c)
 */
int msg_count = 0;

int msg_style = MS_DEFAULT;

/*
 * The "full" delimiter string for a packed maildrop consists
 * of a newline followed by the actual delimiter.  E.g., the
 * full string for a Unix maildrop would be: "\n\nFrom ".
 * "Fdelim" points to the start of the full string and is used
 * in the BODY case of the main routine to search the buffer for
 * a possible eom.  Msg_delim points to the first character of
 * the actual delim. string (i.e., fdelim+1).  Edelim
 * points to the 2nd character of actual delimiter string.  It
 * is used in m_Eom because the first character of the string
 * has been read and matched before m_Eom is called.
 */
char *msg_delim = "";
