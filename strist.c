/* -*-C-*- strist.c */
/*-->strist*/
/**********************************************************************/
/****************************** strist ********************************/
/**********************************************************************/

#include <config.h>
#include "xctype.h"
#include "xstring.h"

/**
 * Return pointer to location of sub[] in s[], ignoring letter case,
 * else (char*)NULL.  This is a simple implementation; a library version
 * should use a more sophisticated version (e.g. Boyer-Moore,
 * Knuth-Morris-Pratt, hardware search).
 */

char* stristr(const char  *s, const char  *sub)
{
    size_t length;

    length = (size_t)strlen(sub);
    if (s == (const char*)NULL)
	return ((char*)NULL);
    if ((sub == (char*)NULL) || (*sub == '\0'))
	return ((char*)s); /* NULL substring always found at start */
    for (; (*s != '\0');)
    {
	if (strnicmp(s,sub,length) == 0)
	    return ((char*)s);
	++s;
    }
    return ((char*)NULL);
}
