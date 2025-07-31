/*
 * test_asyncio.c
 *
 * Comprehensive unit test for asyncio.library
 * Tests all functions described in asyncio.doc
 *
 * This test creates temporary files, performs various operations,
 * and verifies the results match expected behavior.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asyncio.h>
#include <stdio.h>
#include <string.h>

struct Library *AsyncIOBase;

/* Test configuration */
#define TEST_BUFFER_SIZE 8192
#define TEST_FILE_NAME "T:asyncio_test.dat"
#define TEST_FILE_NAME2 "T:asyncio_test2.dat"
#define MAX_LINE_LENGTH 256

/* Test data - Latin-1 only, short lines */
static const char *test_strings[] = {
    "Line 1: Hello World\n",
    "Line 2: Test data\n",
    "Line 3: Short line\n",
    "Line 4: Numbers 12345\n",
    "Line 5: Special chars !@#\n",
    "Line 6: End of file\n",
    NULL
};

/* Simple test data for binary operations */
static const char *test_binary_data = "Binary test data";

/* AsyncIO-specific test configuration */
#define ASYNC_WAIT_TIME 100000  /* Microseconds to wait for async operations */
#define ASYNC_RETRY_COUNT 10    /* Number of retries for async operations */

/* Global variables for test tracking */
static LONG test_count = 0;
static LONG test_passed = 0;
static LONG test_failed = 0;

/* Test result tracking macros */
#define TEST_START(name) { \
    printf("TEST %ld: %s\n", ++test_count, name); \
}

#define TEST_PASS() { \
    printf("  PASS\n"); \
    test_passed++; \
}

#define TEST_FAIL(reason) { \
    printf("  FAIL: %s\n", reason); \
    test_failed++; \
}

#define TEST_ASSERT(condition, message) { \
    if (condition) { \
        printf("    ASSERT: %s - OK\n", message); \
    } else { \
        printf("    ASSERT: %s - FAILED\n", message); \
        TEST_FAIL(message); \
        return FALSE; \
    } \
}

/* Trace output macros for detailed debugging */
#define TRACE(msg) { \
    printf("TRACE: " msg "\n"); \
}

#define TRACE1(msg, arg1) { \
    printf("TRACE: " msg "\n", arg1); \
}

#define TRACE2(msg, arg1, arg2) { \
    printf("TRACE: " msg "\n", arg1, arg2); \
}

#define TRACE3(msg, arg1, arg2, arg3) { \
    printf("TRACE: " msg "\n", arg1, arg2, arg3); \
}

#define TRACE_READ(file, buffer, bytes, result) { \
    printf("TRACE: ReadAsync(%p, %p, %ld) = %ld\n", file, buffer, bytes, result); \
    if (result > 0 && result <= 100) { \
        printf("TRACE: Read data: '%.*s'\n", (int)result, (char*)buffer); \
    } \
}

#define TRACE_WRITE(file, buffer, bytes, result) { \
    printf("TRACE: WriteAsync(%p, %p, %ld) = %ld\n", file, buffer, bytes, result); \
    if (result > 0 && result <= 100) { \
        printf("TRACE: Wrote data: '%.*s'\n", (int)result, (char*)buffer); \
    } \
}

#define TRACE_OPEN(file, name, mode, buffer_size) { \
    printf("TRACE: OpenAsync(\"%s\", %ld, %ld) = %p\n", name, mode, buffer_size, file); \
}

#define TRACE_CLOSE(file, result) { \
    printf("TRACE: CloseAsync(%p) = %ld\n", file, result); \
}

#define TRACE_SEEK(file, position, mode, result) { \
    printf("TRACE: SeekAsync(%p, %ld, %ld) = %ld\n", file, position, mode, result); \
}

#define TRACE_CHAR_READ(file, result) { \
    printf("TRACE: ReadCharAsync(%p) = %ld", file, result); \
    if (result >= 0 && result <= 255) { \
        printf(" ('%c')", (char)result); \
    } \
    printf("\n"); \
}

#define TRACE_CHAR_WRITE(file, ch, result) { \
    printf("TRACE: WriteCharAsync(%p, '%c') = %ld\n", file, ch, result); \
}

