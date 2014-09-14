/***********************************************************************
 @C-file{
    author              = "Nelson H. F. Beebe",
    version             = "2.16",
    date                = "01 January 2013",
    time                = "09:00:49 MST",
    filename            = "isbn.c",
    address             = "University of Utah
			   Department of Mathematics, 110 LCB
			   155 S 1400 E RM 233
			   Salt Lake City, UT 84112-0090
			   USA",
    telephone           = "+1 801 581 5254",
    FAX                 = "+1 801 581 4148",
    URL                 = "http://www.math.utah.edu/~beebe",
    checksum            = "63115 797 2700 21975",
    email               = "beebe@math.utah.edu, beebe@acm.org,
			   beebe@computer.org (Internet)",
    codetable           = "ISO/ASCII",
    keywords            = "bibliography, ISBN, hyphenation",
    license             = "GNU General Public License, version 2 or
                           later",
    supported           = "yes",
    docstring           = "This file contains code for hyphenating
			   International Standard Book Numbers (ISBNs),
			   using the function ISBN_hyphenate(s,t,maxs).
			   No other public objects are defined by this
			   file.

			   If this file is compiled with the
			   preprocessor symbol TEST defined, then a
			   standalone program is produced that can be
			   used to filter test data containing ISBN
			   key/value pairs extracted from BibTeX files.
			   For example, the UNIX commands

			   bibclean -no-warn -max-width 0 *.bib | \
				grep '^ *ISBN *=' >tmpfile
			   sed -e 's/-//g' tmpfile | ./isbn | diff tmpfile -

			   should display no differences in ISBN
			   numbers, except where their hyphenation was
			   originally incorrect, or missing.

			   The checksum field above contains a CRC-16
			   checksum as the first value, followed by the
			   equivalent of the standard UNIX wc (word
			   count) utility output of lines, words, and
			   characters.  This is produced by Robert
			   Solovay's checksum utility.",
 }
***********************************************************************/

#include <config.h>
#include "xstdbool.h"
#include "xstdlib.h"
#include "xstring.h"
#include "xctype.h"

#include "ch.h"
#include "isbn.h"
#include "yesorno.h"


static const char *ISBN_file = (const char*)NULL;

typedef struct
{
    const char *begin;
    const char *end;
    const char *countries;
}
ISBN_range_t;


#define TOKEN_SEPARATORS	" \t"

#if !defined(MAX_ISBN_RANGE)
#define MAX_ISBN_RANGE	2560		/* about 8 times the default size */
#endif

#include "isbn.tbl"	/* generated from awk -f isbn-el-to-bibclean-isbn.awk isbn.el */

extern FILE		*stdlog;

/*@null@*/ extern char	*findfile ARGS((/*@null@*/ const char *pathlist_, /*@null@*/ const char *name_));
extern char		*get_line ARGS((FILE *fp_));
extern char		*Strdup ARGS((const char *s_));
extern FILE		*tfopen ARGS((const char *filename_, const char *mode_));
extern void		warning ARGS((const char *msg_));

static void		add_ISBN_range ARGS((const char *the_begin, const char *the_end, const char *the_countries));
static void		add_one_ISBN_range ARGS((const char *the_begin, const char *the_end, const char *the_countries, size_t where));
static YESorNO		is_valid_ISBN_prefix ARGS((const char *prefix));
static const char	*fix_ISBN ARGS((const char *ISBN_));
static const char	*hyphenate_one_ISBN ARGS((const char *prefix_, const char *ISBN_));
static const char	*hyphenate_one_ISBN_13 ARGS((const char *prefix_, const char *ISBN_13_));
static int		in_ISBN_range ARGS((const char *begin_, const char *ISBN_, const char *end_));
static YESorNO		ISBN_match_country_language ARGS((const char *p1, const char *p2));
static const char 	*next_ISBN ARGS((const char *s_, const char **end_));
static const char 	*next_ISBN_13 ARGS((const char *s_, const char **end_));
static void	        squeeze_ISBN ARGS((char * out_ISBN_, const char *in_ISBN_));


#define skip_non_ISBN_digit(p)    while ((*p != '\0') && !isISBNdigit((int)*p)) p++

