#include <config.h>

#if (SCREEN_LINES > 0)

#if defined(HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif

#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif

#include "xctype.h"
#include "xstring.h"
#include "xunistd.h"

#include "ch.h"
#include "keybrd.h"
#include "yesorno.h"

#define LAST_SCREEN_LINE (-2)	/* used in opt_help() and do_more() */

#if defined(MAX)
#undef MAX
#endif
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))

#ifdef MIN
#undef MIN
#endif /* MIN */
#define	MIN(a,b)	((a) < (b) ? (a) : (b))

#undef MAX_CHAR
#define MAX_CHAR		256

static keyboard_code_t	keymap[MAX_CHAR];

extern int	screen_lines;		/* kbopen() and out_lines() reset */

extern FILE	*tfopen ARGS((const char *filename_, const char *mode_));

static void	beep ARGS((FILE *fpout));

int		do_more ARGS((FILE *fpout_, int line_, int pause_after_, const char *lines[]));

static int	do_search ARGS((FILE *fpout_, int code_, int line_number_, int pause_after_, const char *lines_[]));

static void	erase_characters ARGS((FILE *fpout_, int how_many_));

static          keyboard_code_t kbcode ARGS((void));

static int	kbget ARGS((void));

static void	kbinitmap ARGS((void));


static void
beep(FILE *fpout)
{
    (void)fputc(CH_BELL,fpout);
    (void)fflush(fpout);
}


int
do_more(FILE *fpout,int line_number, int pause_after, const char *lines[])
{
    int code;

#define MORE_HELP \
"More? f)orward b)ackward d)own e)nd q)uit r)efresh t)op u)p \\/)search\n\r"

    (void)fputs(MORE_HELP,fpout);
    (void)fflush(fpout);		/* make screen up-to-date */
    for (;;)			/* loop until a valid input code is received */
    {
	switch (code = kbcode())
	{
	case KEYBOARD_PGUP:		/* backward screen */
	    return (MAX(0,line_number + 1 - 2*pause_after));

	case KEYBOARD_DOWN:		/* go down 1 line (scroll up 1 line) */
	    return (line_number + 2 - pause_after);

	case KEYBOARD_END:		/* end */
	    return (LAST_SCREEN_LINE);

	case KEYBOARD_PGDN:		/* forward screen */
	    return (line_number + 1);

	case KEYBOARD_HELP:
	    (void)fputs(MORE_HELP,fpout);
	    break;

	case KEYBOARD_EOF:
	case KEYBOARD_QUIT:
	    return (EOF);

	case KEYBOARD_AGAIN:		/* refresh */
	    return (MAX(0,line_number + 1 - pause_after));

	case KEYBOARD_HOME:		/* top */
	    return (0);

	case KEYBOARD_UP:		/* go up 1 line (scroll down 1 line) */
	    return (line_number + 0 - pause_after);

	case KEYBOARD_DOWN_PARAGRAPH:
	    /* Move down so that paragraph following the paragraph of
	       the current top-of-screen line is at top-of-screen */
	    line_number++;
	    if (line_number >= (screen_lines - 2))
		line_number -= (screen_lines - 2);
	    while ((lines[line_number] != (char*)NULL) &&
		   (lines[line_number][0] != '\n'))
		line_number++;
	    return ((lines[line_number] == (char*)NULL) ? LAST_SCREEN_LINE :
		    line_number + 1);

	case KEYBOARD_UP_PARAGRAPH:
	    /* Move up so that paragraph of current top-of-screen line
	       has its first line at top of screen. */
	    line_number--;
	    if (line_number >= (screen_lines - 2))
		line_number -= (screen_lines - 2);
	    while ((line_number >= 0) && (lines[line_number] != (char*)NULL) &&
		   (lines[line_number][0] != '\n'))
		line_number--;
	    return (line_number + 1);

        case KEYBOARD_SEARCH_BACKWARD:
        case KEYBOARD_SEARCH_FORWARD:
	    return (do_search(fpout,code,line_number,pause_after,lines));

	case KEYBOARD_UNKNOWN:
	default:			/* anything else produces */
	    beep(fpout);		/* an error beep */
	    break;
	}				/* end switch (c...) */
    }					/* end for (;;) */
}


