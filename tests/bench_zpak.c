#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "zpak.h"

double get_time()
{
    return (double)clock() / CLOCKS_PER_SEC;
}

double benchmark_entry_search(int n)
{
	#define MAX_PATH 64
	static char path[MAX_PATH];
	const char *data = "Nunc leo velit, feugiat sit amet ornare at, sodales nec velit. Nulla sed hendrerit orci.";
	int dataLength = strlen(data) + 1;
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	// create entries
	for (int i = 0; i < n; i++)
	{
		int pathLength;
		while ((pathLength = (rand() % MAX_PATH)) < 40) {};
		for (int c = 0; c < pathLength; c++)
		{
			path[c] = 65 + (rand() % 60);
		}
		path[pathLength - 1] = 0;
		int writeSize = zpak_write(zpak, path, data, dataLength);
		assert(writeSize == dataLength);
	}
	char *output;
	double start = get_time();
	int readSize = zpak_read(zpak, path, (void**)&output);
	double end = get_time();
	assert(readSize == dataLength);
	assert(strcmp(data, output) == 0);
	zpak_destruct(zpak);
	return end - start;
}

int main(int arg, const char **argv) 
{	
	printf("benchmarks:\n"
		"* entry search: %fs\n", benchmark_entry_search(1000000)
	);
	return 0;
}
