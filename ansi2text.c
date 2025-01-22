/*-
 * Copyright (c) 2012, 2013 Nathanial Sloss <nathanialsloss@yahoo.com.au>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE ``AUTHOR'' AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int lines = 25;
int cols = 80;

char *screen;
char *bgcolor;
char *fgcolor;
char *attributes;
int screensize;

char ansibuffer[256];
char *leftover;

int curpos, newpos, cury, newy, curx, newx;
int ansiattr, ansiattr_old;
int bg_color, fg_color, oldbg, oldfg;

int ansipos;
char ansicode;
int ansinum;
bool csistart;

bool colorflag;

void
writebuffer(void *buf, int count) {
	int fdout = fileno(stdout);

	write(fdout, buf, count);

	return;
}

void
writescreen(void) {
	int newbg, newfg, newattr, m, n, o, p;
	int colorcode_len;
	char colorcode[20];
	char nl = 0x0a;
	char buf[2048];
	int buffercount = 0;
	int buffersize = 2048;

	for (m = 0; m < screensize; m += cols) {
		for (n = 0; n < cols; n++) {
			o = n + m;

			if (colorflag && (attributes[o] != ansiattr_old)) {

				snprintf(colorcode, sizeof(colorcode),
				    "\033[m");

				colorcode_len = strlen(colorcode);
				for (p = 0; p < colorcode_len; p++) {
					if (buffercount >= buffersize) {
						writebuffer(buf, buffercount);
						buffercount = 0;
					}
					buf[buffercount] = colorcode[p];
					buffercount++;
				}
			}

			if (colorflag && (bgcolor[o] != oldbg ||
			    fgcolor[o] != oldfg || attributes[o]
			    != ansiattr_old || n == 0)) {
				if (attributes[o] == -1)
					newattr = 0;
				else
					newattr = attributes[o];

				if (bgcolor[o] == -1)
					newbg = 49;
				else
					newbg = bgcolor[o];

				if (fgcolor[o] == -1)
					newfg = 39;
				else
					newfg = fgcolor[o];
			
				snprintf(colorcode, sizeof(colorcode),
				    "\033[%d;%d;%dm", newfg, newbg, newattr);

				ansiattr_old = attributes[o];
				oldbg = bgcolor[o];
				oldfg = fgcolor[o];

				colorcode_len = strlen(colorcode);
				for (p = 0; p < colorcode_len; p++) {
					if (buffercount >= buffersize) {
						writebuffer(buf, buffercount);
						buffercount = 0;
					}
					buf[buffercount] = colorcode[p];
					buffercount++;
				}
			}

			if (buffercount >= buffersize) {
				writebuffer(buf, buffercount);
				buffercount = 0;
			}
			buf[buffercount] = screen[o];
			buffercount++;
		}
		if (buffercount >= buffersize) {
			writebuffer(buf, buffercount);
			buffercount = 0;
		}
		buf[buffercount] = nl;
		buffercount++;
	}
	writebuffer(buf, buffercount);

	return;
}
void parseansi(void) {
	unsigned int j, k, l;
	/* Strip first two characters esc[ */
	for (j = 0; j < sizeof(ansibuffer); j++) {
		if (j + 2 >= (sizeof(ansibuffer)))
			k = 0;
		else
			k = ansibuffer[j + 2];
		ansibuffer[j] = k;
	}
	ansinum = strtoimax(ansibuffer, &leftover, 10);
	strcpy(ansibuffer, leftover);

	if (ansicode == 'J') {
		writescreen();

		ansiattr = 0;
		bg_color = 49;
		fg_color = 39;

		if (ansinum == 2)
			l = 0;
		else
			l = (cury * cols) + curx - 1;
		for (j = l; j < (unsigned int)screensize; j++) {
			screen[j] = ' ';
			attributes[j] = ansiattr;
			bgcolor[j] = bg_color;
			fgcolor[j] = fg_color;
		}
	}
	if (ansicode == 'A') {
		writescreen();
		if (ansinum == 0)
			cury--;
		else if (ansinum > 0)
			cury -= ansinum;
		if (cury < 1)
			cury = 1;
	}
	if (ansicode == 'B') {
		if (ansinum == 0)
			cury++;
		else if (ansinum > 0)
			cury += ansinum;
		if (cury > lines) {
			writescreen();
			cury = lines;
		}
	}
	if (ansicode == 'C') {
		if (ansinum == 0)
			curx++;
		else if (ansinum > 0)
			curx += ansinum;
		if (curx > cols) {
			writescreen();
			curx = cols;
		}
	}
	if (ansicode == 'D') {
		writescreen();
		if (ansinum == 0)
			curx--;
		else if (ansinum > 0)
			curx -= ansinum;
		if (curx < 1)
			curx = 1;
	}
	if (ansicode == 'E') {
		if (ansinum == 0)
			cury++;
		if (ansinum > 0)
			cury += ansinum;
		curx = 1;
		if (cury > lines) {
			writescreen();
			cury = lines;
		}
	}
	if (ansicode == 'F') {
		writescreen();
		if (ansinum == 0)
			cury--;
		if (ansinum > 0)
			cury -= ansinum;
		curx = 1;
		if (cury < 1)
			cury = 1;
	}
	if (ansicode == 'G') {
		if (ansinum < curx)
			writescreen();
		if (ansinum > 0)
			curx = ansinum;
		if (curx > cols) {
			writescreen();
			curx = cols;
		}
	}
	if (ansicode == 'H' || ansicode == 'f') {
		if (ansinum < 1)
			newy = 1;
		else
			newy = ansinum;

		if (ansibuffer[0] == ';') {
			for (j = 0; j < sizeof(ansibuffer); j++) {
				if (j + 1 >= (sizeof(ansibuffer)))
					k = 0;
				else
					k = ansibuffer[j + 1];
				ansibuffer[j] = k;
			}
			newx = strtoimax(ansibuffer, &leftover, 10);
			if (newx < 1)
				newx = 1;
		}

		if (newx > cols)
			newx = cols;
		if (newy > lines)
			newy = lines;

		if ((newy < cury)) 
			writescreen();
		curx = newx;
		cury = newy;
	}
	if (ansicode == 'm') {
		if (ansinum >= 0 && ansinum < 30)
			ansiattr = ansinum;
		else if (ansinum >= 30 && ansinum <= 39)
			fg_color = ansinum;
		else if (ansinum >= 40 && ansinum < 49)
			bg_color = ansinum;

		while (ansibuffer[0] == ';') {
			for (j = 0; j < sizeof(ansibuffer); j++) {
				if (j + 1 >= (sizeof(ansibuffer)))
					k = 0;
				else
					k = ansibuffer[j + 1];
				ansibuffer[j] = k;
			}
			ansinum = strtoimax(ansibuffer, &leftover, 10);
			strcpy(ansibuffer, leftover);

			if (ansinum >= 0 && ansinum < 30)
				ansiattr = ansinum;
			else if (ansinum >= 30 && ansinum <= 39)
				fg_color = ansinum;
			else if (ansinum >= 40 && ansinum < 49)
				bg_color = ansinum;
		}
	}

	return;
}

