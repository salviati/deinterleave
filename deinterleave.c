/*
    deinterleave - a utility for converting interleaved SMD file(s) into a raw binary file
    copyright (C) 2005, Utkan Güngördü <utkan@freeconsole.org>


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#define _GNU_SOURCE
#include <getopt.h>

#define PACKAGE "deinterleave"
#define VERSION "0.0.1"


typedef struct smd_header_s
{
	unsigned char blocks;          /* each block is 16k */
	unsigned char _3;              /* always 3 */
	unsigned char type_flag;       /* 0 = single/last file, 0x40 = split file */
	unsigned char fill_0[5];       /* always 0 */
	unsigned char _aa;             /* always 0xaa */
	unsigned char _bb;             /* always 0xbb */
	unsigned char _6;              /* always 6 */
	unsigned char fill_1[0x1f5];   /* always 0 */
} smd_header_t
#ifdef __GNUC__
__attribute__((packed))
#else
#warning it seems that your compiler is not GCC. smd_header struct is expected to packed;  you need to add the corresponding statement to tell your compiler how to do this.
#endif
;

int opt_inplace, opt_calconly, opt_silent, opt_fragile, opt_checksum_given;
unsigned short opt_checksum;


static int msgl = 5;


enum msgl_e {
	MSGL_INF = 0,
	MSGL_ERR = 9,
	/***********/
	MSGL_VERB = 0,
	MSGL_SIL = 9,
};


static int error(int level, const char *fmt, ...)
{
	va_list va;
	int ret = 0;

	if(msgl > level) return ret;

	va_start(va, fmt);
	ret = vfprintf(stderr, fmt, va);
	va_end(va);

	return ret;
}


static void display_usage()
{
	fprintf(stdout, "%s\n", PACKAGE);
	fprintf(stdout, "A utility for converting interleaved SMD file(s) into a raw binary file\n\n");

	fprintf(stdout, "usage: %s [options] <romlist...>\n\n", PACKAGE);

	fprintf(stdout, "Options:\n");

	fprintf(stdout, "-c --stdout        output the modified file to stdout\n");
	fprintf(stdout, "-f --fragile       fragile mode: stop treating roms on first one results in failure\n");
	fprintf(stdout, "-h --help          display this message and quit\n");
	fprintf(stdout, "-L --license       display software license and quit\n");
	fprintf(stdout, "-o --output <file> write output to file rather than stdout\n");
	fprintf(stdout, "-s --silent        silent mode: display only error messages\n");
	fprintf(stdout, "-v --verbose       be verbose\n");
	fprintf(stdout, "-V --version       display version information and quit\n");
}


static void display_version()
{
	fprintf(stdout, "%s %s (%s)\n", PACKAGE, VERSION, __DATE__);
}


static void display_license()
{
	fprintf(stdout, "You may redistribute copies of this program\n");
	fprintf(stdout, "under the terms of the GNU General Public License.\n");
	fprintf(stdout, "For more information about these matters, see the file named COPYING.\n");
	fprintf(stdout, "Report bugs to <bug@freeconsole.org>.\n");
}


int main(int argc, char *argv[])
{
	FILE *f_in, *f_out;
	int n, c;
	unsigned char *in, *out, *p_in, *p_out;
	unsigned int size;
	smd_header_t h;

	struct option long_options[] =
	{
		{"stdout",    no_argument, 0, 'c'},
		{"help",      no_argument, 0, 'h'},
		{"license",   no_argument, 0, 'L'},
		{"silent",    no_argument, 0, 's'},
		{"strict",    no_argument, 0, 'S'},
		{"version",   no_argument, 0, 'V'},
		{0, 0, 0, 0}
	};

	while((c = getopt_long(argc, argv, "chLsSV", long_options, 0)) != EOF)
	{
		switch(c)
		{
			case 'c':
				opt_inplace = 0;
			break;

			case 'C':
				opt_calconly = 1;
			break;

			case 'f':
				opt_fragile = 1;
			break;

			case 'h':
				display_usage();
				exit(0);

			case 'i':
				opt_inplace = 1;
			break;

			case 'L':
				display_license();
				exit(0);
			break;

			case 's':
				opt_silent = 1;
			break;

			case 'S':
				opt_checksum_given = 1;
				opt_checksum = (unsigned short)strtol(optarg, NULL, 0);
			break;

			case 'V':
				display_version();
				exit(0);
		}
	}

	if(argc-optind == 0)
	{
		if(!opt_silent) fprintf(stderr, "%s: no input files, trying stdin\n", PACKAGE);
		treat_stdin();
		return 0;
	}

	while (optind < argc)
		if(treat_file(argv[optind++]) && opt_fragile) break;



	f_in = fopen(argv[1], "r");
	f_out = fopen(argv[2], "w");

	assert(sizeof(smd_header_t) == 0x200);

	fread(&h, sizeof(smd_header_t), 1, f_in);

	fprintf(stderr, "%d blocks...\n", h.blocks);

	switch(h.type_flag)
	{
		case 0:
			fprintf(stderr, "single/first file\n");
			break;

		case 0x40:
			fprintf(stderr, "split file\n");
			break;

		default:
			fprintf(stderr, "invalid type_flag\n");
			exit(1);
	}

	fseek(f_in, 0, SEEK_END);
	size = /*ftell(f_in) - 0x200*/ h.blocks<<14;
	fprintf(stderr, "allocating %u bytes...\n", size);
	fseek(f_in, 0x200, SEEK_SET);

	in = (unsigned char*)malloc(size);
	assert(in);
	out = (unsigned char*)malloc(size);
	assert(out);

	fread(in, 1, size, f_in);

	for(p_in = in, p_out = out; h.blocks--; p_in+=0x4000, p_out+=0x4000)
	{
		for(n = 0; n < 0x2000; n++)
		{
			p_out[n*2    ] = p_in[0x2000 + n];
			p_out[n*2 + 1] = p_in[0x0000 + n];
		}
	}

	fwrite(out, 1, size, f_out);

	return 0;
}