#define isISBNdigit(c)	(Isdigit((int)(c)) || ((int)(c) == (int)'X') || ((int)(c) == (int)'x'))


/*
 * Search the ISBN_range[] table circularly from the last search
 * position for the next non-empty slot matching the_begin, and
 * install the new triple (the_begin,the_end,the_countries) there.
 * Otherwise, add the triple at the end, if enough space remains.
 */
static void
add_ISBN_range(const char *the_begin, const char *the_end, const char *the_countries)
{
    static int error_count = 0;
    size_t k;
    static size_t start = (size_t) 0;

    /* Silently ignore invalid begin/end pairs */
    if (the_begin == (const char *)NULL)
	return;
    else if (the_end == (const char *)NULL)
	return;

    if (the_begin[0] == '-')
	start = 0;	/* because deletions must always find the first match */

    for (k = start;
	 (k < MAX_ISBN_RANGE) && (ISBN_range[k].begin != (const char *)NULL);
	 ++k)
    {
	if (ISBN_range[k].begin[0] == '-')
	{
	    if (STREQUAL(ISBN_range[k].begin,the_begin))
	    {				/* then already deleted this one */
		start = k;
		return;
	    }
	    else
	        continue;		/* ignore `deleted' entries */
	}
	else if ((the_begin[0] == '-') && STREQUAL(ISBN_range[k].begin, the_begin + 1))
	{	/* then `delete' this entry by changing its begin prefix to start with a hyphen */
	    ISBN_range[k].begin = Strdup(the_begin);
	    start = k;
	    return;
	}
	else if (STREQUAL(ISBN_range[k].begin, the_begin))
	{
	    add_one_ISBN_range(the_begin, the_end, the_countries, k);
	    start = k;
	    return;
	}
    }

    /* If we fell through, then restart the search in the beginning of the table */
    for (k = 0;
	 (k < start) && (ISBN_range[k].begin != (const char *)NULL); ++k)
    {
	if (ISBN_range[k].begin[0] == '-')
	{
	    if (STREQUAL(ISBN_range[k].begin,the_begin))
	    {				/* then already deleted this one */
		start = k;
		return;
	    }
	    else
	        continue;		/* ignore `deleted' entries */
	}
	else if ((the_begin[0] == '-') && STREQUAL(ISBN_range[k].begin, the_begin + 1))
	{	/* then `delete' this entry by changing its begin prefix to start with a hyphen */
	    ISBN_range[k].begin = Strdup(the_begin);
	    start = k;
	    return;
	}
	else if (STREQUAL(ISBN_range[k].begin, the_begin))
	{
	    add_one_ISBN_range(the_begin, the_end, the_countries, k);
	    start = k;
	    return;
	}
    }

    /* If we fell through, then add the new entry at the first deleted
       entry, or after the last used entry */
    for (k = 0;
	 ((k < MAX_ISBN_RANGE) &&
	  (ISBN_range[k].begin != (const char *)NULL) &&
	  (ISBN_range[k].begin[0] != '\0'));
	 ++k)
	continue;

    if (k < (MAX_ISBN_RANGE - 1))	/* then have space to store this new entry */
    {
	start = k;
	add_one_ISBN_range(the_begin, the_end, the_countries, k);
    }
    else if (++error_count == 1)	/* no more than one error message */
	fprintf(stdlog,
		      "More than %lu ISBN ranges fills internal table\n",
		      (unsigned long)MAX_ISBN_RANGE);
}


