#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>

#include "zpak.h"

#define OK 0
#define NOT_OK 1
#define LIB_ERR -1

int readFile(const char *name, void **output, int *size) 
{
	FILE *f = fopen(name, "rb");
	int querySize, readSize;
	void *buffer;
	if (!f) {
		perror(name);
		return NOT_OK;
	}
	if (fseek(f, 0, SEEK_END) < 0) {
		fclose(f);
		perror(name);
		return NOT_OK;
	}
	querySize = ftell(f);
	if (querySize < 0) {
		fclose(f);
		perror(name);
		return NOT_OK;
	}
	if (fseek(f, 0, SEEK_SET) < 0) {
		fclose(f);
		perror(name);
		return NOT_OK;
	}
	buffer = malloc(querySize);
	if (!buffer) {
		fclose(f);
		perror(name);
		return NOT_OK;
	}
	readSize = fread(buffer, 1, querySize, f);
	if (readSize != querySize)
	{
		free(buffer);
		fclose(f);
		perror(name);
		return NOT_OK;
	}
	fclose(f);
	*output = buffer;
	*size = readSize;
	return OK;
}

int writeFile(const char *name, void *output, int size) {
	int rsize;
	FILE *f = fopen(name, "wb+");
	if (!f) {
		perror(name);
		return NOT_OK;
	}
	rsize = fwrite(output, 1, size, f);
	if (rsize != size) {
		fclose(f);
		perror(name);
		return NOT_OK;
	}
	fclose(f);
	return OK;
}

int esnurePath(const char *path) {
	FILE *f = fopen(path, "wr+");
	if (!f) {
		return NOT_OK;
	}
	fclose(f);
	return OK;
}

int writeArchive(int argc, const char **argv) {
	int i, rsize, wsize, psize;
	int totalSize = 0;
	float compression;
	zpak_t *pak;
	void *buffer;
	const char *output, *input;
	/* we need minimum two files (input and output) */
	if (argc < 2) {
		fprintf(stderr, "%s\n", "ERROR: expected at least one input and output");
		return NOT_OK;
	}
	/* validate output */
	output = argv[argc - 1];
	if (esnurePath(output) == NOT_OK) {
		perror(output);
		return NOT_OK;
	}
	pak = zpak_construct(NULL, NULL, ZPAK_F_WRITE | ZPAK_F_LZS);
	if (!pak) {
		fprintf(stderr, "ERROR: could not init zpak");
		return NOT_OK;
	}
	fprintf(stdout, "INFO: archiving %i files\n", argc - 1);
	for (i = 0; i < argc - 1; ++i) {
		input = argv[i];
		if (readFile(input, &buffer, &rsize) == NOT_OK) {
			zpak_destruct(pak);
			return NOT_OK;
		}
		wsize = zpak_write(pak, input, buffer, rsize);
		if (wsize == LIB_ERR) {
			fprintf(stderr, "ERROR: %s\n", zpak_get_last_error(pak));
			zpak_destruct(pak);
			free(buffer);
			return NOT_OK;
		}
		totalSize += wsize;
		compression = (1.f - (float)wsize / (float)rsize) * 100.f;
		free(buffer);
		fprintf(stdout, "    LZS %i/%i comp %02f%c %s\n", wsize, rsize, compression, '%', input);
	}
	psize = zpak_write_end(pak, (void**)&buffer);
	if (writeFile(output, buffer, psize) == NOT_OK) {
		free(buffer);
		zpak_destruct(pak);
		return NOT_OK;
	}
	fprintf(stdout, "INFO: output %s %ib -> %ib\n", output, totalSize, psize);
	free(buffer);
	zpak_destruct(pak);
	return OK;
}

