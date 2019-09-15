#include "minunit.h"
#include "zpak.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// fixme, try not to rely on internal structures
#define ZPAK_HEADER_SIZE 6
#define ZPAK_ENTRY_HEADER_SIZE 24

const char data[] = { 's', 'o', 'm', 'e', 'd', 'a', 't', 'a', 0 };
size_t dataLength = sizeof(data);

const char data2[] = { 'm', 'o', 'r', 'e', 'd', 'a', 't', 'a', 0 };
size_t data2Length = sizeof(data2);

void test_setup(void)
{
}

void test_teardown(void) 
{
}

MU_TEST(it_should_be_constructed_and_destructed) 
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	mu_assert(zpak, "should construct zpak structure");
	zpak_destruct(zpak);
}

MU_TEST(it_should_not_allow_to_write_entries_if_flag_is_set) 
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_READ);
	int res = zpak_write(zpak, "test", "somedata", strlen("somedata") + 1);
	mu_assert(res == -1, "should return -1 if not succeeded");
	zpak_destruct(zpak);
}

MU_TEST(it_should_write_new_entry) 
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	int res = zpak_write(zpak, "test", data, dataLength);
	mu_assert(res == (int)dataLength, "should not compress the data and return the size");
	zpak_destruct(zpak);
}

MU_TEST(it_should_return_proper_binary_blob) 
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	int res = zpak_write(zpak, "test", data, dataLength);
	mu_assert(res == (int)dataLength, "should not compress the data and return the size");
	void *output;
	int outSize = zpak_write_end(zpak, &output);
	int expectedOutSize = ZPAK_HEADER_SIZE + ZPAK_ENTRY_HEADER_SIZE + 5 + dataLength;
	char *sig = (char *)output;
	mu_assert(strncmp(sig, "ZPAK", 4) == 0, "should start with ZPAK");
	mu_assert(outSize == expectedOutSize, "should return proper size");
	free(output);
	zpak_destruct(zpak);
}

MU_TEST(it_should_load_existing_pak)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	zpak_write(zpak, "test", data, dataLength);
	void *output;
	int totalSize = zpak_write_end(zpak, &output);
	zpak_t *zpak2 = zpak_construct(NULL, NULL, ZPAK_F_READ);
	int result = zpak_load_data(zpak2, output, totalSize);
	mu_assert(result == 0, zpak_get_last_error(zpak2));
	zpak_destruct(zpak);
	zpak_destruct(zpak2);
	free(output);
}

MU_TEST(it_should_load_existing_static_pak)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	zpak_write(zpak, "test", data, dataLength);
	void *output;
	int totalSize = zpak_write_end(zpak, &output);
	zpak_t *zpak2 = zpak_construct(NULL, NULL, ZPAK_F_READ);
	int result = zpak_load_static_data(zpak2, output, totalSize);
	mu_assert(result == 0, zpak_get_last_error(zpak2));
	zpak_destruct(zpak);
	zpak_destruct(zpak2);
	free(output);
}

MU_TEST(it_should_forbid_writing_in_static_buffer)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	zpak_write(zpak, "test", data, dataLength);
	void *output;
	int totalSize = zpak_write_end(zpak, &output);
	zpak_t *zpak2 = zpak_construct(NULL, NULL, ZPAK_F_READ);
	int result = zpak_load_static_data(zpak2, output, totalSize);
	mu_assert(result == 0, zpak_get_last_error(zpak2));
	int res = zpak_write(zpak2, "test-2", data, dataLength);
	mu_assert(res == -1, "it should not allow to write new entries");
	zpak_destruct(zpak);
	zpak_destruct(zpak2);
	free(output);
}

MU_TEST(it_should_pak_and_unpack_entry)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	zpak_write(zpak, "test", data, dataLength);
	void *output;
	int totalSize = zpak_write_end(zpak, &output);
	zpak_t *zpak2 = zpak_construct(NULL, NULL, ZPAK_F_READ);
	zpak_load_data(zpak2, output, totalSize);
	void *outdata;
	int readSize = zpak_read(zpak2, "test", &outdata);
	mu_assert(readSize == (int)dataLength, "should return proper data size");
	mu_assert(strcmp(data, outdata) == 0, "should unpack entry data");
	zpak_destruct(zpak);
	zpak_destruct(zpak2);
	free(output);
	free(outdata);
}