static void
add_one_ISBN_range(const char *the_begin, const char *the_end,
		   const char *the_countries, size_t where)
{	/* add an entry at slot where, without bounds checking, but with
	   valid-value checking */

    if (
	(the_begin != (const char*)NULL) && (is_valid_ISBN_prefix(the_begin) == NO) ||
        (the_end != (const char*)NULL) && (is_valid_ISBN_prefix(the_end) == NO)
    )
    {
	fprintf(stdlog, "Invalid country/language-publisher ISBN prefix [%s] in ISBN file [%s]\n", the_begin, ISBN_file);
	return;
    }
    else if ((the_begin != (const char*)NULL) && (the_end != (const char*)NULL) && STRGREATER(the_begin,the_end))
    {
	fprintf(stdlog,
		      "Non-increasing country/language-publisher ISBN range [%s .. %s] in ISBN file [%s]\n",
		      the_begin, the_end, ISBN_file);
	return;
    }

    ISBN_range[where].begin = (the_begin == (const char *)NULL) ? the_begin : Strdup(the_begin);
    ISBN_range[where].end = (the_end == (const char *)NULL) ? the_end : Strdup(the_end);
    ISBN_range[where].countries = (the_countries == (const char *)NULL) ? "" : Strdup(the_countries);
}


void
do_ISBN_file(/*@null@*/ const char *pathlist, /*@null@*/ const char *name)
{
    FILE *fp;
    char *p;

    if (name == (const char*)NULL)
	return;

    if ((ISBN_file = findfile(pathlist,name)) == (char*)NULL)
	return;				/* silently ignore missing files */

    if ((fp = tfopen(ISBN_file,"r")) == (FILE*)NULL)
	return;				/* silently ignore missing files */

    /* The ISBN file is expected to look like the output of
       -print-ISBN-table: lines are (1) blank or empty, (2) comments
       from percent to end-of-line, (3) pairs of whitespace-separated
       (begin-prefix, end-prefix) values, or (4) triples of
       whitespace-separated (begin-prefix, end-prefix values, countries).
       In the latter case, the countries continue to end-of-line or a
       comment character, whichever comes first, and may include
       blanks. */
    while ((p = get_line(fp)) != (char*)NULL)
    {
	const char *the_begin;
	const char *the_end;
	const char *the_countries;
	char *comment;

	comment = strchr(p, BIBTEX_COMMENT_PREFIX);
	if (comment != (const char*)NULL)
	    *comment = '\0';		/* then discard comment text */

	the_begin = strtok(p, TOKEN_SEPARATORS);
	if (the_begin == (const char*)NULL)
	    continue;			/* ignore blank or empty lines */
	if (*the_begin == (char)BIBTEX_COMMENT_PREFIX)
	    continue;			/* ignore comment lines */
	the_end = strtok((char*)NULL, TOKEN_SEPARATORS);
	if (the_end == (const char*)NULL)
	{
	    fprintf(stdlog,"Expected end-prefix after begin-prefix [%s] in ISBN file [%s]\n",
			  the_begin, ISBN_file);
	    continue;
	}
	the_countries = strtok((char*)NULL, "");
	if (the_countries != (const char*)NULL)
	{				/* skip over leading space */
	    while (Isspace((int)*the_countries))
		++the_countries;
	}
	if ((the_countries != (const char*)NULL) && (*the_countries == '\0'))
	    the_countries = (const char*)NULL;
#if defined(DEBUG)
	fprintf(stdlog,
		      "DEBUG:\t[%s]\t[%s]\t[%s]\t[%s]\n",
		      ISBN_file,
		      the_begin,
		      the_end,
		      ((the_countries == (const char*)NULL) ? "" : the_countries));
#endif
	add_ISBN_range(the_begin, the_end, the_countries);
    }
    (void)fclose(fp);
}


void
do_print_ISBN_table(VOID)
{
    size_t k;

    /* For brevity and readability, we output the country/language
       group prefix only when it changes, preceded by pair of newlines. */

    fprintf(stdlog, "%%%%%% ISBN ranges and country/language groups\n");
    for (k = 0; (ISBN_range[k].begin != (const char *)NULL); ++k)
    {
	const char *country_names;

	if (k == 0)
	{
	    if (ISBN_range[k].countries[0] == '\0')
		country_names = (const char *)NULL;
	    else
		country_names = ISBN_range[k].countries;
	}
	else if (STREQUAL(ISBN_range[k-1].countries,ISBN_range[k].countries) &&
		 (ISBN_match_country_language(ISBN_range[k-1].begin,ISBN_range[k].begin) == YES))
	    country_names = (const char *)NULL;
	else if (ISBN_range[k].countries[0] == '\0')
	    country_names = (const char *)NULL;
	else
	    country_names = ISBN_range[k].countries;

	/* We intentionally include `deleted' entries (beginning with a hyphen), so
	   as not to conceal information from the user. */
	fprintf(stdlog, "%s%-11s\t%-11s%s%s\n",
		      ((country_names == (const char *)NULL) ? "" : "\n\n"),
		      ISBN_range[k].begin,
		      ISBN_range[k].end,
		      ((country_names == (const char *)NULL) ? "" : "\t"),
		      ((country_names == (const char *)NULL) ? "" : country_names));
    }
}

