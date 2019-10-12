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

#include <inttypes.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "zpak.h"
#include "lzs/lzs.h"

// 262144 bytes
#define ZPAK_VERSION 1
#define ZPAK_INIT_SIZE 1024 * 256
#define ZPAK_BUFFER_PAD 1024

typedef struct zpak_header_s {
	char signature[4]; // ZPAK
	uint8_t version;
	uint8_t compType; // 0 -> none, 1 -> lz
} zpak_header_t;

typedef struct zpak_entry_header_s {
	uint32_t size;
	uint32_t compSize;
	uint64_t nameHash; // path hash to speedup lookups
	uint32_t flags;
	uint32_t nameLength;
} zpak_entry_header_t;

typedef enum
{
	ZO_STATIC_DATA = 1 // no deallocation, external static buffer
} zpak_options_t;

typedef struct {
	zpak_it_t *it;
	uint64_t cursor;
} zpak_entry_handle_t;

#define MAX_ENTRY_HANDLES 16

struct zpak_s {
	zpak_alloc_fn alloc;
	zpak_logger_fn logger;
	void* memctx; // allocator context
	zpak_options_t opt;
	zpak_flags_t flags;
	void *data;
	const void *staticData;
	uint32_t curSize; // buffer write size
	uint32_t bufSize; // buffer allocated size (which may be bigger)
	const char *err;
	// zpak_entry_handle_t handles[MAX_ENTRY_HANDLES];
};

struct zpak_it_s {
	zpak_t *ctx;
	uint32_t current;
};

static void* __default_alloc(void *memctx, void *ptr, int size);
static void  __default_logger(const char *message);
static void* __start_zpak(zpak_t *ctx);
static void* __resize_zpak_buffer(zpak_t *ctx, uint32_t newSize);
static uint32_t __calc_entry_size(const zpak_entry_header_t *entry);
static uint32_t __it_read_and_destruct(zpak_it_t *it, void **data);
static uint64_t __hash_string(const uint8_t *str);
static const zpak_entry_header_t* __it_get_entry_header(zpak_it_t *it);

#define SET_ERROR(str) \
	ctx->err = str; \
	return -1; \

#define SET_STR(dest, str) \
	{ \
		int i = 0; \
      	while (str[i]) { \
      		dest[i] = str[i]; \
      		i++; \
      	} \
      	dest[i] = 0; \
	} \

#define ASSERT(assertion, message) \
	if (!(assertion)) { \
		SET_ERROR(message); \
	} \

#ifdef DEBUG_LOG
	#define LOG(message) ctx->logger(message)
#else
	#define LOG(message)
#endif

#define M_MIN(a, b) a < b ? a : b
#define M_MAX(a, b) a > b ? a : b

#define GET_ZPAK_BLOB(ctx) ctx->opt & ZO_STATIC_DATA ? ctx->staticData : ctx->data;

zpak_t* zpak_construct(zpak_alloc_fn allocator, void* memctx, unsigned int flags)
{
	zpak_alloc_fn zpak_alloc = __default_alloc;
	if (allocator)
		zpak_alloc = allocator;
	zpak_t *ctx = zpak_alloc(memctx, NULL, sizeof(zpak_t));
	if (!ctx) 
		return NULL;
	memset(ctx, 0, sizeof(zpak_t));
	zpak_set_alloc_fn(ctx, zpak_alloc, memctx);
	if (flags == 0)
		flags = ZPAK_F_RW | ZPAK_F_LZS;
	ctx->flags = flags;
	return ctx;
}

int zpak_destruct(zpak_t *ctx)
{
	if (!(ctx->opt & ZO_STATIC_DATA))
	{
		if (ctx->data)
			ctx->alloc(ctx->memctx, ctx->data, 0);
	}
	ctx->data = NULL;
	ctx->staticData = NULL;
	ctx->alloc(ctx->memctx, ctx, 0);
	return 0;
}

int zpak_load_data(zpak_t *ctx, const void *data, unsigned int size)
{
	ASSERT(data, "no data was passed");
	ASSERT(size > 0, "data buffer with incorrect size");
	ASSERT(size >= sizeof(zpak_header_t), "data buffer is too small to be processed");
	ASSERT(!ctx->data, "internal data buffer already exists");
	zpak_header_t *header = (zpak_header_t*)data;
	ASSERT(strncmp(header->signature, "ZPAK", 4) == 0, "data buffer is not valid zpak");
	ASSERT(header->version == 1, "unsupported zpak version");
	ASSERT(header->compType <= 1, "unsupported zpak compression type");
	ctx->bufSize = size;
	ctx->curSize = size;
	ctx->data = ctx->alloc(ctx->memctx, NULL, size);
	if (header->compType == 1) 
		ctx->flags |= ZPAK_F_LZS;
	ASSERT(ctx->data, "could not allocate internal buffer");
	memcpy(ctx->data, data, size);
	return 0;
}

