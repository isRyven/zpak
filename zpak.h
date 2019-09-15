/*
 * zpak-file-archiver

 * MIT License
 
 * Copyright (c) 2019 isRyven<ryven.mt@gmail.com>

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
	zpak is a lightweight in-memory file archiver, primarily used to preserve
	structure of the script files in a binary blob, which can then be injected into 
	the executable.
	
	As of version 1:
	* Does not contain directory entries.
	* Does not preserve file stats.
	* Uses lzs compression, which suits great for textual data (70% compression rate).
	* Provides only necessary functionality to read, write and walk entries.

	zpak binary blob structure:
		header {
			signature
			version
			compression
		}
		entry {
			header {
				size
				compSize
				flags
				nameLength
			}
			name 
			data
		}

	Todos: 
	* add user header structure getter
	* add user entry structure getter 
	* add an option to hash entry names into 32b/64b fields, omitting the entry names
	* implement writing/reading by handles (support stream read/writes)
*/

#ifndef ZPAK_ZPAK_H
#define ZPAK_ZPAK_H

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct zpak_s zpak_t;
typedef struct zpak_it_s zpak_it_t;

typedef enum {
	/**
	 * Read only
	 */
	ZPAK_F_READ  = 1 << 0,
	/**
	 * Write only
	 */
	ZPAK_F_WRITE = 1 << 1,
	/**
	 * Read and write
	 */
	ZPAK_F_RW    = 1 << 2,
	/**
	 * Use Lempel-Ziv-Stac compression
	 */
	ZPAK_F_LZS  = 1 << 3,
} zpak_flags_t;

/**
 * Custom allocator. When size is zero, the allocator should behave 
 * like free and return NULL. When size is not zero, the allocator 
 * should behave like realloc. The allocator should return NULL if 
 * it cannot fulfill the request. The default allocator uses malloc, 
 * realloc and free.
 */
typedef void *(*zpak_alloc_fn)(void *memctx, void *ptr, int size);

/**
 * Custom logger (non-functional)
 */
typedef void (*zpak_logger_fn)(const char *message);

/**
 * Constructs new zpak instance
 * @param allocator custom mem allocator, set NULL to use default allocator
 * @param memctx custom mem allocator context
 * @flags zpak flags
 * @return success code
 */
zpak_t* zpak_construct(zpak_alloc_fn allocator, void* memctx, unsigned int flags);

/**
 * Destruct zpak instance
 * @param ctx
 * @return success code
 */
int zpak_destruct(zpak_t *ctx);

/**
 * Load zpak from exiting blob into context, respectively copying the blob
 * @param ctx
 * @param data zpak blob
 * @param size data size
 * @return success code
 */
int zpak_load_data(zpak_t *ctx, const void *data, unsigned int size);

/**
 * Inititalize exiting zpak blob into context (no copy)
 * @param ctx
 * @param data zpak blob
 * @param size data size
 * @return success code
 */
int zpak_load_static_data(zpak_t *ctx, const void *data, unsigned int size);

/**
 * Creates new entry in zpak
 * @param ctx
 * @param entryName essentially file path set in the zpak
 * @param data data to compress
 * @param size data size
 * @return compressed size
 */
int zpak_write(zpak_t *ctx, const char *entryName, const void *data, int size);

/**
 * Returns complete archive. User is responsible for freeing it up
 * @param ctx
 * @param data pointer
 * @return data size 
 */
int zpak_write_end(zpak_t *ctx, void **data);

/**
 * Reads and decompresses entry data. User is responsible for freeing up the buffer
 * @param ctx
 * @param entryName
 * @param data decompressed data pointer
 * @return decompressed size
 */
int zpak_read(zpak_t *ctx, const char *entryName, void **data);

// iterator

/**
 * Constructs new iterator instance
 * @param ctx zpak instance
 * @param it iterator instance
 */
zpak_it_t* zpak_it_construct(zpak_t *ctx);

/**
 * Destructs iterator instance
 * @param it iterator instance
 */
void zpak_it_destruct(zpak_it_t *it);

/**
 * Moves to the next entry
 * @param it iterator instance
 * @return success code
 */
int zpak_it_next(zpak_it_t *it);

/**
 * Gets entry's decompressed data size
 * @param it iterator instance
 * @return decompressed data size
 */
int zpak_it_get_entry_size(zpak_it_t *it);

/**
 * Gets entry's name
 * @param it iterator instance
 * @return entry name pointer
 */
const char* zpak_it_get_entry_name(zpak_it_t *it);

/**
 * Reads entry data. User is responsible for freeing up the buffer
 * @param it iterator instance
 * @param data decompressed data pointer
 * @return decompressed data size
 */
int zpak_it_read(zpak_it_t *it, void **data);

/**
 * Reads entry data into user defined buffer
 * @param it iterator instance
 * @param data output buffer
 * @param size output buffer size
 * @return decompressed data size
 */
int zpak_it_read_buf(zpak_it_t *it, void *data, int size);

// error

/**
 * Gets last error string
 * @param ctx
 * @return error string
 */
const char* zpak_get_last_error(zpak_t *ctx);

/**
 * Sets custom allocator handler
 * @param ctx
 * @param allocator
 * @param memctx allocator context
 */
void zpak_set_alloc_fn(zpak_t *ctx, zpak_alloc_fn allocator, void* memctx);

/**
 * Sets custom logger handler, not functional atm
 * @param ctx
 * @param logger function
 */
void zpak_set_logger_fn(zpak_t *ctx, zpak_logger_fn logger);

#ifdef __cplusplus
}
#endif

#endif // ZPAK_ZPAK_H