static const char *
fix_ISBN(const char *ISBN)
{
    size_t k;

    for (k = 0; (ISBN_range[k].begin != (const char*)NULL); ++k)
    {
	if (ISBN_range[k].begin[0] == '-')
	    continue;			/* ignored `deleted' entries */
	if (in_ISBN_range(ISBN_range[k].begin, ISBN, ISBN_range[k].end) == 0)
	    return (hyphenate_one_ISBN(ISBN_range[k].begin, ISBN));
    }
    return ((const char*)NULL);
}


static const char *
fix_ISBN_13(const char *ISBN_13)
{
    size_t k;

    for (k = 0; (ISBN_range[k].begin != (const char*)NULL); ++k)
    {
	if (ISBN_range[k].begin[0] == '-')
	    continue;			/* ignored `deleted' entries */
	if (in_ISBN_range(ISBN_range[k].begin, &ISBN_13[3], ISBN_range[k].end)  == 0)
        {
	    return (hyphenate_one_ISBN_13(ISBN_range[k].begin, ISBN_13));
        }
    }
    return ((const char*)NULL);
}


/**
 * Given a countrygroupnumber-publishernumber prefix, and an ISBN
 * optionally containing spaces and hyphens, return a pointer to an
 * unmodifiable properly-hyphenated ISBN stored in an internal buffer
 * that is overwritten on subsequent calls, or NULL if the correct
 * number of ISBN digits is not found.
 *
 * The input ISBN can contain optional leading and trailing text,
 * such as a line from a BibTeX .bib file, like this:
 * ISBN =         "0-387-09823-2 (paperback)",
 */
static const char *
hyphenate_one_ISBN(const char *prefix, const char *ISBN)
{
    static char new_ISBN[MAX_ISBN];
    int k;

    skip_non_ISBN_digit(ISBN);

    for (k = 0; (*ISBN != '\0') && (k < (MAX_ISBN - 2)); )
    {
	if (*prefix == '-')
	{
	    new_ISBN[k++] = '-';
	    prefix++;
	}
	else if (*prefix != '\0')
	{
	    skip_non_ISBN_digit(ISBN);
	    if (*ISBN == '\0')
		break;
	    new_ISBN[k++] = *ISBN++;
	    prefix++;
	    if ((*prefix == '\0') && (k < MAX_ISBN))
		new_ISBN[k++] = '-';
	}
	else			/* past prefix */
	{
	    skip_non_ISBN_digit(ISBN);
	    if (*ISBN == '\0')
		break;
	    new_ISBN[k++] = *ISBN++;
	}
    }
    if ((k == (MAX_ISBN - 2)) && !isISBNdigit(*ISBN))
    {
	new_ISBN[(MAX_ISBN - 2)] = new_ISBN[(MAX_ISBN - 3)];
				/* move checksum digit to end */
	new_ISBN[(MAX_ISBN - 3)] = '-';	/* prefix it with a hyphen */
	new_ISBN[(MAX_ISBN - 1)] = '\0'; /* terminate the string */
	return ((const char*)&new_ISBN[0]);
    }
    else
	return ((const char*)NULL);
}


/**
 * Given an ISBN-10 countrygroupnumber-publishernumber prefix, and
 * an ISBN-13 optionally containing spaces and hyphens, return a
 * pointer to an unmodifiable properly-hyphenated ISBN-13 stored in
 * an internal buffer that is overwritten on subsequent calls, or
 * NULL if the correct number of ISBN-13 digits is not found.
 * 
 * The input ISBN-13 can contain optional leading and trailing text,
 * such as a line from a BibTeX .bib file, like this:
 *
 * 	SBN-13 =      "978-0-387-09823-4 (paperback)",
 *
 */