int zpak_load_static_data(zpak_t *ctx, const void *data, unsigned int size)
{
	ASSERT(data, "no data was passed");
	ASSERT(size > 0, "data buffer with incorrect size");
	ASSERT(size >= sizeof(zpak_header_t), "data buffer is too small to be processed");
	ASSERT(!ctx->data, "internal data buffer already exists");
	ASSERT(!ctx->staticData, "internal static data buffer already exists");
	zpak_header_t *header = (zpak_header_t*)data;
	ASSERT(strncmp(header->signature, "ZPAK", 4) == 0, "data buffer is not valid zpak");
	ASSERT(header->version == 1, "unsupported zpak version");
	ASSERT(header->compType <= 1, "unsupported zpak compression type");
	ctx->bufSize = size;
	ctx->curSize = size;
	ctx->opt |= ZO_STATIC_DATA;
	ctx->flags = ZPAK_F_READ;
	ctx->staticData = data;
	if (header->compType == 1) 
		ctx->flags |= ZPAK_F_LZS;
	return 0;
}

int zpak_write(zpak_t *ctx, const char *entryName, const void *data, int size)
{
	ASSERT(entryName && entryName[0], "entry name should not be an emptry string");
	ASSERT(!(ctx->opt & ZO_STATIC_DATA), "cannot write entry into static data buffer");
	ASSERT(data, "no data was passed");
	ASSERT(size > 0, "data buffer with incorrect size");
	ASSERT(!(ctx->flags & ZPAK_F_READ), "cannot write entry in non-writable zpak");
	ASSERT(ctx->data || __start_zpak(ctx), "could not allocate internal buffer");

	uint32_t nameLength = strlen(entryName) + 1;
	uint32_t dataSize = size + ZPAK_BUFFER_PAD; // compensate negative compression
	uint32_t estimatedSpace = sizeof(zpak_entry_header_t) + nameLength + dataSize;  
	uint32_t remainingSpace = ctx->bufSize - ctx->curSize;
	if (estimatedSpace > remainingSpace)
	{
		uint32_t finalSize = M_MAX(estimatedSpace, ZPAK_INIT_SIZE);
		if (!__resize_zpak_buffer(ctx, ctx->bufSize + finalSize))
		{
			SET_ERROR("could not extend existing buffer");
		}
	}
	uint8_t *cursor = (uint8_t*)ctx->data + ctx->curSize;
	zpak_entry_header_t *entry = (zpak_entry_header_t*)cursor;
	entry->size = size;
	entry->nameHash = __hash_string((const uint8_t*)entryName);
	entry->nameLength = nameLength;
	cursor += sizeof(zpak_entry_header_t);
	SET_STR(cursor, entryName);
	cursor += nameLength;
	if (ctx->flags & ZPAK_F_LZS) 
	{
		size_t compSize = lzs_compress(cursor, dataSize, data, size);
		entry->compSize = compSize;
	}
	else
	{
		memcpy(cursor, data, size);
		entry->compSize = size;
	}
	cursor += entry->compSize;
	ctx->curSize += cursor - ((uint8_t*)ctx->data + ctx->curSize);
	return entry->compSize;
}

int zpak_write_end(zpak_t *ctx, void **data)
{
	ASSERT(!(ctx->opt & ZO_STATIC_DATA), "cannot flush static data");
	ASSERT(ctx->data, "no data to flush");
	*data = ctx->alloc(ctx->memctx, NULL, ctx->curSize);
	ASSERT(*data, "could not allocate zpak output buffer");
	memcpy(*data, ctx->data, ctx->curSize); 
	return ctx->curSize;
}

int zpak_read(zpak_t *ctx, const char *entryName, void **data)
{
	ASSERT(entryName && entryName[0], "entry name should not be an emptry string");
	const void *blob = GET_ZPAK_BLOB(ctx);
	ASSERT(blob, "cannot read empty zpak blob");
	zpak_it_t *it = zpak_it_construct(ctx);
	uint64_t entryNameHash = __hash_string((const uint8_t*)entryName);
	while (zpak_it_next(it))
	{
		const zpak_entry_header_t *entryHeader = __it_get_entry_header(it);
		if (entryHeader->nameHash == entryNameHash)
			return __it_read_and_destruct(it, data);
	}
	zpak_it_destruct(it);
	return 0;
}

zpak_it_t* zpak_it_construct(zpak_t *ctx)
{
	zpak_it_t *it = ctx->alloc(ctx->memctx, NULL, sizeof(zpak_it_t));
	if (!it) 
		return NULL;
	it->ctx = ctx;
	it->current = 0;
	return it;
}

void zpak_it_destruct(zpak_it_t *it)
{
	if (!it)
		return;
	it->ctx->alloc(it->ctx->memctx, it, 0);
}

int zpak_it_next(zpak_it_t *it)
{
	const void *blob = GET_ZPAK_BLOB(it->ctx);
	if (!blob)
		return 0;
	if (it->current >= it->ctx->bufSize)
		return 0;
	if (it->current == 0)
	{
		it->current += sizeof(zpak_header_t);
	}
	else
	{
		const uint8_t *cursor = (const uint8_t*)blob + it->current;
		const zpak_entry_header_t *entry = (const zpak_entry_header_t*)cursor;
		it->current += __calc_entry_size(entry);
	}
	return it->current < it->ctx->curSize;
}