static int
do_search(FILE *fpout, int code, int line_number, int pause_after, const char *lines[])
{
    int c;
    int k;
    int last_line_number = line_number;

    /*@-modobserver@*/
    static char search_string[80] = ""; /* preserved across calls */

    (void)fputs((code == KEYBOARD_SEARCH_BACKWARD) ?
		"Search backward: " :
		"Search forward: ",fpout);
    (void)fflush(fpout);
    for (k = 0; ;)
    {
	c = kbget();
	if ((c == CH_NUL) || (c == CH_ESCAPE) ||
	    (c == (int)'\r') || (c == (int)'\n'))
	    break;		/* here's the loop exit */
	else if ((c == CH_BACKSPACE) || (c == CH_DELETE))
	{
	    erase_characters(fpout,1);
	    if (k > 0)
		k--;
	    search_string[k] = '\0';
	}
	else if (c == CH_LINE_KILL)	/* erase entire search string */
	{
	    erase_characters(fpout,k);
	    k = 0;
	    search_string[k] = '\0';
	}
	else if (c == CH_REPRINT)	/* reprint entire search string */
	{
	    int m;

	    erase_characters(fpout,k);
	    for (m = 0; m < k; ++m)
		(void)fputc((int)search_string[m],fpout);
	    (void)fflush(fpout);
	}
	else if (c == CH_WORD_ERASE)	/* erase last non-blank word */
	{
	    YESorNO saw_word;

	    saw_word = NO;
	    for (--k ; k >= 0; --k)
	    {
		if (Isspace(search_string[k]) && (saw_word == YES))
		{
		    k++;	/* so that we keep this space */
		    break;
		}
		if (!Isspace(search_string[k]))
		    saw_word = YES;
		erase_characters(fpout,1);
	    }
	    if (k < 0)
		k = 0;
	    search_string[k] = '\0';
	    (void)fflush(fpout);
	}
	else if (k < ((int)sizeof(search_string) - 1))
	{
	    search_string[k++] = (char)c;
	    (void)fputc(c,fpout);	/* echo input character */
	    (void)fflush(fpout);
	}
	else			/* search string too long: beep at user */
	    beep(fpout);
    }
    if (k > 0)			/* got new search string */
	search_string[k] = '\0';
    else			/* re-use last search string */
    {
	(void)fputs(search_string,fpout);
	(void)fputc('\r',fpout);
	(void)fputc('\n',fpout);
	(void)fflush(fpout);
    }
    (void)fputc('\r',fpout);

    for (k = (int)(strlen("Search backward: ") + strlen(search_string));
	 (k > 0); --k)
	(void)fputc(' ',fpout);	/* erase the search line */
    (void)fputc('\r',fpout);

    (void)fflush(fpout);

#if defined(DEBUG)
    (void)sleep(1);		/* DEBUG delay */
#endif

    if (code == KEYBOARD_SEARCH_BACKWARD)
	line_number = MAX(0,line_number - pause_after);
    else
	line_number++;
    while ((line_number >= 0) &&
	   (lines[line_number] != (const char*)NULL))
    {
	if (stristr(lines[line_number],search_string) != (char*)NULL)
	    break;
	else if (code == KEYBOARD_SEARCH_BACKWARD)
	    line_number--;
	else
	    line_number++;
    }
    if ((line_number < 0) || (lines[line_number] == (const char*)NULL))
    {				/* then search failed */
	beep(fpout);
	line_number = MAX(0,last_line_number + 1 - pause_after);
    }
    return (line_number);
}


static void
erase_characters(FILE *fpout, int how_many)
{
    for ( ; how_many > 0; --how_many)
    {
	(void)fputc(CH_BACKSPACE,fpout);
	(void)fputc(' ',fpout);
	(void)fputc(CH_BACKSPACE,fpout);
    }
    (void)fflush(fpout);
}


static void	reset_terminal ARGS((void));
static void	set_terminal ARGS((void));

static FILE	*fptty = (FILE*)NULL;	/* for kbxxx() functions */
static YESorNO	tty_init = NO;		/* set to YES if tty_save set */


void
kbclose(VOID)
{
    reset_terminal();
    if (fptty != (FILE*)NULL)
	(void)fclose(fptty);
}


keyboard_code_t
kbcode(VOID)
{
    int c = kbget();

    if (c == EOF)
	return (KEYBOARD_EOF);
    else if (c == CH_ESCAPE)
    {					/* handle some X Window System keys */
	c = kbget();
	if (c == (int)'[')		/* ']' for balance */
	{
	    c = kbget();
	    if (c == (int)'A')		/* "\e[A" ("]" for balance) */
		return (KEYBOARD_UP);
	    else if (c == (int)'B')	/* "\e[B" ("]" for balance) */
		return (KEYBOARD_DOWN);
	    else if (c == (int)'C')	/* "\e[C" ("]" for balance) */
		return (KEYBOARD_END);
	    else if (c == (int)'D')	/* "\e[D" ("]" for balance) */
		return (KEYBOARD_HOME);
	    else if ((c == (int)'5') && (kbget() == (int)'~')) /* "\e[5~" ("]" for balance) */
	        return (KEYBOARD_PGUP);
	    else if ((c == (int)'6') && (kbget() == (int)'~')) /* "\e[6~" ("]" for balance) */
	        return (KEYBOARD_PGDN);
	}
	return (KEYBOARD_UNKNOWN);
    }
    else
	return (keymap[(unsigned)c]);
}


