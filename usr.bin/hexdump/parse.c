/*	$OpenBSD: parse.c,v 1.8 2001/11/19 19:02:14 mpech Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
/*static char sccsid[] = "from: @(#)parse.c	5.6 (Berkeley) 3/9/91";*/
static char rcsid[] = "$OpenBSD: parse.c,v 1.8 2001/11/19 19:02:14 mpech Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <err.h>
#include "hexdump.h"

void addfile __P((char *));
void add __P((char *));
int size __P((FS *));
void rewrite __P((FS *));
void escape __P((char *));

FU *endfu;					/* format at end-of-data */

void
addfile(name)
	char *name;
{
	char *p;
	FILE *fp;
	size_t len;

	if (!(fp = fopen(name, "r")))
		err(1, "%s", name);
	while ((p = fgetln(fp, &len))) {
		if (*(p + len - 1) == '\n')
			*(p + len - 1) = '\0';
		else {
			warnx("incomplete line");
			continue;
		}
		for (; *p && isspace(*p); ++p);
		if (!*p || *p == '#')
			continue;
		add(p);
	}
	(void)fclose(fp);
}

void
add(fmt)
	char *fmt;
{
	char *p;
	static FS **nextfs;
	FS *tfs;
	FU *tfu, **nextfu;
	char *savep, *emalloc();

	/* start new linked list of format units */
	/* NOSTRICT */
	tfs = (FS *)emalloc(sizeof(FS));
	if (!fshead)
		fshead = tfs;
	else
		*nextfs = tfs;
	nextfs = &tfs->nextfs;
	nextfu = &tfs->nextfu;

	/* take the format string and break it up into format units */
	for (p = fmt;;) {
		/* skip leading white space */
		for (; isspace(*p); ++p);
		if (!*p)
			break;

		/* allocate a new format unit and link it in */
		/* NOSTRICT */
		tfu = (FU *)emalloc(sizeof(FU));
		*nextfu = tfu;
		nextfu = &tfu->nextfu;
		tfu->reps = 1;

		/* if leading digit, repetition count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p);
			if (!isspace(*p) && *p != '/')
				errx(1, "bad format {%s}", fmt);
			/* may overwrite either white space or slash */
			tfu->reps = atoi(savep);
			tfu->flags = F_SETREP;
			/* skip trailing white space */
			for (++p; isspace(*p); ++p);
		}

		/* skip slash and trailing white space */
		if (*p == '/')
			while (isspace(*++p));

		/* byte count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p);
			if (!isspace(*p))
				errx(1, "bad format {%s}", fmt);
			tfu->bcnt = atoi(savep);
			/* skip trailing white space */
			for (++p; isspace(*p); ++p);
		}

		/* format */
		if (*p != '"')
			errx(1, "bad format {%s}", fmt);
		for (savep = ++p; *p != '"';)
			if (*p++ == 0)
				errx(1, "bad format {%s}", fmt);
		if (!(tfu->fmt = malloc(p - savep + 1)))
			err(1, "malloc");
		(void) strncpy(tfu->fmt, savep, p - savep);
		tfu->fmt[p - savep] = '\0';
		escape(tfu->fmt);
		p++;
	}
	/* no single fu in fmt */
	if (tfs->nextfu == NULL)
		errx(1, "bad format {%s}", fmt);
}

static const char *spec = ".#-+ 0123456789";
int
size(fs)
	FS *fs;
{
	FU *fu;
	int bcnt, cursize;
	char *fmt;
	int prec;

	/* figure out the data block size needed for each format unit */
	for (cursize = 0, fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (fu->bcnt) {
			cursize += fu->bcnt * fu->reps;
			continue;
		}
		for (bcnt = prec = 0, fmt = fu->fmt; *fmt; ++fmt) {
			if (*fmt != '%')
				continue;
			/*
			 * skip any special chars -- save precision in
			 * case it's a %s format.
			 */
			while (strchr(spec + 1, *++fmt));
			if (*fmt == '.' && isdigit(*++fmt)) {
				prec = atoi(fmt);
				while (isdigit(*++fmt));
			}
			switch(*fmt) {
			case 'c':
				bcnt += 1;
				break;
			case 'd': case 'i': case 'o': case 'u':
			case 'x': case 'X':
				bcnt += 4;
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				bcnt += 8;
				break;
			case 's':
				bcnt += prec;
				break;
			case '_':
				switch(*++fmt) {
				case 'c': case 'p': case 'u':
					bcnt += 1;
					break;
				}
			}
		}
		cursize += bcnt * fu->reps;
	}
	return(cursize);
}

