
/*
 * popsbr.h -- header for POP client subroutines
 */

int pop_init (char *, char *, char *, char *, char *, int, int, char *);
int pop_fd (char *, int, char *, int);
int pop_stat (int *, int *);
int pop_retr (int, int (*)(char *));
int pop_dele (int);
int pop_noop (void);
int pop_rset (void);
int pop_top (int, int, int (*)(char *));
int pop_quit (void);
int pop_done (void);
int pop_set (int, int, int);
int pop_list (int, int *, int *, int *);