int
kbget(VOID)
{
    if (fptty != (FILE*)NULL)
    {
	return (getc(fptty));
    }
    else
	return (EOF);
}


void
kbopen(VOID)
{
    kbinitmap();
    if ((fptty = tfopen("/dev/tty","r")) != (FILE*)NULL)
    {
	set_terminal();
	screen_lines = get_screen_lines();
    }
}


#if defined(HAVE_TERMIO_H)
#include <termio.h>
static struct termio tty_save;			/* SVID2 and XPG2 interface */

static void
reset_terminal(VOID)				/* restore saved modes */
{
    if (tty_init == YES)
	(void)ioctl((int)(fileno(fptty)),(int)TCSETAF,(char*)&tty_save);
}


static void
set_terminal(VOID)				/* set to cbreak input mode */
{
    struct termio tty;			/* SVID2, XPG2 interface */

    if (ioctl((int)(fileno(fptty)),(int)TCGETA,(char*)&tty) != -1)
    {
	tty_save = tty;
	tty_init = YES;
	tty.c_iflag &= ~(INLCR | ICRNL | ISTRIP | IXON | BRKINT);

#if defined(IUCLC)
	tty.c_iflag &= ~IUCLC;		/* absent from POSIX */
#endif /* defined(IUCLC) */

	tty.c_lflag &= ~(ECHO | ICANON);
	tty.c_cc[4] = 5;		/* MIN */
	tty.c_cc[5] = 2;		/* TIME */
	(void)ioctl((int)(fileno(fptty)),(int)TCSETAF,(char*)&tty);
    }
}
#endif /* HAVE_TERMIO_H */


#if defined(HAVE_TERMIOS_H)
#include <termios.h>
static struct termios tty_save;	/* XPG3, POSIX.1, FIPS 151-1 interface */

static void
reset_terminal(VOID)				/* restore saved modes */
{
    if (tty_init == YES)
	(void)tcsetattr((int)(fileno(fptty)),TCSANOW,&tty_save);
}


static void
set_terminal(VOID)				/* set to cbreak input mode */
{
    struct termios tty;		/* XPG3, POSIX.1, FIPS 151-1 interface */

    if (tcgetattr((int)(fileno(fptty)),&tty) != -1)
    {
	tty_save = tty;
	tty_init = YES;
	tty.c_iflag &= ~(INLCR | ICRNL | ISTRIP | IXON | BRKINT);

#if defined(IUCLC)
	tty.c_iflag &= ~IUCLC;		/* absent from POSIX */
#endif /* defined(IUCLC) */

	tty.c_lflag &= ~(ECHO | ICANON);
	tty.c_cc[VMIN] = 5;		/* MIN */
	tty.c_cc[VTIME] = 2;		/* TIME */
	(void)tcsetattr((int)(fileno(fptty)),TCSANOW,&tty);
    }
}
#endif /* defined(HAVE_TERMIOS_H) */

#if defined(HAVE_SGTTY_H)
#include <sgtty.h>

static struct sgttyb tty_save;			/* Berkeley style interface */


static void
reset_terminal(VOID)			/* restored saved terminal modes */
{
    if (tty_init == YES)
	(void)ioctl((int)(fileno(fptty)),(int)TIOCSETP,(char*)&tty_save);
}


static void
set_terminal(VOID)		/* set terminal for cbreak input mode */
{
    struct sgttyb tty;

    /* Try to put file into cbreak mode for character-at-a-time input */
    if (ioctl((int)(fileno(fptty)),(int)TIOCGETP,(char*)&tty) != -1)
    {
	tty_save = tty;
	tty_init = YES;
	tty.sg_flags &= ~(ECHO | LCASE);
	tty.sg_flags |= CBREAK;
	(void)ioctl((int)(fileno(fptty)),(int)TIOCSETP,(char*)&tty);
    }
}
#endif /* defined(HAVE_SGTTY_H) */