#define TRACE_LINE_READ(file, buffer, size, result) { \
    printf("TRACE: ReadLineAsync(%p, %p, %ld) = %ld\n", file, buffer, size, result); \
    if (result > 0) { \
        printf("TRACE: Read line: '%s'\n", buffer); \
    } \
}

#define TRACE_LINE_WRITE(file, line, result) { \
    printf("TRACE: WriteLineAsync(%p, \"%s\") = %ld\n", file, line, result); \
}

#define TRACE_PEEK(file, buffer, bytes, result) { \
    printf("TRACE: PeekAsync(%p, %p, %ld) = %ld\n", file, buffer, bytes, result); \
    if (result > 0 && result <= 100) { \
        printf("TRACE: Peeked data: '%.*s'\n", (int)result, (char*)buffer); \
    } \
}

/* Function prototypes */
BOOL test_open_close(void);
BOOL test_write_operations(void);
BOOL test_read_operations(void);
BOOL test_seek_operations(void);
BOOL test_peek_operations(void);
BOOL test_line_operations(void);
BOOL test_char_operations(void);
BOOL test_error_handling(void);
BOOL test_file_handle_operations(void);
void cleanup_test_files(void);
void print_test_summary(void);

/* AsyncIO helper functions */
void wait_for_async_operation(void);
BOOL verify_file_content(const char *filename, const char *expected_data, LONG expected_length);

/* AsyncIO helper functions */

/* Wait for async operations to complete */
void wait_for_async_operation(void)
{
    TRACE("Waiting for async operation to complete");
    /* Give the file system time to complete background operations */
    Delay(ASYNC_WAIT_TIME / 1000000, (ASYNC_WAIT_TIME % 1000000) / 1000);
    TRACE("Async operation wait completed");
}

/* Verify file content using standard DOS operations */
BOOL verify_file_content(const char *filename, const char *expected_data, LONG expected_length)
{
    BPTR file;
    char buffer[256];
    LONG bytes_read;
    BOOL result = FALSE;
    
    TRACE2("Verifying file content: %s (expected %ld bytes)", filename, expected_length);
    
    file = Open(filename, MODE_READ);
    if (file != 0) {
        bytes_read = Read(file, buffer, sizeof(buffer) - 1);
        Close(file);
        
        if (bytes_read == expected_length) {
            buffer[bytes_read] = '\0';
            if (strcmp(buffer, expected_data) == 0) {
                TRACE("File content verification successful");
                result = TRUE;
            } else {
                TRACE2("File content mismatch: expected '%s', got '%s'", expected_data, buffer);
            }
        } else {
            TRACE2("File length mismatch: expected %ld, got %ld", expected_length, bytes_read);
        }
    } else {
        TRACE1("Failed to open file for verification: %s", filename);
    }
    
    return result;
}