static const char *
hyphenate_one_ISBN_13(const char *prefix, const char *ISBN_13)
{
    static char new_ISBN_13[MAX_ISBN_13];
    int k;


    skip_non_ISBN_digit(ISBN_13);

    for (k = 0; (*ISBN_13 != '\0') && (k < (MAX_ISBN_13 - 1)); )
    {
	if (k == 0)
	{
	    if ((strncmp("978", ISBN_13, 3) == 0) || (strncmp("979", ISBN_13, 3) == 0) )
	    {
		new_ISBN_13[k++] = *ISBN_13++;
		new_ISBN_13[k++] = *ISBN_13++;
		new_ISBN_13[k++] = *ISBN_13++;
		new_ISBN_13[k++] = '-';
	    }
	    else
            {
		warning("ISBN-13 must begin with either 978 or 979: ``%v''");
            }
	}

	if (*prefix == '-')
	{
	    new_ISBN_13[k++] = '-';
	    prefix++;
	}
	else if (*prefix != '\0')
	{
	    skip_non_ISBN_digit(ISBN_13);
	    if (*ISBN_13 == '\0')
		break;
	    new_ISBN_13[k++] = *ISBN_13++;
	    prefix++;
	    if ((*prefix == '\0') && (k < MAX_ISBN_13))
		new_ISBN_13[k++] = '-';
	}
	else			/* past prefix */
	{
	    skip_non_ISBN_digit(ISBN_13);
	    if (*ISBN_13 == '\0')
		break;
	    new_ISBN_13[k++] = *ISBN_13++;
	}
    }

    if ((k == (MAX_ISBN_13 - 2)) && !isISBNdigit(*ISBN_13))
    {
	new_ISBN_13[(MAX_ISBN_13 - 2)] = new_ISBN_13[(MAX_ISBN_13 - 3)]; /* move checksum digit to end */
	new_ISBN_13[(MAX_ISBN_13 - 3)] = '-';	/* prefix it with a hyphen */
	new_ISBN_13[(MAX_ISBN_13 - 1)] = '\0'; /* terminate the string */
	return ((const char*)&new_ISBN_13[0]);
    }
    else
	return ((const char*)NULL);
}


/**
 * Compare the countrygroupnumber-publishernumber part of ISBN
 * against the range (begin, end), and return -1 (less than),
 * 0 (in range), or +1 (greater than).
 */
static int
in_ISBN_range(const char *begin, const char *ISBN, const char *end)
{
    char begin_prefix[MAX_ISBN];
    char end_prefix[MAX_ISBN];
    char ISBN_prefix[MAX_ISBN];

    squeeze_ISBN(begin_prefix, begin);
    squeeze_ISBN(ISBN_prefix, ISBN);

    if (strncmp(ISBN_prefix, begin_prefix, strlen(begin_prefix)) < 0)
	return (-1);

    squeeze_ISBN(end_prefix, end);
    if (strncmp(end_prefix, ISBN_prefix, strlen(end_prefix)) < 0)
	return (1);

    return (0);
}


static YESorNO
is_valid_ISBN_prefix(const char *prefix)
{
    /* Return YES if prefix matches "^[0-9]+-[0-9]+$" and has a length
       < 10, and else, NO */
    int n;
    int len;

    for (len = 0, n = 0; Isdigit((int)*prefix); ++prefix)
	(len++, n++);
    if (n == 0)
	return (NO);

    if (*prefix != '-')
	return (NO);
    prefix++;
    len++;

    for (n = 0; Isdigit((int)*prefix); ++prefix)
	(len++, n++);
    if (n == 0)
	return (NO);

    if (*prefix != '\0')
	return (NO);
    if (len >= 10)			/* longest possible is 9999999-9[-9-9] */
	return (NO);

    return (YES);
}


/**
 * Given a string s[] containing one or more ISBNs, rewrite the
 * string in-place with correct ISBN hyphenation.  Up to maxs-1
 * non-NUL characters of s[] may be used. t[] is workspace, at
 * least as large as s[].   If insufficient workspace is
 * available, s[] is returned unchanged.
 */
