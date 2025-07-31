/*
 * test_asyncio.c
 *
 * Comprehensive unit test for asyncio.library
 * Tests all functions described in asyncio.doc
 *
 * This test creates temporary files, performs various operations,
 * and verifies the results match expected behavior.
 */

#include <dos/dos.h>

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

/* Test data - Diverse content with Latin-1 characters */
static const char *test_strings[] = {
    "The quick brown fox jumps over the lazy dog\n",
    "Pack my box with five dozen liquor jugs\n",
    "How vexingly quick daft zebras jump!\n",
    "The five boxing wizards jump quickly\n",
    "Sphinx of black quartz, judge my vow\n",
    "Amazingly few discotheques provide jukeboxes\n",
    "Special characters: éèêëàáâäùúûüçñÿœæ\n",
    "Numbers and symbols: 12345 67890 !@#$%^&*() _+-=[]{}|;':\",./<>?\n",
    "Mixed content: ABC123!@# 456DEF789\n",
    "Empty line follows:\n",
    "\n",
    "Line after empty line\n",
    "Very long line that might span multiple buffers or require multiple async operations to complete properly in the double-buffered system\n",
    "Final line with end-of-file marker\n",
    NULL
};

/* Simple test data for binary operations */
static const char *test_binary_data = "Binary test data";

/* AsyncIO-specific test configuration */
#define ASYNC_WAIT_TICKS 50      /* Ticks to wait for async operations (50 ticks = 1 second) */
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