int listArchive(int argc, const char **argv)
{
	int i, size;
	zpak_t *pak;
	zpak_it_t *it;
	void *buffer;
	const char *output, *input;
	/* we need minimum one file (input) */
	if (argc < 1) {
		fprintf(stderr, "%s\n", "ERROR: expected at least one input");
		return NOT_OK;
	}
	output = argv[argc - 1];
	if (readFile(output, &buffer, &size) == NOT_OK) {
		return NOT_OK;
	}
	pak = zpak_construct(NULL, NULL, ZPAK_F_READ);
	if (!pak) {
		free(buffer);
		fprintf(stderr, "ERROR: could not init zpak");
		return NOT_OK;
	}
	if (zpak_load_static_data(pak, buffer, size) == LIB_ERR) {
		free(buffer);
		zpak_destruct(pak);
		fprintf(stderr, "ERROR: %s\n", zpak_get_last_error(pak));
		return NOT_OK;
	}
	
	it = zpak_it_construct(pak);
	if (!it) {
		free(buffer);
		zpak_destruct(pak);
		fprintf(stderr, "ERROR: could not allocate iterator");
		return NOT_OK;
	}
	while (zpak_it_next(it)) {
		const char *entryName = zpak_it_get_entry_name(it);
		/* filter */
		if (argc > 1) {
			for (i = 0; i < argc - 1; ++i) {
				input = argv[i];
				if (strncmp(entryName, input, 256) == 0) {
					printf("    LZS %ib %s\n", zpak_it_get_entry_size(it), entryName);
				}
			}
		} else {
			printf("    LZS %ib %s\n", zpak_it_get_entry_size(it), entryName);
		}
	}
	zpak_it_destruct(it);
	zpak_destruct(pak);
	free(buffer);
	return OK;
}

int main(int argc, const char **argv)
{
	enum {
		ACT_ARCHIVE_NONE,
		ACT_ARCHIVE_WRITE,
		ACT_ARCHIVE_READ,
		ACT_ARCHIVE_LIST,
		ACT_ARCHIVE_ADD
	};
	int action = ACT_ARCHIVE_NONE;
	if (argc <= 2) {
		if (argc == 2)
			fprintf(stderr, "ERROR: no paths were provided\n");
		else 
			fprintf(stderr, "ERROR: no actions were requested\n");
		// fprintf(stderr, "Usage: zpak [-w/-a path [path ...] output, -r [path [path ...]] input, -l [path, [path ...]] input]\n");
		fprintf(stderr, "Usage: zpak [-w/-a path [path ...] output, -l [path, [path ...]] input]\n");
		fprintf(stderr, "       -w Writes files into zpak\n");
		// fprintf(stderr, "       -r Reads files from zpak\n");
		fprintf(stderr, "       -l Lists files in zpak\n");
		fprintf(stderr, "       -a add files to zpak\n");
		// fprintf(stderr, "       -f specify extraction output path\n");
		return NOT_OK;
	}
	if (argv[1][0] != '-') {
		fprintf(stderr, "ERROR: expected command flag as first argument");
		return NOT_OK;
	}
	/* get command */
	switch ((argv[1][1])) {
		case 'w':
			action = ACT_ARCHIVE_WRITE;
			break;
		case 'r':
			action = ACT_ARCHIVE_READ;
			break;
		case 'l':
			action = ACT_ARCHIVE_LIST;
			break;
		case 'a':
			action = ACT_ARCHIVE_ADD;
			break;
		default:
			break;
	}
	/* validate command */
	if (action == ACT_ARCHIVE_NONE) {
		fprintf(stderr, "ERROR: expected valid command flag as first argument");
		return NOT_OK;
	}
	/* run command */
	if (action == ACT_ARCHIVE_WRITE) {
		if (writeArchive(argc - 2, argv + 2) == NOT_OK) {
			return NOT_OK;
		}
	} else if (action == ACT_ARCHIVE_READ) {
		fprintf(stderr, "ERROR: not implemented\n");
	} else if (action == ACT_ARCHIVE_LIST) {
		if (listArchive(argc - 2, argv + 2) == NOT_OK) {
			return NOT_OK;
		}
	} else if (action == ACT_ARCHIVE_ADD) {
		fprintf(stderr, "ERROR: not implemented\n");
	}

	return OK;
}