void
rewrite(fs)
	FS *fs;
{
	enum { NOTOKAY, USEBCNT, USEPREC } sokay;
	PR *pr, **nextpr;
	FU *fu;
	char *p1, *p2;
	char savech, *fmtp;
	int nconv, prec;

	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		/*
		 * break each format unit into print units; each
		 * conversion character gets its own.
		 */
		for (nconv = 0, fmtp = fu->fmt; *fmtp; nextpr = &pr->nextpr) {
			/* NOSTRICT */
			pr = (PR *)emalloc(sizeof(PR));
			if (!fu->nextpr)
				fu->nextpr = pr;
			else
				*nextpr = pr;

			/* skip preceding text and up to the next % sign */
			for (p1 = fmtp; *p1 && *p1 != '%'; ++p1);

			/* only text in the string */
			if (!*p1) {
				pr->fmt = fmtp;
				pr->flags = F_TEXT;
				break;
			}

			/*
			 * get precision for %s -- if have a byte count, don't
			 * need it.
			 */
			if (fu->bcnt) {
				sokay = USEBCNT;
				/* skip to conversion character */
				for (++p1; strchr(spec, *p1); ++p1);
			} else {
				/* skip any special chars, field width */
				while (strchr(spec + 1, *++p1));
				if (*p1 == '.' && isdigit(*++p1)) {
					sokay = USEPREC;
					prec = atoi(p1);
					while (isdigit(*++p1));
				}
				else
					sokay = NOTOKAY;
			}

			p2 = p1 + 1;		/* set end pointer */

			/*
			 * figure out the byte count for each conversion;
			 * rewrite the format as necessary, set up blank-
			 * padding for end of data.
			 */
			switch(*p1) {
			case 'c':
				pr->flags = F_CHAR;
				switch(fu->bcnt) {
				case 0: case 1:
					pr->bcnt = 1;
					break;
				default:
					errx(1, "bad byte count for conversion character \'%c\'", *p1);
				}
				break;
			case 'd': case 'i':
				pr->flags = F_INT;
				goto sw1;
			case 'l':
				++p2;
				switch(p1[1]) {
				case 'd': case 'i':
					++p1;
					pr->flags = F_INT;
					goto sw1;
				case 'o': case 'u': case 'x': case 'X':
					++p1;
					pr->flags = F_UINT;
					goto sw1;
				default:
					p1[2] = '\0';
					errx(1, "bad conversion character %%%s", p1);
				}
				/* NOTREACHED */
			case 'o': case 'u': case 'x': case 'X':
				pr->flags = F_UINT;
sw1:				switch(fu->bcnt) {
				case 0: case 4:
					pr->bcnt = 4;
					break;
				case 1:
					pr->bcnt = 1;
					break;
				case 2:
					pr->bcnt = 2;
					break;
				default:
					errx(1, "bad byte count for conversion character \'%c\'", *p1);
				}
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				pr->flags = F_DBL;
				switch(fu->bcnt) {
				case 0: case 8:
					pr->bcnt = 8;
					break;
				case 4:
					pr->bcnt = 4;
					break;
				default:
					errx(1, "bad byte count for conversion character \'%c\'", *p1);
				}
				break;
			case 's':
				pr->flags = F_STR;
				switch(sokay) {
				case NOTOKAY:
					errx(1, "%%s requires a precision or a byte count");
				case USEBCNT:
					pr->bcnt = fu->bcnt;
					break;
				case USEPREC:
					pr->bcnt = prec;
					break;
				}
				break;
			case '_':
				++p2;
				switch(p1[1]) {
				case 'A':
					endfu = fu;
					fu->flags |= F_IGNORE;
					/* FALLTHROUGH */
				case 'a':
					pr->flags = F_ADDRESS;
					++p2;
					switch(p1[2]) {
					case 'd': case 'o': case'x':
						*p1 = 'q';
						p1[1] = p1[2];
						break;
					default:
						p1[3] = '\0';
						errx(1, "bad conversion character %%%s", p1);
					}
					break;
				case 'c':
					pr->flags = F_C;
					/* *p1 = 'c';	set in conv_c */
					goto sw2;
				case 'p':
					pr->flags = F_P;
					*p1 = 'c';
					goto sw2;
				case 'u':
					pr->flags = F_U;
					/* *p1 = 'c';	set in conv_u */
sw2:					switch(fu->bcnt) {
					case 0: case 1:
						pr->bcnt = 1;
						break;
					default:
						p1[2] = '\0';
						errx(1, "bad byte count for conversion character \'%s\'", p1);
					}
					break;
				default:
					p1[2] = '\0';
					errx(1, "bad conversion character %%%s", p1);
				}
				break;
			default:
				errx(1, "bad conversion character %%%c", *p1);
			}

			/*
			 * copy to PR format string, set conversion character
			 * pointer, update original.
			 */
			savech = *p2;
			p1[(pr->flags&F_ADDRESS)?2:1] = '\0';
			if (!(pr->fmt = strdup(fmtp)))
				err(1, "malloc");
			*p2 = savech;
			pr->cchar = pr->fmt + (p1 - fmtp);
			fmtp = p2;

			/* only one conversion character if byte count */
			if (!(pr->flags&F_ADDRESS) && fu->bcnt && nconv++)
				errx(1,
				    "byte count with multiple conversion characters");
		}
		/*
		 * if format unit byte count not specified, figure it out
		 * so can adjust rep count later.
		 */
		if (!fu->bcnt)
			for (pr = fu->nextpr; pr; pr = pr->nextpr)
				fu->bcnt += pr->bcnt;
	}
	/*
	 * if the format string interprets any data at all, and it's
	 * not the same as the blocksize, and its last format unit
	 * interprets any data at all, and has no iteration count,
	 * repeat it as necessary.
	 *
	 * if, rep count is greater than 1, no trailing whitespace
	 * gets output from the last iteration of the format unit.
	 */
	for (fu = fs->nextfu;; fu = fu->nextfu) {
		if (!fu->nextfu && fs->bcnt < blocksize &&
		    !(fu->flags&F_SETREP) && fu->bcnt)
			fu->reps += (blocksize - fs->bcnt) / fu->bcnt;
		if (fu->reps > 1) {
			for (pr = fu->nextpr;; pr = pr->nextpr)
				if (!pr->nextpr)
					break;
			for (p1 = pr->fmt, p2 = NULL; *p1; ++p1)
				p2 = isspace(*p1) ? p1 : NULL;
			if (p2)
				pr->nospace = p2;
		}
		if (!fu->nextfu)
			break;
	}
}

void
escape(p1)
	char *p1;
{
	char *p2;

	/* alphabetic escape sequences have to be done in place */
	for (p2 = p1;; ++p1, ++p2) {
		if (!*p1) {
			*p2 = *p1;
			break;
		}
		if (*p1 == '\\')
			switch(*++p1) {
			case 'a':
			     /* *p2 = '\a'; */
				*p2 = '\007';
				break;
			case 'b':
				*p2 = '\b';
				break;
			case 'f':
				*p2 = '\f';
				break;
			case 'n':
				*p2 = '\n';
				break;
			case 'r':
				*p2 = '\r';
				break;
			case 't':
				*p2 = '\t';
				break;
			case 'v':
				*p2 = '\v';
				break;
			default:
				*p2 = *p1;
				break;
			}
	}
}
