
/*
 * rcvmail.h -- rcvmail hook definitions
 */

#if defined(SMTPMTS)
# include <ctype.h>
# include <errno.h>
# include <setjmp.h>
# include <stdio.h>
# include <sys/types.h>
# include <mts/smtp/smtp.h>
#endif /* SMTPMTS */


#if defined(SMTPMTS)
# define RCV_MOK	0
# define RCV_MBX	1
#endif /* SMTPMTS */


#ifdef NRTC			/* sigh */
# undef RCV_MOK
# undef RCV_MBX
# define RCV_MOK	RP_MOK
# define RCV_MBX	RP_MECH
#endif /* NRTC */