int
main(int argc, char **argv) {
	int ch, nr, readpos;
	int fdin = fileno(stdin);
	char buffer[2048];

	cols = 80;
	lines = 25;

	curpos = newpos = 0;
	cury = newy = 1;
	curx = newx = 1;
	bg_color = fg_color = oldbg = oldfg = -1;
	ansiattr = ansiattr_old = -1;
	
	colorflag = false;			/* Defaults to b&w */

	while ((ch = getopt(argc, argv, "cl:w:")) != -1)
		switch (ch) {
		case 'c':
			colorflag = true;
			break;
		case 'l':
			lines = atoi(optarg);	/* lines per screen */
			break;
		case 'w':
			cols = atoi(optarg);	/* coloumns per screen */
			break;
		default:
		case '?':
			(void)fprintf(stderr,
			    "usage: ansi2text [-c -w [width] -l [lines]] [file]\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	argv += optind;

	if (*argv) {
		if ((fdin = open(*argv, O_RDONLY)) < 0)
			err(errno, "Error opening file: %s", *argv);
	}

	screensize = cols * lines;
	if (screensize < 1) {
		fprintf(stderr, "Screensize must be at least 1 x 1.\n");

		return EXIT_FAILURE;
	}

	/* Allocate virtual screen */
	screen = malloc(screensize);
	attributes = malloc(screensize);
	bgcolor = malloc(screensize);
	fgcolor = malloc(screensize);

	/* Initialise virtual screen with spaces */
	memset(screen, ' ', screensize);
	memset(bgcolor, -1, screensize);
	memset(fgcolor, -1, screensize);
	memset(attributes, -1, screensize);

	while ((nr = read(fdin, &buffer, sizeof(buffer))) != -1 
	    && nr != 0) {
		for(readpos = 0; readpos < nr; readpos++) {

			if (buffer[readpos] == 0x1b) {
				csistart = true;
				memset(ansibuffer, 0, sizeof(ansibuffer));
				ansipos = 0;
				ansinum = 0;
				ansicode = 0;
			}
			if (csistart) {
				ansibuffer[ansipos] = buffer[readpos];

				if (ansibuffer[ansipos] >= '0' &&
				    ansipos > 1 && ansibuffer[1] != '[')
					csistart = false;

				if (ansibuffer[ansipos] >= 'A' &&
				    ansipos > 1 && ansibuffer[1] == '[') {
					ansicode = ansibuffer[ansipos];
				} else
					ansicode = 0;
				if (ansicode) {
					parseansi();
					csistart = false;
					ansicode = 0;
				}
				ansipos++;
			} else {
				if (buffer[readpos] == 0x0a) {
					cury++;
					curx = 1;
				}
				if (buffer[readpos] == 0x0d)
					curx = 1;
				if (buffer[readpos] == 0x08) {
					curx--;
					if (curx < 1)
						curx = 1;
				}

				if (curx > cols) {
					cury++;
					curx = 1;
				}
				if (cury > lines) {
					writescreen();
					cury = 1;
				}

				curpos = ((cury - 1) * cols) + curx - 1;
				if (curpos == screensize) {
					cury = curx = 1;
					curpos = 0;
				}

				if (buffer[readpos] >= ' ') {
					if ((fg_color != 39 || bg_color != 49) &&
					    ansiattr == 0)
						ansiattr = 22;

					attributes[curpos] = ansiattr;
					bgcolor[curpos] = bg_color;
					fgcolor[curpos] = fg_color;
					screen[curpos] = buffer[readpos];
					curx++;
				}
			}
		}
	}
	writescreen();

	close(fdin);

	/* free virtual screen */
	free(fgcolor);
	free(bgcolor);
	free(attributes);
	free(screen);

	return EXIT_SUCCESS;
}