/* Main test function */
int main(int argc, char *argv[])
{
    printf("=== AsyncIO Library Unit Test Suite ===\n");
    printf("Testing all functions from asyncio.doc\n\n");

#ifdef ASIO_SHARED_LIB
    /* Open the asyncio.library for shared library version */
    AsyncIOBase = OpenLibrary("asyncio.library", 0);
    if (AsyncIOBase == NULL) {
        printf("ERROR: Failed to open asyncio.library\n");
        printf("IoErr: %ld\n", IoErr());
        return 1;
    }
    printf("Successfully opened asyncio.library\n");
#endif

    /* Initialize test counters */
    test_count = 0;
    test_passed = 0;
    test_failed = 0;

    /* Run all test suites */
    TRACE("Starting test suite execution");
    
    if (test_open_close()) {
        printf("Open/Close tests completed\n");
    } else {
        TRACE("Open/Close tests failed");
    }
    
    if (test_write_operations()) {
        printf("Write operation tests completed\n");
    } else {
        TRACE("Write operation tests failed");
    }
    
    if (test_read_operations()) {
        printf("Read operation tests completed\n");
    } else {
        TRACE("Read operation tests failed");
    }
    
    if (test_seek_operations()) {
        printf("Seek operation tests completed\n");
    } else {
        TRACE("Seek operation tests failed");
    }
    
    if (test_peek_operations()) {
        printf("Peek operation tests completed\n");
    } else {
        TRACE("Peek operation tests failed");
    }
    
    if (test_line_operations()) {
        printf("Line operation tests completed\n");
    } else {
        TRACE("Line operation tests failed");
    }
    
    if (test_char_operations()) {
        printf("Character operation tests completed\n");
    } else {
        TRACE("Character operation tests failed");
    }
    
    if (test_error_handling()) {
        printf("Error handling tests completed\n");
    } else {
        TRACE("Error handling tests failed");
    }
    
    if (test_file_handle_operations()) {
        printf("File handle operation tests completed\n");
    } else {
        TRACE("File handle operation tests failed");
    }

    /* Cleanup and summary */
    TRACE("Starting cleanup phase");
    cleanup_test_files();
    print_test_summary();

#ifdef ASIO_SHARED_LIB
    /* Close the asyncio.library for shared library version */
    TRACE("Closing asyncio.library");
    if (AsyncIOBase != NULL) {
        TRACE1("AsyncIOBase = %p, calling CloseLibrary", AsyncIOBase);
        CloseLibrary(AsyncIOBase);
        AsyncIOBase = NULL;
        printf("Closed asyncio.library\n");
        TRACE("Library closed successfully");
    } else {
        TRACE("AsyncIOBase is NULL, no library to close");
    }
#endif

    TRACE1("Test suite completed, returning exit code %d", (test_failed == 0) ? 0 : 1);
    
    /* Ensure we always return a proper exit code */
    if (test_failed == 0) {
        TRACE("All tests passed successfully");
        return 0;
    } else {
        TRACE1("Some tests failed (%ld failures)", test_failed);
        return 1;
    }
}

