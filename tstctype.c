/***********************************************************************
Test the <ctype.h> functions for characters in 0..255.
[23-Aug-2003]
***********************************************************************/

#include <stdio.h>
#include <ctype.h>

void
test_isxxx(const char *name, int (*fcn)(int))
{
    int k;

    (void)printf("\n");

    (void)printf("Test of %s: ", name);
    for (k = 0; k <= 255; ++k)
    {
	if ((*fcn)(k))
	    (void)printf(" %d", k);
    }
    (void)printf("\n");

    (void)printf("Test of %s: ", name);
    for (k = 0; k <= 255; ++k)
    {
	if ((*fcn)(k))
	    (void)printf("%c", k);
    }
    (void)printf("\n");
}

void
test_to(const char *name, int (*fcn)(int))
{
    int k;

    (void)printf("\n");

    (void)printf("Test of %s: ", name);
    for (k = 0; k <= 255; ++k)
    {
	if ((*fcn)(k) != k)
	    (void)printf(" %d->%d", k, (*fcn)(k));
    }
    (void)printf("\n");

    (void)printf("Test of %s: ", name);
    for (k = 0; k <= 255; ++k)
    {
	if ((*fcn)(k) != k)
	    (void)printf(" %c->%c", k, (*fcn)(k));
    }
    (void)printf("\n");
}

int
main(int argc, char* argv[])
{

    test_isxxx("isalnum", isalnum);
    test_isxxx("isalpha", isalpha);
    test_isxxx("iscntrl", iscntrl);
    test_isxxx("isdigit", isdigit);
    test_isxxx("isgraph", isgraph);
    test_isxxx("islower", islower);
    test_isxxx("isprint", isprint);
    test_isxxx("ispunct", ispunct);
    test_isxxx("isspace", isspace);
    test_isxxx("isupper", isupper);
    test_isxxx("isxdigit", isxdigit);
    test_to("tolower", tolower);
    test_to("toupper", toupper);

    return (0);
}