MU_TEST(it_should_write_multiple_entrie)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	zpak_write(zpak, "test", data, dataLength);
	zpak_write(zpak, "more", data2, data2Length);
	void *output;
	int totalSize = zpak_write_end(zpak, &output);
	int expectedTotalSize = ZPAK_HEADER_SIZE + 
		ZPAK_ENTRY_HEADER_SIZE + 5 + dataLength +
		ZPAK_ENTRY_HEADER_SIZE + 5 + data2Length;
	mu_assert(totalSize == expectedTotalSize, "should return proper zpak size");

	zpak_t *zpak2 = zpak_construct(NULL, NULL, ZPAK_F_READ);
	zpak_load_data(zpak2, output, totalSize);
	void *outdata;
	zpak_read(zpak2, "test", &outdata);
	mu_assert(strcmp(data, outdata) == 0, "should unpack entry data");
	free(outdata);
	zpak_read(zpak2, "more", &outdata);
	mu_assert(strcmp(data2, outdata) == 0, "should unpack entry data");
	free(outdata);
	free(output);
	zpak_destruct(zpak);
	zpak_destruct(zpak2);
}

MU_TEST(it_should_create_iterator)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	zpak_write(zpak, "path1", data, dataLength);
	zpak_write(zpak, "path2", data, dataLength);
	zpak_write(zpak, "path3", data, dataLength);
	const char *expectedPaths[] = { "path1", "path2", "path3" };
	zpak_it_t *it = zpak_it_construct(zpak);
	int i;
	for (i = 0; zpak_it_next(it); i++)
	{
		const char *tempName = zpak_it_get_entry_name(it);
		int expecteSize = zpak_it_get_entry_size(it);
		mu_assert(expecteSize == (int)dataLength, "should match the expected size");
		mu_assert(strcmp(expectedPaths[i], tempName) == 0, "should match expected names");
	}
	mu_assert(i == 3, "should iterate all entries");
	zpak_it_destruct(it);
	zpak_destruct(zpak);
}

MU_TEST(it_should_compress_entry_data_using_lzs_compression)
{
	const char data[] = {'d','a','t','a',' ','d','a','t','a',' ','d','a','t','a',' ','d','a','t','a',' ',0};
	size_t dataLength = sizeof(data);
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW | ZPAK_F_LZS);
	int compSize = zpak_write(zpak, "pathpathpathpath", data, dataLength);
	mu_assert(compSize < (int)dataLength,  "should return compressed size");
	zpak_destruct(zpak);
}

MU_TEST(it_should_decompress_lzs_compressed_entry_data)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW | ZPAK_F_LZS);
	zpak_write(zpak, "test", data, dataLength);
	void *output;
	int finalSize = zpak_write_end(zpak, &output);
	zpak_t *zpak2 = zpak_construct(NULL, NULL, ZPAK_F_READ);
	zpak_load_data(zpak, output, finalSize);
	void *outdata;
	int decompSize = zpak_read(zpak, "test", &outdata);
	mu_assert(decompSize == (int)dataLength, "the lengths should match");
	mu_assert(strcmp(data, outdata) == 0, "the data should be correctly decompressed");	
	free(output);
	free(outdata);
	zpak_destruct(zpak2);
	zpak_destruct(zpak);
}

MU_TEST(it_should_write_data_in_user_buffer)
{
	zpak_t *zpak = zpak_construct(NULL, NULL, ZPAK_F_RW);
	zpak_write(zpak, "test", data, dataLength);
	zpak_write(zpak, "more", data2, data2Length);
	zpak_it_t *it = zpak_it_construct(zpak);
	zpak_it_next(it);
	int expectedSize = zpak_it_get_entry_size(it);
	void *userBuff = malloc(expectedSize);
	int readSize = zpak_it_read_buf(it, userBuff, expectedSize);
	mu_assert_int_eq(expectedSize, readSize);
	free(userBuff);
	zpak_it_destruct(it);
	zpak_destruct(zpak);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&test_setup, &test_teardown);
	MU_RUN_TEST(it_should_be_constructed_and_destructed);
	MU_RUN_TEST(it_should_not_allow_to_write_entries_if_flag_is_set);
	MU_RUN_TEST(it_should_write_new_entry);
	MU_RUN_TEST(it_should_return_proper_binary_blob);
	MU_RUN_TEST(it_should_load_existing_pak);
	MU_RUN_TEST(it_should_load_existing_static_pak);
	MU_RUN_TEST(it_should_forbid_writing_in_static_buffer);
	MU_RUN_TEST(it_should_pak_and_unpack_entry);
	MU_RUN_TEST(it_should_write_multiple_entrie);
	MU_RUN_TEST(it_should_create_iterator);
	MU_RUN_TEST(it_should_compress_entry_data_using_lzs_compression);
	MU_RUN_TEST(it_should_decompress_lzs_compressed_entry_data);
	MU_RUN_TEST(it_should_write_data_in_user_buffer);
}

int main(int argc, char **argv) {
	MU_RUN_SUITE(test_suite);
	MU_REPORT();
	return minunit_fail;
}