/* Test OpenAsync and CloseAsync functions */
BOOL test_open_close(void)
{
    struct AsyncFile *file;
    LONG result;

    TEST_START("OpenAsync - Create new file for writing");
    TRACE1("Creating new file for writing: %s", TEST_FILE_NAME);
    file = OpenAsync(TEST_FILE_NAME, MODE_WRITE, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME, MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should return valid file handle");
    if (file) {
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync returned NULL");
        return FALSE;
    }

    TEST_START("OpenAsync - Open existing file for reading");
    TRACE1("Opening existing file for reading: %s", TEST_FILE_NAME);
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should return valid file handle for reading");
    if (file) {
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync returned NULL for reading");
        return FALSE;
    }

    TEST_START("OpenAsync - Append mode");
    TRACE1("Opening file for append mode: %s", TEST_FILE_NAME);
    file = OpenAsync(TEST_FILE_NAME, MODE_APPEND, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME, MODE_APPEND, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should return valid file handle for appending");
    if (file) {
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync returned NULL for appending");
        return FALSE;
    }

    TEST_START("CloseAsync - NULL file handle");
    TRACE("Testing CloseAsync with NULL file handle");
    result = CloseAsync(NULL);
    TRACE_CLOSE(NULL, result);
    TEST_ASSERT(result < 0, "CloseAsync should fail with NULL file handle");
    TEST_PASS();

    return TRUE;
}

/* Test WriteAsync and WriteCharAsync functions */
BOOL test_write_operations(void)
{
    struct AsyncFile *file;
    LONG result;
    const char *test_data = "Test write data\n";
    LONG data_len = strlen(test_data);

    TEST_START("WriteAsync - Write data to file");
    TRACE1("Opening file for writing: %s", TEST_FILE_NAME);
    file = OpenAsync(TEST_FILE_NAME, MODE_WRITE, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME, MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        TRACE2("Writing data: '%s' (%ld bytes)", test_data, data_len);
        result = WriteAsync(file, (APTR)test_data, data_len);
        TRACE_WRITE(file, (APTR)test_data, data_len, result);
        TEST_ASSERT(result == data_len, "WriteAsync should write all bytes");
        
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        
        /* Wait for async operations to complete */
        wait_for_async_operation();
        
        /* Verify the file content using standard DOS operations */
        TEST_ASSERT(verify_file_content(TEST_FILE_NAME, test_data, data_len), 
                   "File content should match written data");
        
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("WriteCharAsync - Write single characters");
    TRACE1("Opening file for character writing: %s", TEST_FILE_NAME2);
    file = OpenAsync(TEST_FILE_NAME2, MODE_WRITE, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME2, MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        TRACE("Writing character 'A'");
        result = WriteCharAsync(file, 'A');
        TRACE_CHAR_WRITE(file, 'A', result);
        TEST_ASSERT(result == 1, "WriteCharAsync should write one byte");
        
        TRACE("Writing character 'B'");
        result = WriteCharAsync(file, 'B');
        TRACE_CHAR_WRITE(file, 'B', result);
        TEST_ASSERT(result == 1, "WriteCharAsync should write one byte");
        
        TRACE("Writing character 'C'");
        result = WriteCharAsync(file, 'C');
        TRACE_CHAR_WRITE(file, 'C', result);
        TEST_ASSERT(result == 1, "WriteCharAsync should write one byte");
        
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    return TRUE;
}

/* Test ReadAsync and ReadCharAsync functions */
BOOL test_read_operations(void)
{
    struct AsyncFile *file;
    LONG result;
    char buffer[256];
    LONG byte_read;

    TEST_START("ReadAsync - Read data from file");
    TRACE1("Opening file for reading: %s", TEST_FILE_NAME);
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        TRACE1("Reading up to %ld bytes into buffer", sizeof(buffer) - 1);
        result = ReadAsync(file, buffer, sizeof(buffer) - 1);
        TRACE_READ(file, buffer, sizeof(buffer) - 1, result);
        TEST_ASSERT(result > 0, "ReadAsync should read some data");
        
        buffer[result] = '\0';
        TRACE2("Successfully read %ld bytes: '%s'", result, buffer);
        
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("ReadCharAsync - Read single characters");
    TRACE1("Opening file for character reading: %s", TEST_FILE_NAME2);
    file = OpenAsync(TEST_FILE_NAME2, MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME2, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        TRACE("Reading first character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == 'X', "ReadCharAsync should read 'X'");
        
        TRACE("Reading second character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == 'Y', "ReadCharAsync should read 'Y'");
        
        TRACE("Reading third character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == 'Z', "ReadCharAsync should read 'Z'");
        
        TRACE("Reading at EOF");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == -1, "ReadCharAsync should return -1 at EOF");
        
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    return TRUE;
}

/* Test SeekAsync function */
BOOL test_seek_operations(void)
{
    struct AsyncFile *file;
    LONG result;
    char buffer[10];

    TEST_START("SeekAsync - Seek to beginning of file");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        result = SeekAsync(file, 0, MODE_START);
        TEST_ASSERT(result >= 0, "SeekAsync should succeed");
        
        result = ReadAsync(file, buffer, 5);
        TEST_ASSERT(result == 5, "ReadAsync should read 5 bytes after seek");
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("SeekAsync - Seek from current position");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        result = SeekAsync(file, 5, MODE_CURRENT);
        TEST_ASSERT(result >= 0, "SeekAsync should succeed");
        
        result = ReadAsync(file, buffer, 5);
        TEST_ASSERT(result == 5, "ReadAsync should read 5 bytes after seek");
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("SeekAsync - Get current position");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        result = SeekAsync(file, 0, MODE_CURRENT);
        TEST_ASSERT(result >= 0, "SeekAsync should return current position");
        printf("    Current position: %ld", result);
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    return TRUE;
}

/* Test PeekAsync function */
BOOL test_peek_operations(void)
{
    struct AsyncFile *file;
    LONG result;
    char buffer1[10], buffer2[10];

    TEST_START("PeekAsync - Peek without advancing file pointer");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        /* Peek first 5 bytes */
        result = PeekAsync(file, buffer1, 5);
        TEST_ASSERT(result == 5, "PeekAsync should read 5 bytes");
        
        /* Read the same 5 bytes */
        result = ReadAsync(file, buffer2, 5);
        TEST_ASSERT(result == 5, "ReadAsync should read 5 bytes");
        
        /* Compare the data */
        TEST_ASSERT(memcmp(buffer1, buffer2, 5) == 0, "Peeked and read data should be identical");
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    return TRUE;
}

/* Test ReadLineAsync, WriteLineAsync, FGetsAsync, and FGetsLenAsync functions */
BOOL test_line_operations(void)
{
    struct AsyncFile *file;
    LONG result;
    char buffer[MAX_LINE_LENGTH];
    LONG length;
    int i;

    TEST_START("WriteLineAsync - Write lines to file");
    file = OpenAsync(TEST_FILE_NAME, MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        for (i = 0; test_strings[i] != NULL; i++) {
            result = WriteLineAsync(file, (STRPTR)test_strings[i]);
            TEST_ASSERT(result == strlen(test_strings[i]), "WriteLineAsync should write all bytes");
        }
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("ReadLineAsync - Read lines from file");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        for (i = 0; test_strings[i] != NULL; i++) {
            result = ReadLineAsync(file, buffer, sizeof(buffer));
            TEST_ASSERT(result == strlen(test_strings[i]), "ReadLineAsync should read correct number of bytes");
            TEST_ASSERT(strcmp(buffer, test_strings[i]) == 0, "ReadLineAsync should read correct data");
        }
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("FGetsAsync - Read lines with FGetsAsync");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        for (i = 0; test_strings[i] != NULL; i++) {
            APTR result_ptr = FGetsAsync(file, buffer, sizeof(buffer));
            TEST_ASSERT(result_ptr == buffer, "FGetsAsync should return buffer pointer");
            TEST_ASSERT(strcmp(buffer, test_strings[i]) == 0, "FGetsAsync should read correct data");
        }
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("FGetsLenAsync - Read lines with length tracking");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        for (i = 0; test_strings[i] != NULL; i++) {
            APTR result_ptr = FGetsLenAsync(file, buffer, sizeof(buffer), &length);
            TEST_ASSERT(result_ptr == buffer, "FGetsLenAsync should return buffer pointer");
            TEST_ASSERT(length == strlen(test_strings[i]), "FGetsLenAsync should return correct length");
            TEST_ASSERT(strcmp(buffer, test_strings[i]) == 0, "FGetsLenAsync should read correct data");
        }
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    return TRUE;
}

/* Test ReadCharAsync and WriteCharAsync functions */
BOOL test_char_operations(void)
{
    struct AsyncFile *file;
    LONG result;
    LONG byte_read;

    TEST_START("WriteCharAsync - Write individual characters");
    TRACE1("Opening file for character writing: %s", TEST_FILE_NAME2);
    file = OpenAsync(TEST_FILE_NAME2, MODE_WRITE, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME2, MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        TRACE("Writing character 'X'");
        result = WriteCharAsync(file, 'X');
        TRACE_CHAR_WRITE(file, 'X', result);
        TEST_ASSERT(result == 1, "WriteCharAsync should write one byte");
        
        TRACE("Writing character 'Y'");
        result = WriteCharAsync(file, 'Y');
        TRACE_CHAR_WRITE(file, 'Y', result);
        TEST_ASSERT(result == 1, "WriteCharAsync should write one byte");
        
        TRACE("Writing character 'Z'");
        result = WriteCharAsync(file, 'Z');
        TRACE_CHAR_WRITE(file, 'Z', result);
        TEST_ASSERT(result == 1, "WriteCharAsync should write one byte");
        
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("ReadCharAsync - Read individual characters");
    TRACE1("Opening file for character reading: %s", TEST_FILE_NAME2);
    file = OpenAsync(TEST_FILE_NAME2, MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, TEST_FILE_NAME2, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        TRACE("Reading first character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == 'X', "ReadCharAsync should read 'X'");
        
        TRACE("Reading second character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == 'Y', "ReadCharAsync should read 'Y'");
        
        TRACE("Reading third character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == 'Z', "ReadCharAsync should read 'Z'");
        
        TRACE("Reading at EOF");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ(file, byte_read);
        TEST_ASSERT(byte_read == -1, "ReadCharAsync should return -1 at EOF");
        
        TRACE1("Closing file handle %p", file);
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    return TRUE;
}

/* Test error handling scenarios */
BOOL test_error_handling(void)
{
    struct AsyncFile *file;
    LONG result;
    char buffer[10];

    TEST_START("Error handling - Open non-existent file for reading");
    file = OpenAsync("NONEXISTENT_FILE", MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file == NULL, "OpenAsync should return NULL for non-existent file");
    if (file == NULL) {
        printf("    IoErr: %ld", IoErr());
        TEST_PASS();
    } else {
        CloseAsync(file);
        TEST_FAIL("OpenAsync should fail for non-existent file");
        return FALSE;
    }

    TEST_START("Error handling - Read from write-only file");
    file = OpenAsync(TEST_FILE_NAME, MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        result = ReadAsync(file, buffer, 5);
        TEST_ASSERT(result == -1, "ReadAsync should fail on write-only file");
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    TEST_START("Error handling - Write to read-only file");
    file = OpenAsync(TEST_FILE_NAME, MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed");
    
    if (file) {
        result = WriteAsync(file, "test", 4);
        TEST_ASSERT(result == -1, "WriteAsync should fail on read-only file");
        
        result = CloseAsync(file);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed");
        return FALSE;
    }

    return TRUE;
}

/* Test OpenAsyncFromFH function */
BOOL test_file_handle_operations(void)
{
    struct AsyncFile *async_file;
    BPTR dos_file;
    LONG result;
    char buffer[10];

    TEST_START("OpenAsyncFromFH - Open from DOS file handle");
    dos_file = Open(TEST_FILE_NAME, MODE_READ);
    TEST_ASSERT(dos_file != 0, "DOS Open should succeed");
    
    if (dos_file != 0) {
        async_file = OpenAsyncFromFH(dos_file, MODE_READ, TEST_BUFFER_SIZE);
        TEST_ASSERT(async_file != NULL, "OpenAsyncFromFH should return valid file handle");
        
        if (async_file) {
            result = ReadAsync(async_file, buffer, 5);
            TEST_ASSERT(result == 5, "ReadAsync should read 5 bytes");
            
            result = CloseAsync(async_file);
            TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        }
        
        Close(dos_file);
        TEST_PASS();
    } else {
        TEST_FAIL("DOS Open failed");
        return FALSE;
    }

    return TRUE;
}

/* Cleanup test files */
void cleanup_test_files(void)
{
    TRACE("Starting file cleanup");
    printf("\nCleaning up test files...\n");
    
    TRACE1("Deleting test file: %s", TEST_FILE_NAME);
    if (DeleteFile(TEST_FILE_NAME) == 0) {
        printf("Deleted %s\n", TEST_FILE_NAME);
        TRACE1("Successfully deleted %s", TEST_FILE_NAME);
    } else {
        TRACE2("Failed to delete %s, IoErr: %ld", TEST_FILE_NAME, IoErr());
    }
    
    TRACE1("Deleting test file: %s", TEST_FILE_NAME2);
    if (DeleteFile(TEST_FILE_NAME2) == 0) {
        printf("Deleted %s\n", TEST_FILE_NAME2);
        TRACE1("Successfully deleted %s", TEST_FILE_NAME2);
    } else {
        TRACE2("Failed to delete %s, IoErr: %ld", TEST_FILE_NAME2, IoErr());
    }
    
    TRACE("File cleanup completed");
}

/* Print test summary */
void print_test_summary(void)
{
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %ld\n", test_count);
    printf("Passed: %ld\n", test_passed);
    printf("Failed: %ld\n", test_failed);
    printf("Success rate: %.1f%%\n", (test_count > 0) ? (test_passed * 100.0 / test_count) : 0.0);
    
    if (test_failed == 0) {
        printf("\nALL TESTS PASSED! \\o/\n");
    } else {
        printf("\nSOME TESTS FAILED! :(\n");
    }
} 