int zpak_it_get_entry_size(zpak_it_t *it)
{
	const void *blob = GET_ZPAK_BLOB(it->ctx);
	const uint8_t *cursor = (const uint8_t*)blob + it->current;
	const zpak_entry_header_t *entry = (const zpak_entry_header_t*)cursor;
	return entry->size;
}

const char* zpak_it_get_entry_name(zpak_it_t *it)
{
	const void *blob = GET_ZPAK_BLOB(it->ctx);
	const uint8_t *cursor = (const uint8_t*)blob + it->current;
	cursor += sizeof(zpak_entry_header_t);
	return (const char*)cursor;
}

static const zpak_entry_header_t* __it_get_entry_header(zpak_it_t *it) 
{
	const void *blob = GET_ZPAK_BLOB(it->ctx);
	const uint8_t *cursor = (const uint8_t*)blob + it->current;
	const zpak_entry_header_t *entry = (const zpak_entry_header_t*)cursor;
	return entry;
}

int zpak_it_read(zpak_it_t *it, void **data)
{
	zpak_t *ctx = it->ctx;
	const void *blob = GET_ZPAK_BLOB(ctx);
	ASSERT(blob, "cannot read empty zpak blob");
	const uint8_t *cursor = (const uint8_t*)blob + it->current;
	const zpak_entry_header_t *entry = (const zpak_entry_header_t*)cursor;
	cursor += sizeof(zpak_entry_header_t) + entry->nameLength;
	*data = ctx->alloc(ctx->memctx, NULL, entry->size);
	if (ctx->flags & ZPAK_F_LZS)
		lzs_decompress((uint8_t*)*data, entry->size, cursor, entry->compSize);
	else
		memcpy(*data, cursor, entry->size);
	return entry->size;
}

int zpak_it_read_buf(zpak_it_t *it, void *data, int size)
{
	zpak_t *ctx = it->ctx;
	const void *blob = GET_ZPAK_BLOB(ctx);
	ASSERT(blob, "cannot read empty zpak blob");
	const uint8_t *cursor = (const uint8_t*)blob + it->current;
	const zpak_entry_header_t *entry = (const zpak_entry_header_t*)cursor;
	cursor += sizeof(zpak_entry_header_t) + entry->nameLength;
	if (ctx->flags & ZPAK_F_LZS)
		lzs_decompress((uint8_t*)data, size, cursor, entry->compSize);
	else
		memcpy(data, cursor, entry->size);
	return entry->size;
}

static uint32_t __it_read_and_destruct(zpak_it_t *it, void **data)
{
	uint32_t size = zpak_it_read(it, data);
	zpak_it_destruct(it);
	return size;
}

const char* zpak_get_last_error(zpak_t *ctx)
{
	return ctx->err;
}

void zpak_set_alloc_fn(zpak_t *ctx, zpak_alloc_fn allocator, void* memctx)
{
	if (allocator)
	{
		ctx->alloc = allocator;
		ctx->memctx = memctx;
	}
	else
	{
		ctx->alloc = __default_alloc;
		ctx->memctx = NULL;
	}
}

void zpak_set_logger_fn(zpak_t *ctx, zpak_logger_fn logger)
{
	if (logger)
		ctx->logger = logger;
	else
		ctx->logger = __default_logger;
}

static void* __default_alloc(void *memctx, void *ptr, int size)
{
	if (size == 0) 
	{
		free(ptr);
		return NULL;
	}
	return realloc(ptr, (size_t)size);
}

static void __default_logger(const char *message)
{
	printf("%s\n", message);
}

static void* __start_zpak(zpak_t *ctx)
{
	if (ctx->data) 
		return ctx->data;
	ctx->data = ctx->alloc(ctx->memctx, NULL, ZPAK_INIT_SIZE);
	if (!ctx->data)
		return NULL;
	zpak_header_t *header = (zpak_header_t *)ctx->data;
	SET_STR(header->signature, "ZPAK")
	header->compType = 0;
	if (ctx->flags & ZPAK_F_LZS)
		header->compType = 1;
	header->version = ZPAK_VERSION;
	ctx->curSize = sizeof(zpak_header_t);
	ctx->bufSize = ZPAK_INIT_SIZE;
	return ctx->data;
}

static void* __resize_zpak_buffer(zpak_t *ctx, uint32_t newSize)
{
	if (!ctx->data)
		return NULL;
	if (ctx->curSize > newSize)
		return NULL;
	ctx->data = ctx->alloc(ctx->memctx, ctx->data, newSize);
	if (!ctx->data)
		return NULL;
	ctx->bufSize = newSize;
	return ctx->data;
}

static uint32_t __calc_entry_size(const zpak_entry_header_t *entry)
{
	uint32_t size = sizeof(zpak_entry_header_t);
	size += entry->nameLength;
	size += entry->compSize;
	return size;
}


// djb2 string hashing algorithm
static uint64_t __hash_string(const uint8_t *str)
{
    uint64_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}
