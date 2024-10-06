/*
 * bin2exe - A small utility to convert .com file to .exe
 *
 * This is a modified version of the bin2exe utility included in booterify.
 * 
 * The original code is available at https://github.com/raphnet/booterify and 
 * distributed under the MIT License.
 *
 * Copyright (c) 2019 Raphaël Assenat
 * Copyright (c) 2022 Davide Bresolin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define EXTRA_BTTES_OFFSET		2
#define NUM_PAGES_OFFSET		4
#define MIN_EXTRA_PARA_OFFSET	10
#define MAX_EXTRA_PARA_OFFSET	12
#define INITIAL_SP_OFFSET		16
#define INITIAL_IP_OFFSET		20

#define DEFAULT_IP		0x0100
#define DEFAULT_SSIZE	0x0200

static void writeLe16(unsigned char *dst, int offset, int value)
{
	dst[offset] = value;
	dst[offset+1] = value >> 8;
}

static void printHelp(void)
{
	printf("Usage: ./bin2exe [options] input_file output_file.exe\n");
	printf(" -i offset       Set initial instruction pointer (IP). Default: %d\n", DEFAULT_IP);
	printf(" -s size         Set stack size. Default %d\n", DEFAULT_SSIZE);
	printf(" -v              Enable verbose output\n");
}

int main(int argc, char **argv)
{
	FILE *infptr, *outfptr;
	const char *infile, *outfile;
	long insize;
	int initial_ip = DEFAULT_IP;
	int st_size = DEFAULT_SSIZE;
	int verbose = 0;
	int opt;
	int pages, extra_bytes, extra_memory, initial_sp;
	unsigned char *binary;
	unsigned char exe_header[32] = {
		'M','Z',	// SIG
		0,0,		// Extra bytes
		0,0,		// Pages
		0,0,		// Relocation items (none)
		2,0,		// Header size (in paragraphs. 32 bytes)
		0x00, 0x10,	// minimum paragraphs
		0x00, 0x10, // required paragraphs
		0xF0, 0xFF, // initial SS
		0xFF, 0xFE, // initial SP
		0x00, 0x00, // checksum
		0x00, 0x00, // Initial IP
		0xF0, 0xFF, // initial CS
		0x00, 0x00, // relocation segment offset
		0x00, 0x00, // overlay
		0x00, 0x00, // overlay information
	};

	while (-1 != (opt = getopt(argc, argv, "hi:s:v"))) {
		switch (opt)
		{
			case 'h':
				printHelp();
				return 0;

			case 'i':
				initial_ip = strtol(optarg, NULL, 0);
				break;

			case 's':
				st_size = strtol(optarg, NULL, 0);
				break;

			case 'v':
				verbose = 1;
				break;

			case '?':
				break;

			default:
				fprintf(stderr, "unknown option. try -h\n");
				return -1;
		}
	}

	if (argc - optind < 2) {
		fprintf(stderr, "An input and output file must be specified. See -h\n");
		return -1;
	}

	infile = argv[optind];
	outfile = argv[optind+1];

	if (verbose) {
		printf("Input file: %s\n", infile);
		printf("Output file: %s\n", outfile);
		printf("initial ip: $%04x\n", initial_ip);
	}

	infptr = fopen(infile, "rb");
	if (!infptr) {
		perror(infile);
		return -1;
	}

	fseek(infptr, 0, SEEK_END);
	insize = ftell(infptr);
	if (insize > 0x10000) {
		fprintf(stderr, "Input file too large (> 64k)\n");
		fclose(infptr);
		return -1;
	}

	binary = malloc(insize);
	if (!binary) {
		perror("could not allocate memory to load file\n");
		fclose(infptr);
		return -1;
	}

	fseek(infptr, 0, SEEK_SET);
	if (fread(binary, insize, 1, infptr) != 1) {
		perror("cound not read input file\n");
		free(binary);
		fclose(infptr);
		return -1;
	}

	fclose(infptr);

	outfptr = fopen(outfile, "wb");
	if (!outfptr) {
		perror(outfile);
		free(binary);
		return -1;
	}

	// update the header

	pages = (insize + 511) / 512;
	extra_bytes = insize - (insize / 512) * 512;
	initial_sp = insize + st_size;
	extra_memory = (st_size + 15) / 16;

	if (verbose) {
		printf("Size: %ld bytes (%d pages + %d byte(s))\n", insize, pages, extra_bytes);
	}

	writeLe16(exe_header, INITIAL_IP_OFFSET, initial_ip);
	writeLe16(exe_header, EXTRA_BTTES_OFFSET, extra_bytes);
	writeLe16(exe_header, NUM_PAGES_OFFSET, pages);

	writeLe16(exe_header, MIN_EXTRA_PARA_OFFSET, extra_memory);
	writeLe16(exe_header, MAX_EXTRA_PARA_OFFSET, extra_memory);
	writeLe16(exe_header, INITIAL_SP_OFFSET, initial_sp);

	fwrite(exe_header, sizeof(exe_header), 1, outfptr);
	fwrite(binary, insize, 1, outfptr);

	fclose(outfptr);

	free(binary);

	return 0;
}