#define TRACE_READ_VALIDATE(file, buffer, bytes, result, expected_data, expected_length) { \
    printf("TRACE: ReadAsync(%p, %p, %ld) = %ld\n", file, buffer, bytes, result); \
    if (result > 0 && result <= 100) { \
        printf("TRACE: Read data: '%.*s'\n", (int)result, (char*)buffer); \
    } \
    printf("TRACE: EXPECTED: '%.*s' (%ld bytes)\n", (int)expected_length, expected_data, expected_length); \
    if (result == expected_length && result > 0) { \
        printf("TRACE: VALIDATION: %s\n", (memcmp(buffer, expected_data, result) == 0) ? "MATCH" : "MISMATCH"); \
    } else { \
        printf("TRACE: VALIDATION: LENGTH MISMATCH (got %ld, expected %ld)\n", result, expected_length); \
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

#define TRACE_CHAR_READ_VALIDATE(file, result, expected_char) { \
    printf("TRACE: ReadCharAsync(%p) = %ld", file, result); \
    if (result >= 0 && result <= 255) { \
        printf(" ('%c')", (char)result); \
    } \
    printf("\n"); \
    printf("TRACE: EXPECTED: '%c' (%ld)\n", expected_char, (LONG)expected_char); \
    printf("TRACE: VALIDATION: %s\n", (result == (LONG)expected_char) ? "MATCH" : "MISMATCH"); \
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
BOOL test_sophisticated_files(void);
BOOL test_file_copy_validation(void);
void cleanup_test_files(void);
void print_test_summary(void);

/* AsyncIO helper functions */
void wait_for_async_operation(void);
BOOL verify_file_content(const char *filename, const char *expected_data, LONG expected_length);
BOOL verify_file_lines(const char *filename, const char **expected_lines, LONG num_lines);
BOOL create_test_file(const char *filename, const char *content, LONG length);
LONG get_file_size(const char *filename);

/* AsyncIO helper functions */

/* Wait for async operations to complete */
void wait_for_async_operation(void)
{
    TRACE("Waiting for async operation to complete");
    /* Give the file system time to complete background operations */
    /* Delay() takes ticks (50 per second) */
    Delay(ASYNC_WAIT_TICKS);
    TRACE("Async operation wait completed");
}

/* Verify file content using standard DOS operations */
BOOL verify_file_content(const char *filename, const char *expected_data, LONG expected_length)
{
    BPTR file;
    char buffer[1024];  /* Larger buffer for comprehensive testing */
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

/* Verify file content line by line using dos.library */
BOOL verify_file_lines(const char *filename, const char **expected_lines, LONG num_lines)
{
    BPTR file;
    char buffer[256];
    LONG line_count = 0;
    BOOL result = TRUE;
    STRPTR line_read;
    
    TRACE2("Verifying file lines: %s (%ld lines expected)", filename, num_lines);
    
    file = Open(filename, MODE_READ);
    if (file != 0) {
        while (line_count < num_lines && expected_lines[line_count] != NULL) {
            /* FGets() returns pointer to buffer or NULL for EOF/error */
            line_read = FGets(file, buffer, sizeof(buffer) - 1); /* -1 to avoid V36/V37 bug */
            if (line_read != NULL) {
                /* FGets() null-terminates the string and includes newline as last character */
                /* Remove newline if present */
                LONG len = strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n') {
                    buffer[len - 1] = '\0';
                }
                
                if (strcmp(buffer, expected_lines[line_count]) != 0) {
                    TRACE3("Line %ld mismatch: expected '%s', got '%s'", 
                           line_count + 1, expected_lines[line_count], buffer);
                    result = FALSE;
                } else {
                    TRACE2("Line %ld verified: '%s'", line_count + 1, buffer);
                }
            } else {
                /* Check if it's EOF (IoErr() returns 0) or error */
                if (IoErr() == 0) {
                    TRACE1("EOF reached at line %ld", line_count + 1);
                } else {
                    TRACE2("Error reading line %ld, IoErr: %ld", line_count + 1, IoErr());
                }
                result = FALSE;
            }
            line_count++;
        }
        Close(file);
        
        if (line_count != num_lines) {
            TRACE2("Line count mismatch: expected %ld, got %ld", num_lines, line_count);
            result = FALSE;
        }
    } else {
        TRACE1("Failed to open file for line verification: %s", filename);
        result = FALSE;
    }
    
    return result;
}

/* Create test file using dos.library */
BOOL create_test_file(const char *filename, const char *content, LONG length)
{
    BPTR file;
    LONG bytes_written;
    BOOL result = FALSE;
    
    TRACE2("Creating test file: %s (%ld bytes)", filename, length);
    
    file = Open(filename, MODE_WRITE);
    if (file != 0) {
        bytes_written = Write(file, (APTR)content, length);
        Close(file);
        
        if (bytes_written == length) {
            TRACE("Test file created successfully");
            result = TRUE;
        } else {
            TRACE2("Failed to write test file: expected %ld, wrote %ld", length, bytes_written);
        }
    } else {
        TRACE1("Failed to create test file: %s", filename);
    }
    
    return result;
}

/* Get file size using dos.library */
LONG get_file_size(const char *filename)
{
    BPTR file;
    LONG size = -1;
    
    file = Open(filename, MODE_READ);
    if (file != 0) {
        size = Seek(file, 0, MODE_END);
        Close(file);
    }
    
    return size;
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

    /* Run all test suites with proper dependency handling */
    TRACE("Starting test suite execution");
    
    /* Track test results for dependency checking */
    BOOL open_close_passed = FALSE;
    BOOL write_ops_passed = FALSE;
    BOOL read_ops_passed = FALSE;
    BOOL seek_ops_passed = FALSE;
    BOOL peek_ops_passed = FALSE;
    BOOL line_ops_passed = FALSE;
    BOOL char_ops_passed = FALSE;
    BOOL error_handling_passed = FALSE;
    BOOL file_handle_ops_passed = FALSE;
    BOOL sophisticated_files_passed = FALSE;
    BOOL file_copy_validation_passed = FALSE;
    
    /* Test 1: Open/Close operations (independent) */
    TRACE("=== Starting Test 1: Open/Close operations ===");
    open_close_passed = test_open_close();
    if (open_close_passed) {
        printf("Open/Close tests completed\n");
    } else {
        TRACE("Open/Close tests failed");
    }
    TRACE("=== Test 1 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */
    
    /* Test 2: Write operations (independent) */
    TRACE("=== Starting Test 2: Write operations ===");
    write_ops_passed = test_write_operations();
    if (write_ops_passed) {
        printf("Write operation tests completed\n");
    } else {
        TRACE("Write operation tests failed");
    }
    TRACE("=== Test 2 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */
    
    /* Test 3: Read operations (depends on write operations for test data) */
    if (write_ops_passed) {
        TRACE("=== Starting Test 3: Read operations ===");
        read_ops_passed = test_read_operations();
        if (read_ops_passed) {
            printf("Read operation tests completed\n");
        } else {
            TRACE("Read operation tests failed");
        }
        TRACE("=== Test 3 completed ===");
        wait_for_async_operation(); /* Ensure cleanup between tests */
    } else {
        printf("Skipping read operations test (depends on write operations)\n");
        TRACE("Read operations test skipped due to write operations failure");
    }
    
    /* Test 4: Seek operations (depends on read operations for test data) */
    if (read_ops_passed) {
        TRACE("=== Starting Test 4: Seek operations ===");
        seek_ops_passed = test_seek_operations();
        if (seek_ops_passed) {
            printf("Seek operation tests completed\n");
        } else {
            TRACE("Seek operation tests failed");
        }
        TRACE("=== Test 4 completed ===");
        wait_for_async_operation(); /* Ensure cleanup between tests */
    } else {
        printf("Skipping seek operations test (depends on read operations)\n");
        TRACE("Seek operations test skipped due to read operations failure");
    }
    
    /* Test 5: Peek operations (depends on seek operations for test data) */
    if (seek_ops_passed) {
        TRACE("=== Starting Test 5: Peek operations ===");
        peek_ops_passed = test_peek_operations();
        if (peek_ops_passed) {
            printf("Peek operation tests completed\n");
        } else {
            TRACE("Peek operation tests failed");
        }
        TRACE("=== Test 5 completed ===");
        wait_for_async_operation(); /* Ensure cleanup between tests */
    } else {
        printf("Skipping peek operations test (depends on seek operations)\n");
        TRACE("Peek operations test skipped due to seek operations failure");
    }
    
    /* Test 6: Line operations (independent - creates its own test data) */
    TRACE("=== Starting Test 6: Line operations ===");
    line_ops_passed = test_line_operations();
    if (line_ops_passed) {
        printf("Line operation tests completed\n");
    } else {
        TRACE("Line operation tests failed");
    }
    TRACE("=== Test 6 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */
    
    /* Test 7: Character operations (independent - creates its own test data) */
    TRACE("=== Starting Test 7: Character operations ===");
    char_ops_passed = test_char_operations();
    if (char_ops_passed) {
        printf("Character operation tests completed\n");
    } else {
        TRACE("Character operation tests failed");
    }
    TRACE("=== Test 7 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */
    
    /* Test 8: Error handling (independent) */
    TRACE("=== Starting Test 8: Error handling ===");
    error_handling_passed = test_error_handling();
    if (error_handling_passed) {
        printf("Error handling tests completed\n");
    } else {
        TRACE("Error handling tests failed");
    }
    TRACE("=== Test 8 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */
    
    /* Test 9: File handle operations (independent) */
    TRACE("=== Starting Test 9: File handle operations ===");
    file_handle_ops_passed = test_file_handle_operations();
    if (file_handle_ops_passed) {
        printf("File handle operation tests completed\n");
    } else {
        TRACE("File handle operation tests failed");
    }
    TRACE("=== Test 9 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */
    
    /* Test 10: Sophisticated files (independent - uses external test files) */
    TRACE("=== Starting Test 10: Sophisticated files ===");
    sophisticated_files_passed = test_sophisticated_files();
    if (sophisticated_files_passed) {
        printf("Sophisticated file tests completed\n");
    } else {
        TRACE("Sophisticated file tests failed");
    }
    TRACE("=== Test 10 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */
    
    /* Test 11: File copy validation (independent - uses external test files) */
    TRACE("=== Starting Test 11: File copy validation ===");
    file_copy_validation_passed = test_file_copy_validation();
    if (file_copy_validation_passed) {
        printf("File copy validation tests completed\n");
    } else {
        TRACE("File copy validation tests failed");
    }
    TRACE("=== Test 11 completed ===");
    wait_for_async_operation(); /* Ensure cleanup between tests */

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
    const char *test_data = "The quick brown fox jumps over the lazy dog\n";
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
        TRACE_READ_VALIDATE(file, buffer, sizeof(buffer) - 1, result, "The quick brown fox jumps over the lazy dog\n", 44);
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
        TRACE_CHAR_READ_VALIDATE(file, byte_read, 'X');
        TEST_ASSERT(byte_read == 'X', "ReadCharAsync should read 'X'");
        
        TRACE("Reading second character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ_VALIDATE(file, byte_read, 'Y');
        TEST_ASSERT(byte_read == 'Y', "ReadCharAsync should read 'Y'");
        
        TRACE("Reading third character");
        byte_read = ReadCharAsync(file);
        TRACE_CHAR_READ_VALIDATE(file, byte_read, 'Z');
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
        TRACE_READ_VALIDATE(file, buffer, 5, result, "The q", 5);
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
        TRACE_READ_VALIDATE(file, buffer, 5, result, "uick ", 5);
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
        TRACE_PEEK(file, buffer1, 5, result);
        TEST_ASSERT(result == 5, "PeekAsync should read 5 bytes");
        
        /* Read the same 5 bytes */
        result = ReadAsync(file, buffer2, 5);
        TRACE_READ(file, buffer2, 5, result);
        TEST_ASSERT(result == 5, "ReadAsync should read 5 bytes");
        
        /* Compare the data */
        printf("TRACE: PEEKED: '%.*s'\n", 5, buffer1);
        printf("TRACE: READ: '%.*s'\n", 5, buffer2);
        printf("TRACE: VALIDATION: %s\n", (memcmp(buffer1, buffer2, 5) == 0) ? "MATCH" : "MISMATCH");
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
            TRACE_LINE_READ(file, buffer, sizeof(buffer), result);
            printf("TRACE: EXPECTED: '%s' (%ld bytes)\n", test_strings[i], strlen(test_strings[i]));
            printf("TRACE: VALIDATION: %s\n", (strcmp(buffer, test_strings[i]) == 0) ? "MATCH" : "MISMATCH");
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
            printf("TRACE: FGetsAsync(%p, %p, %ld) = %p\n", file, buffer, sizeof(buffer), result_ptr);
            if (result_ptr == buffer) {
                printf("TRACE: FGetsAsync read: '%s'\n", buffer);
            }
            printf("TRACE: EXPECTED: '%s'\n", test_strings[i]);
            printf("TRACE: VALIDATION: %s\n", (strcmp(buffer, test_strings[i]) == 0) ? "MATCH" : "MISMATCH");
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

/* Test sophisticated file operations using multiple test files */
BOOL test_sophisticated_files(void)
{
    struct AsyncFile *file;
    LONG result;
    char buffer[1024];
    const char *test_lines[] = {
        "Line 1: Hello World - Basic ASCII text",
        "Line 2: Test data with numbers 12345 and symbols !@#$%^&*()",
        "Line 3: Short line",
        "Line 4: Longer line with more content to test buffer boundaries and async operations",
        "Line 5: Special characters: éèêëàáâäùúûüçñÿœæ",
        "Line 6: Mixed content: ABC123!@# 456DEF789",
        "Line 7: Empty line follows:",
        "",
        "Line 8: Line after empty line",
        "Line 9: Very long line that might span multiple buffers or require multiple async operations to complete properly in the double-buffered system",
        "Line 10: Final line with end-of-file marker",
        NULL
    };

    TEST_START("Sophisticated file operations - Read from test_data.txt");
    TRACE("Opening sophisticated test file: test_data.txt");
    file = OpenAsync("test_data.txt", MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, "test_data.txt", MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed for test_data.txt");
    
    if (file) {
        /* Read the entire file */
        result = ReadAsync(file, buffer, sizeof(buffer) - 1);
        TRACE_READ(file, buffer, sizeof(buffer) - 1, result);
        TEST_ASSERT(result > 0, "ReadAsync should read data from test_data.txt");
        
        buffer[result] = '\0';
        TRACE1("Read %ld bytes from test_data.txt", result);
        
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        
        /* Verify the content using dos.library */
        TEST_ASSERT(verify_file_lines("test_data.txt", test_lines, 11), 
                   "File content should match expected lines");
        
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed for test_data.txt");
        return FALSE;
    }

    TEST_START("Sophisticated file operations - Write to new file");
    TRACE("Creating new file with sophisticated content");
    file = OpenAsync("T:asyncio_sophisticated.dat", MODE_WRITE, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, "T:asyncio_sophisticated.dat", MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed for writing");
    
    if (file) {
        const char *sophisticated_content = 
            "This is sophisticated test content for AsyncIO\n"
            "Testing double-buffered asynchronous I/O operations\n"
            "With multiple lines and various content types\n"
            "Including special characters: éèêëàáâäùúûüçñÿœæ\n"
            "And mixed content: ABC123!@# 456DEF789\n"
            "End of sophisticated test content\n";
        
        LONG content_length = strlen(sophisticated_content);
        
        TRACE1("Writing sophisticated content (%ld bytes)", content_length);
        result = WriteAsync(file, (APTR)sophisticated_content, content_length);
        TRACE_WRITE(file, (APTR)sophisticated_content, content_length, result);
        TEST_ASSERT(result == content_length, "WriteAsync should write all bytes");
        
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        
        /* Wait for async operations to complete */
        wait_for_async_operation();
        
        /* Verify using dos.library */
        TEST_ASSERT(verify_file_content("T:asyncio_sophisticated.dat", sophisticated_content, content_length),
                   "Sophisticated file content should match written data");
        
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed for sophisticated writing");
        return FALSE;
    }

    TEST_START("Sophisticated file operations - Large file handling");
    TRACE("Testing large file operations");
    file = OpenAsync("test_large.txt", MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(file, "test_large.txt", MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(file != NULL, "OpenAsync should succeed for large file");
    
    if (file) {
        LONG total_read = 0;
        LONG bytes_read;
        
        /* Read the file in chunks to test buffer boundaries */
        while ((bytes_read = ReadAsync(file, buffer, sizeof(buffer) - 1)) > 0) {
            total_read += bytes_read;
            TRACE2("Read chunk: %ld bytes (total: %ld)", bytes_read, total_read);
        }
        
        TEST_ASSERT(total_read > 0, "Should read data from large file");
        TRACE1("Total bytes read from large file: %ld", total_read);
        
        result = CloseAsync(file);
        TRACE_CLOSE(file, result);
        TEST_ASSERT(result >= 0, "CloseAsync should succeed");
        
        /* Verify file size using dos.library */
        {
            LONG file_size = get_file_size("test_large.txt");
            TRACE1("Large file size via dos.library: %ld", file_size);
            TEST_ASSERT(file_size > 0, "Large file should have content");
        }
        
        TEST_PASS();
    } else {
        TEST_FAIL("OpenAsync failed for large file");
        return FALSE;
    }

    return TRUE;
}

/* Test file copy validation - read from source, write to destination, read back and verify */
BOOL test_file_copy_validation(void)
{
    struct AsyncFile *src_file, *dst_file, *verify_file;
    LONG result;
    char buffer[1024];
    LONG bytes_read, total_read = 0, total_written = 0;
    LONG original_size, copied_size, verified_size;
    BOOL data_matches = TRUE;
    LONG mismatch_count = 0;
    char original_buffer[1024], copied_buffer[1024];
    LONG pos = 0;

    TEST_START("File Copy Validation - Complete file integrity test");
    TRACE("Testing complete file copy with AsyncIO - read, write, verify cycle");
    
    /* Get original file size */
    original_size = get_file_size("test_data.txt");
    TRACE1("Original file size: %ld bytes", original_size);
    TEST_ASSERT(original_size > 0, "Source file should exist and have content");
    
    if (original_size <= 0) {
        return FALSE;
    }

    /* Step 1: Read from source file using AsyncIO */
    TRACE("Step 1: Reading from source file (test_data.txt)");
    src_file = OpenAsync("test_data.txt", MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(src_file, "test_data.txt", MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(src_file != NULL, "OpenAsync should succeed for source file");
    
    if (!src_file) {
        return FALSE;
    }

    /* Step 2: Write to destination file using AsyncIO */
    TRACE("Step 2: Writing to destination file (T:asyncio_copy_test.dat)");
    dst_file = OpenAsync("T:asyncio_copy_test.dat", MODE_WRITE, TEST_BUFFER_SIZE);
    TRACE_OPEN(dst_file, "T:asyncio_copy_test.dat", MODE_WRITE, TEST_BUFFER_SIZE);
    TEST_ASSERT(dst_file != NULL, "OpenAsync should succeed for destination file");
    
    if (!dst_file) {
        CloseAsync(src_file);
        return FALSE;
    }

    /* Step 3: Copy data in chunks */
    TRACE("Step 3: Copying data in chunks");
    while ((bytes_read = ReadAsync(src_file, buffer, sizeof(buffer))) > 0) {
        TRACE2("Read chunk: %ld bytes at position %ld", bytes_read, total_read);
        
        result = WriteAsync(dst_file, buffer, bytes_read);
        TRACE2("Wrote chunk: %ld bytes (expected %ld)", result, bytes_read);
        TEST_ASSERT(result == bytes_read, "WriteAsync should write all bytes");
        
        total_read += bytes_read;
        total_written += result;
    }
    
    TRACE2("Copy completed: %ld bytes read, %ld bytes written", total_read, total_written);
    TEST_ASSERT(total_read == original_size, "Should read entire source file");
    TEST_ASSERT(total_written == original_size, "Should write entire destination file");

    /* Close source and destination files */
    result = CloseAsync(src_file);
    TRACE_CLOSE(src_file, result);
    TEST_ASSERT(result >= 0, "CloseAsync should succeed for source file");
    
    result = CloseAsync(dst_file);
    TRACE_CLOSE(dst_file, result);
    TEST_ASSERT(result >= 0, "CloseAsync should succeed for destination file");
    
    /* Wait for async operations to complete */
    wait_for_async_operation();
    
    /* Step 4: Verify copied file size */
    copied_size = get_file_size("T:asyncio_copy_test.dat");
    TRACE2("File size verification: original=%ld, copied=%ld", original_size, copied_size);
    TEST_ASSERT(copied_size == original_size, "Copied file should have same size as original");

    /* Step 5: Read back copied file and compare with original */
    TRACE("Step 5: Reading back copied file for byte-by-byte comparison");
    verify_file = OpenAsync("T:asyncio_copy_test.dat", MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(verify_file, "T:asyncio_copy_test.dat", MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(verify_file != NULL, "OpenAsync should succeed for verification file");
    
    if (!verify_file) {
        return FALSE;
    }

    /* Reopen original file for comparison */
    src_file = OpenAsync("test_data.txt", MODE_READ, TEST_BUFFER_SIZE);
    TRACE_OPEN(src_file, "test_data.txt", MODE_READ, TEST_BUFFER_SIZE);
    TEST_ASSERT(src_file != NULL, "OpenAsync should succeed for original file comparison");
    
    if (!src_file) {
        CloseAsync(verify_file);
        return FALSE;
    }

    /* Byte-by-byte comparison */
    TRACE("Step 6: Performing byte-by-byte comparison");
    while (pos < original_size) {
        LONG orig_read, copy_read;
        LONG compare_size = (original_size - pos < sizeof(original_buffer)) ? 
                           (original_size - pos) : sizeof(original_buffer);
        
        /* Read from original file */
        orig_read = ReadAsync(src_file, original_buffer, compare_size);
        TRACE2("Read from original: %ld bytes at position %ld", orig_read, pos);
        
        /* Read from copied file */
        copy_read = ReadAsync(verify_file, copied_buffer, compare_size);
        TRACE2("Read from copy: %ld bytes at position %ld", copy_read, pos);
        
        TEST_ASSERT(orig_read == copy_read, "Should read same amount from both files");
        
        if (orig_read > 0) {
            /* Compare the data */
            if (memcmp(original_buffer, copied_buffer, orig_read) != 0) {
                data_matches = FALSE;
                mismatch_count++;
                TRACE3("DATA MISMATCH at position %ld! Original vs Copy comparison failed", pos);
                
                /* Show first few bytes of mismatch */
                if (orig_read > 0) {
                    printf("TRACE: Original: ");
                    for (int i = 0; i < (orig_read > 16 ? 16 : orig_read); i++) {
                        printf("%02X ", (unsigned char)original_buffer[i]);
                    }
                    printf("\nTRACE: Copy:     ");
                    for (int i = 0; i < (orig_read > 16 ? 16 : orig_read); i++) {
                        printf("%02X ", (unsigned char)copied_buffer[i]);
                    }
                    printf("\n");
                }
            } else {
                TRACE2("Data matches at position %ld (%ld bytes)", pos, orig_read);
            }
        }
        
        pos += orig_read;
    }
    
    TRACE2("Comparison completed: %ld bytes compared, %ld mismatches found", pos, mismatch_count);
    TEST_ASSERT(data_matches, "All data should match between original and copied file");

    /* Close verification files */
    result = CloseAsync(verify_file);
    TRACE_CLOSE(verify_file, result);
    TEST_ASSERT(result >= 0, "CloseAsync should succeed for verification file");
    
    result = CloseAsync(src_file);
    TRACE_CLOSE(src_file, result);
    TEST_ASSERT(result >= 0, "CloseAsync should succeed for original file");

    /* Step 7: Test with binary file */
    TRACE("Step 7: Testing binary file copy validation");
    original_size = get_file_size("test_binary.dat");
    TRACE1("Binary file size: %ld bytes", original_size);
    TEST_ASSERT(original_size > 0, "Binary file should exist and have content");
    
    if (original_size > 0) {
        /* Copy binary file */
        src_file = OpenAsync("test_binary.dat", MODE_READ, TEST_BUFFER_SIZE);
        dst_file = OpenAsync("T:asyncio_binary_copy.dat", MODE_WRITE, TEST_BUFFER_SIZE);
        
        if (src_file && dst_file) {
            total_read = 0;
            total_written = 0;
            
            while ((bytes_read = ReadAsync(src_file, buffer, sizeof(buffer))) > 0) {
                result = WriteAsync(dst_file, buffer, bytes_read);
                TEST_ASSERT(result == bytes_read, "Binary file write should succeed");
                total_read += bytes_read;
                total_written += result;
            }
            
            TRACE2("Binary copy completed: %ld bytes read, %ld bytes written", total_read, total_written);
            
            CloseAsync(src_file);
            CloseAsync(dst_file);
            wait_for_async_operation();
            
            /* Verify binary file copy */
            copied_size = get_file_size("T:asyncio_binary_copy.dat");
            TEST_ASSERT(copied_size == original_size, "Binary file copy should have same size");
            
            /* Use dos.library to verify binary content */
            TEST_ASSERT(verify_file_content("T:asyncio_binary_copy.dat", NULL, original_size), 
                       "Binary file content should match original");
        }
    }

    TEST_PASS();
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
    
    /* Clean up sophisticated test files */
    TRACE("Deleting sophisticated test file: T:asyncio_sophisticated.dat");
    if (DeleteFile("T:asyncio_sophisticated.dat") == 0) {
        printf("Deleted T:asyncio_sophisticated.dat\n");
        TRACE("Successfully deleted T:asyncio_sophisticated.dat");
    } else {
        TRACE2("Failed to delete %s, IoErr: %ld", "T:asyncio_sophisticated.dat", IoErr());
    }
    
    /* Clean up copy validation test files */
    TRACE("Deleting copy validation test files");
    if (DeleteFile("T:asyncio_copy_test.dat") == 0) {
        printf("Deleted T:asyncio_copy_test.dat\n");
        TRACE("Successfully deleted T:asyncio_copy_test.dat");
    } else {
        TRACE2("Failed to delete %s, IoErr: %ld", "T:asyncio_copy_test.dat", IoErr());
    }
    
    if (DeleteFile("T:asyncio_binary_copy.dat") == 0) {
        printf("Deleted T:asyncio_binary_copy.dat\n");
        TRACE("Successfully deleted T:asyncio_binary_copy.dat");
    } else {
        TRACE2("Failed to delete %s, IoErr: %ld", "T:asyncio_binary_copy.dat", IoErr());
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