int
get_screen_lines(VOID)	/* this must come after terminal header includes! */
{

#if defined(HAVE_ISATTY)
    if ((isatty(fileno(stdin)) == 0) || (isatty(fileno(stdout)) == 0))
	return (0);
#endif

#if defined(TIOCGWINSZ)
    if (fptty != (FILE*)NULL)
    {
	struct winsize window_size;

	(void)ioctl((int)(fileno(fptty)),(int)TIOCGWINSZ,&window_size);

	if (window_size.ws_row > 0)
	    return ((int)window_size.ws_row);
    }
#else /* defined(TIOCGWINSZ) */
    /* some systems store screen lines in environment variables  */
    {
	char *p;
	if (((p = getenv("ROWS")) != (char*)NULL) ||
	    ((p = getenv("LINES")) != (char*)NULL))
	{
	    int n;

	    n = (int)atoi(p);

	    if (n > 0)
		return (n);
	}
    }
#endif /* defined(TIOCGWINSZ) */

    return (SCREEN_LINES);
}


static void
kbinitmap(VOID)
{
    (void)Memset((void*)&keymap[0],(int)KEYBOARD_UNKNOWN,sizeof(keymap));

    keymap[(unsigned)'b']	= KEYBOARD_PGUP;
    keymap[(unsigned)'B']	= KEYBOARD_PGUP;
    keymap[(unsigned)META('V')]	= KEYBOARD_PGUP; /* Emacs scroll-down */

    keymap[(unsigned)'\r']	= KEYBOARD_DOWN; /* the less and more pagers bind */
    keymap[(unsigned)'\n']	= KEYBOARD_DOWN; /* these keys to KEYBOARD_DOWN */
    keymap[(unsigned)'d']	= KEYBOARD_DOWN;
    keymap[(unsigned)'D']	= KEYBOARD_DOWN;
    keymap[(unsigned)CTL('N')]	= KEYBOARD_DOWN; /* Emacs next-line*/

    keymap[(unsigned)'e']	= KEYBOARD_END;
    keymap[(unsigned)'E']	= KEYBOARD_END;
    keymap[(unsigned)META('>')]	= KEYBOARD_END; /* Emacs end-of-buffer */
    keymap[(unsigned)'>']	= KEYBOARD_END;

    keymap[(unsigned)'f']	= KEYBOARD_PGDN;
    keymap[(unsigned)'F']	= KEYBOARD_PGDN;
    keymap[(unsigned)' ']	= KEYBOARD_PGDN;
    keymap[(unsigned)CTL('V')]	= KEYBOARD_PGDN; /* Emacs scroll-up */

    keymap[(unsigned)'h']	= KEYBOARD_HELP;
    keymap[(unsigned)'H']	= KEYBOARD_HELP;
    keymap[(unsigned)'?']	= KEYBOARD_HELP;
    keymap[(unsigned)CH_BACKSPACE] = KEYBOARD_HELP; /* Emacs help */

    keymap[(unsigned)CH_ESCAPE]	= KEYBOARD_QUIT; /* ESCape gets out */
    keymap[(unsigned)'q']	= KEYBOARD_QUIT;
    keymap[(unsigned)'Q']	= KEYBOARD_QUIT;

    keymap[(unsigned)'.']	= KEYBOARD_AGAIN;
    keymap[(unsigned)'r']	= KEYBOARD_AGAIN;
    keymap[(unsigned)'R']	= KEYBOARD_AGAIN;
    keymap[(unsigned)CTL('L')]	= KEYBOARD_AGAIN; /* Emacs recenter */

    keymap[(unsigned)'t']	= KEYBOARD_HOME;
    keymap[(unsigned)'T']	= KEYBOARD_HOME;
    keymap[(unsigned)META('<')]	= KEYBOARD_HOME; /* Emacs beginning-of-buffer */
    keymap[(unsigned)'<']	= KEYBOARD_HOME;

    keymap[(unsigned)CTL('R')]	= KEYBOARD_SEARCH_BACKWARD; /* Emacs */
    keymap[(unsigned)'\\']	= KEYBOARD_SEARCH_BACKWARD;

    keymap[(unsigned)CTL('S')]	= KEYBOARD_SEARCH_FORWARD; /* Emacs */
    keymap[(unsigned)'/']	= KEYBOARD_SEARCH_FORWARD;

    keymap[(unsigned)'u']	= KEYBOARD_UP;
    keymap[(unsigned)'U']	= KEYBOARD_UP;
    keymap[(unsigned)CTL('P')]	= KEYBOARD_UP; 	/* Emacs previous-line */

    keymap[(unsigned)'[']	= KEYBOARD_UP_PARAGRAPH;
    keymap[(unsigned)']']	= KEYBOARD_DOWN_PARAGRAPH;

    keymap[(unsigned)'{']	= KEYBOARD_UP_PARAGRAPH;
    keymap[(unsigned)'}']	= KEYBOARD_DOWN_PARAGRAPH;
}

#endif /* (SCREEN_LINES > 0) */