void
ISBN_hyphenate(/*@out@*/ char *s, /*@out@*/ char *t, size_t maxs)
{
    const char *p;
    const char *r;
    const char *next;
    const char *start;

    t[0] = '\0';

    for (p = start = s; (p = next_ISBN(p, &next), p) != (const char*)NULL; start = p)
    {
	if ((strlen(t) + (size_t)(p-start)) >= maxs)
	    return;		/* insufficient space: premature return */

	strncat(t, start, (size_t)(p - start));
	r = fix_ISBN(p);
	if (r != (char*)NULL)
	{
	    if ((strlen(t) + strlen(r)) >= maxs)
		return;		/* insufficient space: premature return */
	    strcat(t, r);
	    p = next;
	}
	else
	{
	    if ((strlen(t) + 1) >= maxs)
		return;		/* insufficient space: premature return */
	    strncat(t, p, 1);
	    ++p;
	}
    }
    if ((strlen(t) + strlen(start)) >= maxs)
	return;		/* insufficient space: premature return */
    strcat(t, start);
    strcpy(s, t);
}

/**
 * Given a string s[] containing one or more ISBN_13s, rewrite the
 * string in-place with correct ISBN_13 hyphenation.  Up to maxs-1
 * non-NUL characters of s[] may be used. t[] is workspace, at
 * least as large as s[].   If insufficient workspace is
 * available, s[] is returned unchanged.
 */
void
ISBN_13_hyphenate(/*@out@*/ char *s, /*@out@*/ char *t, size_t maxs)
{
    const char *p;
    const char *r;
    const char *next;
    const char *start;

   t[0] = '\0';

    for (p = start = s; (p = next_ISBN_13(p,&next), p) != (const char*)NULL;
	 start = p)
    {
	if ((strlen(t) + (size_t)(p-start)) >= maxs)
	    return;		/* insufficient space: premature return */
	(void)strncat(t,start,(size_t)(p-start));
	r = fix_ISBN_13(p);
	if (r != (char*)NULL)
	{
	    if ((strlen(t) + strlen(r)) >= maxs)
		return;		/* insufficient space: premature return */
	    (void)strcat(t,r);
	    p = next;
	}
	else
	{
	    if ((strlen(t) + 1) >= maxs)
		return;		/* insufficient space: premature return */
	    (void)strncat(t,p,1);
	    ++p;
	}
    }
    if ((strlen(t) + strlen(start)) >= maxs)
	return;		/* insufficient space: premature return */
    (void)strcat(t,start);
    (void)strcpy(s,t);
}


/**
 * Check the consistency of the ISBN_range[] table, and then
 * modify its compile-time setting so that all entries are
 * guaranteed to have non-NULL countries.  We need to ensure this,
 * because an "-ISBN-file filename" option can `delete' table
 * entries (by resetting the begin string to start with a hyphen).
 */
void
ISBN_initialize(VOID)
{
    size_t k;

   for (k = 0; (ISBN_range[k].begin != (const char *)NULL); ++k)
    {
	if (ISBN_range[k].end == (const char*)NULL)
	{
	    fprintf(stdlog,
			  "Illegal ISBN range end [%s .. NULL]\n",
			  ISBN_range[k].begin);
	    ISBN_range[k].end = "";
	}

	if (is_valid_ISBN_prefix(ISBN_range[k].begin) == NO)
	{
	      fprintf(stdlog, "Invalid country/language-publisher ISBN prefix [%s]\n", ISBN_range[k].begin);
	      ISBN_range[k].begin = "";
	}

	if (is_valid_ISBN_prefix(ISBN_range[k].end) == NO)
	{
	      fprintf(stdlog, "Invalid country/language-publisher ISBN prefix [%s]\n", ISBN_range[k].end);
	      ISBN_range[k].end = "";
	}

	if (STRGREATER(ISBN_range[k].begin, ISBN_range[k].end))
	{
	    fprintf(stdlog,
			  "Non-increasing country/language-publisher ISBN range [%s .. %s] deleted\n",
			  ISBN_range[k].begin, ISBN_range[k].end);
	    ISBN_range[k].begin = ISBN_range[k].end = "";
	}

	if (ISBN_range[k].countries == (const char *)NULL)
	{
	    if ((k == 0) || ISBN_match_country_language(ISBN_range[k-1].begin,ISBN_range[k].begin) == NO)
	    {
		fprintf(stdlog,
			      "Missing country names for ISBN range [%s .. %s]\n",
			      ISBN_range[k].begin, ISBN_range[k].end);
		ISBN_range[k].countries = "";
	    }
	    else if (ISBN_match_country_language(ISBN_range[k-1].begin,ISBN_range[k].begin) == YES)
		ISBN_range[k].countries = ISBN_range[k - 1].countries;
	    else
		ISBN_range[k].countries = "";
	}
    }
}



