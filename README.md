## About
Simple file archiver powered by `Lempel-Ziv-Stac` compression (about 70% compression rate).  
Adds about `11kb` to the final binary size.

## Usage

```cmake
add_executable(exe ...)
# include zpak lib
add_subdirectory(deps/zpak)
# link zpak
target_link_libraries(exe zpak)
```

## Create zpak
```c
#include <zpak.h>

int main() 
{
	void *buffer;
	// create zpak object for a write with LZS compression enabled
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_WRITE | ZPAK_F_LZS);
	if (!zpak) {
		fprintf(stderr, "%s\n", "Error: could not allocate zpak");
		return 1;
	}
	// get file somehow
	void *buffer = get_file_data("filename.txt");
	int fileSize = get_file_size("filename.txt");
	// write new entry into zpak, respectively compressing the data
	int compressedSize = zpak_write(zpak, "filename.txt", buffer, fileSize);
	// error occured
	if (compressedSize == -1) {
		fprintf(stderr, "%s\n", zpak_get_last_error(zpak));
		zpak_destruct(zpak);
		return 1;
	}
	// ... write more files, as needed
	compressedSize = zpak_write(zpak, "anotherfile.txt", buffer, fileSize);
	// ... do more work
	// finish writing, copy data into buffer (does not flush the internal buffer)
	int finalSize = zpak_write_end(zpak, (void**)&buffer);
	// ... save file on disk
	free(buffer);
	// free zpak resources and internal buffer
	zpak_destruct(zpak);
	return 0;
}

```

## Reading zpak
```c
#include <zpak.h>

int main() 
{
	// get zpak somehow
	void *zpakBlob = get_zpak_blob();
	int zpakBlobSize = get_zpak_blob_size();  
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_READ);
	// load blob into zpak, respectively copies zpak blob into its internal memory
	// use "zpak_load_static_data" to use blob data directly, without copying
	if (zpak_load_data(zpak, zpakBlob, zpakBlobSize) == -1) {
		fprintf(stderr, "%s\n", zpak_get_last_error(zpak));
		zpak_destruct(zpak);
		return 1;
	}
	// extract file
	void *data;
	int size = zpak_read(zpak, "filename.txt", &data);
	if (size == 0) {
		fprintf(stderr, "%s\n", zpak_get_last_error(zpak));
		zpak_destruct(zpak);
		return 1;
	}
	// ... use data
	free(data);
	zpak_destruct(zpak);
	return 0;
}
```

## Iterate zpak file entries
```c
#include <string.h>
#include <zpak.h>

int file_exists(zpak_t *zpak, const char *path, int *size) 
{
	zpak_it_t *it = zpak_it_construct(zpak);
	if (!it) {
		fprintf(stderr, "%s\n", "could not allocate iterator");
		return 0;
	}
	while (zpak_it_next(it))
	{
		const char *tempName = zpak_it_get_entry_name(it);
		if (strncmp(tempName, resolvedPath, 256) == 0)
		{
			*size = zpak_it_get_entry_size(it);
			zpak_it_destruct(it);
			return 1;
		}
	}
	zpak_it_destruct(it);
	return 0;
}
// Reads entry data. User is responsible for freeing up the buffer
int zpak_it_read(zpak_it_t *it, void **data);
// Reads entry data into user defined buffer
int zpak_it_read_buf(zpak_it_t *it, void *data, int size);
```

## Building standalone zpak archiver
```sh
$ mkdir build && cd build
$ cmake .. -DZPAK_BUILD_ARCHIVER
$ make
$ ./zpak file1.txt file2.txt archive.zpak
```

## License

### zpak

```
MIT License
 
Copyright (c) 2019 isRyven<ryven.mt@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Lempel-Ziv-Stac lib
```
----------------------------------------------------------------------------
Copyright (c) 2017 Craig McQueen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
----------------------------------------------------------------------------
```
