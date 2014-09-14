#ifndef ISBN_H_DEFINED_
#define ISBN_H_DEFINED_

#define MAX_ISBN	11 /* array size for complete ISBN and terminal NUL */
#define MAX_ISBN_RAW	14 /* array size for complete ISBN with - between groups, and terminal NUL */
#define MAX_ISBN_13	14 /* array size for complete ISBN-13 and terminal NUL */
#define MAX_ISBN_13_RAW	18 /* array size for complete ISBN-13 with - between groups, and terminal NUL */


#define skip_non_ISBN_digit(p)    while ((*p != '\0') && !isISBNdigit((int)*p)) p++

#define isISBNdigit(c)  (Isdigit((int)(c)) || ((int)(c) == (int)'X') || ((int)(c) == (int)'x'))

#define ISBN_DIGIT_VALUE(c)     ((((int)(c) == (int)'X') || ((int)(c) == (int)'x')) ? 10 : ((int)(c) - (int)'0'))

extern void		do_ISBN_file ARGS((/*@null@*/ const char *pathlist_, /*@null@*/ const char *name_));
extern void		do_print_ISBN_table ARGS((void));
extern void		ISBN_initialize ARGS((void));

void                    ISBN_hyphenate ARGS((/*@out@*/ char *s_,/*@out@*/ char *t_,size_t maxs_));
void                    ISBN_initialize ARGS((void));


#endif /* ISBN_H_DEFINED_ */
