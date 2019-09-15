#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>

#include "zpak.h"

int main(int argc, const char **argv)
{
	FILE *f;
	unsigned char *buf;
	const char *name;
	// char cwd[1024];
	int size, r, compsize;
	unsigned int totalsize = 0;
	float comprate;
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_WRITE | ZPAK_F_LZS);
	if (!zpak)
	{
		fprintf(stderr, "%s\n", "Error: could not allocate zpak");
		return 1;
	}
	if (argc <= 2)
	{
		fprintf(stderr, "%s\n", "Error: no input files were provided");
		fprintf(stderr, "%s\n", "Usage: zpak-exe input [input ...] output");
		zpak_destruct(zpak);
		return 1;
	}
	fprintf(stdout, "Info: packing %i files\n", argc - 2);
	for (int i = 1; i < argc - 1; i++)
	{
		name = argv[i];
		f = fopen(argv[i], "rb");
		if (!f)
		{
			zpak_destruct(zpak);
			fprintf(stderr, "Error: could not open file for a read: %s\n%s\n", argv[i], strerror(errno));
			return 1;
		}
		if (fseek(f, 0, SEEK_END) < 0)
		{
			fclose(f);
			zpak_destruct(zpak);
			fprintf(stderr, "Error: could not seek in file: %s\n%s\n", argv[i], strerror(errno));
			return 1;
		}
		size = ftell(f);
		if (size < 0)
		{
			fclose(f);
			zpak_destruct(zpak);
			fprintf(stderr, "Error: could not tell in file: %s\n%s\n", argv[i], strerror(errno));
			return 1;
		}
		if (fseek(f, 0, SEEK_SET) < 0)
		{
			fclose(f);
			zpak_destruct(zpak);
			fprintf(stderr, "Error: could not seek in file: %s\n%s\n", argv[i], strerror(errno));
			return 1;
		}
		buf = malloc(size);
		if (!buf)
		{
			fclose(f);
			zpak_destruct(zpak);
			fprintf(stderr, "Error: could not allocate %ib chunk for file: %s\n", size, argv[i]);
			return 1;
		}
		r = fread(buf, 1, size, f);
		if (r != size)
		{
			free(buf);
			fclose(f);
			zpak_destruct(zpak);
			fprintf(stderr, "Error: could not read data from file: %s\n%s\n", argv[i], strerror(errno));
			return 1;
		}
		totalsize += size;
		compsize = zpak_write(zpak, name, buf, size);
		if (compsize == -1)
		{
			fprintf(stderr, "%s\n", zpak_get_last_error(zpak));
			free(buf);
			fclose(f);
			zpak_destruct(zpak);
			return 1;
		}
		comprate = (1.f - (float)compsize / (float)size) * 100.f;
		free(buf);
		fclose(f);
		fprintf(stdout, "    LZS %i/%i comp %02f%c %s\n", compsize, size, comprate, '%', name);
	}
	
	f = fopen(argv[argc - 1], "wb+");
	if (!f)
	{
		zpak_destruct(zpak);
		fprintf(stderr, "Error: could not open for a write: %s\n%s\n", argv[argc-1], strerror(errno));
		return 1;
	}
	size = zpak_write_end(zpak, (void**)&buf);
	r = fwrite(buf, 1, size, f);
	if (r != size)
	{
		fprintf(stderr, "Warning: could not write full size: %i/%i\n", r, size);
	}
	fclose(f);
	zpak_destruct(zpak);
	fprintf(stdout, "Output %s %ib -> %ib\n", argv[argc-1], totalsize, size);
	return 0;
}