/**
 * Return YES if the country/language prefixes of p1 and p2 match, else NO
 */
static YESorNO
ISBN_match_country_language(const char *p1, const char *p2)
{
    size_t k;

    if ((p1 == (const char *)NULL) || (p2 == (const char *)NULL))
	return (NO);

    for (k = 0; (p1[k] != '\0') && (p2[k] != '\0'); ++k)
    {
	if (p1[k] != p2[k])
	    return (NO);
	else if (p1[k] == '-')
	    return (YES);
    }

    return (NO);
}


static const char *
next_ISBN(const char *s,const char **next)
{
    size_t n;
    const char *start;

    while (*s != '\0')			/* scan over s[] */
    {
	for ( ; (*s != '\0') && !isISBNdigit(*s); ++s) /* ignore non-ISBN digits */
	    continue;

	for (n = 0, start = s; (*s != '\0'); ++s)	/* scan over ISBN */
	{
	    if (isISBNdigit(*s))
	    {
		n++;
		if (n == 10)		/* then we found an ISBN */
		{
		    *next = s + 1;
		    return (start);
		}
	    }
	    else if ((*s == ' ') || (*s == '-'))
		/* NO-OP */;
	    else
		break;
	}
    }
    *next = (const char*)NULL;
    return ((const char*)NULL);		/* no ISBN recognized */
}

static const char *
next_ISBN_13(const char *s,const char **next)
{
    size_t n;
    const char *start;

    while (*s != '\0')			/* scan over s[] */
    {
	for ( ; (*s != '\0') && !isISBNdigit(*s); ++s) /* ignore non-ISBN_13 digits */
	    continue;

	for (n = 0, start = s; (*s != '\0'); ++s)	/* scan over ISBN_13 */
	{
	    if (isISBNdigit(*s))
	    {
		n++;
		if (n == 13)		/* then we found an ISBN_13 */
		{
		    *next = s + 1;
		    return (start);
		}
	    }
	    else if ((*s == ' ') || (*s == '-'))
		/* NO-OP */;
	    else
		break;
	}
    }
    *next = (const char*)NULL;
    return ((const char*)NULL);		/* no ISBN_13 recognized */
}


/**
 * Copy in_ISBN to out_ISBN, eliminating non-ISBN characters.
 */
static void
squeeze_ISBN(char * out_ISBN, const char *in_ISBN)
{
    char *limit = out_ISBN + MAX_ISBN;

    while (out_ISBN < limit)
    {
	skip_non_ISBN_digit(in_ISBN);
	*out_ISBN = *in_ISBN;
	if (*in_ISBN == '\0')
	    break;
	in_ISBN++;
	out_ISBN++;
    }
}

#if defined(TEST)
#define MAX_BUF		4096

int			main ARGS((int argc_, char* argv_[]));

int
main(int argc, char* argv[])
{
    char buf[MAX_BUF];
    char buf2[MAX_BUF];

    while (fgets(buf,MAX_BUF,stdin) != (char*)NULL)
    {
	ISBN_hyphenate(buf,buf2,MAX_BUF);
	(void)fputs(buf,stdout);
	(void)fflush(stdout);
    }

    exit (EXIT_SUCCESS);
    return (EXIT_SUCCESS);
}
#endif /* defined(TEST) */

