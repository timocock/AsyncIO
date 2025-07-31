# AsyncIO Library Unit Tests

This directory contains comprehensive unit tests for the asyncio.library that exercise all functions described in the asyncio.doc documentation.

## Test Coverage

The test suite covers all the following functions:

### File Operations
- `OpenAsync()` - Opening files in read, write, and append modes
- `OpenAsyncFromFH()` - Opening from existing DOS file handles
- `CloseAsync()` - Closing files and cleanup

### Reading Operations
- `ReadAsync()` - Reading blocks of data
- `ReadCharAsync()` - Reading individual characters
- `ReadLineAsync()` - Reading complete lines
- `FGetsAsync()` - Reading lines (fgets-style)
- `FGetsLenAsync()` - Reading lines with length tracking
- `PeekAsync()` - Reading without advancing file pointer

### Writing Operations
- `WriteAsync()` - Writing blocks of data
- `WriteCharAsync()` - Writing individual characters
- `WriteLineAsync()` - Writing complete lines

### File Positioning
- `SeekAsync()` - Setting file position with various modes

### Error Handling
- Invalid file operations
- Non-existent files
- Mode violations (read on write-only, write on read-only)
- NULL parameter handling

## Building the Tests

To build the unit test executable:

```
cd unittests
smake
```

This will create `test_asyncio` executable.

## Running the Tests

To run the tests:

```
cd unittests
smake test
```

Or using the Amiga shell script:

```
run_tests
```

Or directly:

```
test_asyncio
``
```

## Test Output

The test suite provides detailed output including:

- Individual test results with PASS/FAIL status
- Assertion details for each test
- Comprehensive test summary with success rate
- Automatic cleanup of test files

## Test Files

The tests create temporary files in the `T:` directory:
- `T:asyncio_test.dat` - Main test file for various operations
- `T:asyncio_test2.dat` - Secondary test file for character operations

These files are automatically cleaned up after the tests complete.

## Test Structure

The test suite is organized into logical test groups:

1. **Open/Close Tests** - Basic file opening and closing
2. **Write Operations** - Testing WriteAsync and WriteCharAsync
3. **Read Operations** - Testing ReadAsync and ReadCharAsync
4. **Seek Operations** - Testing SeekAsync with different modes
5. **Peek Operations** - Testing PeekAsync functionality
6. **Line Operations** - Testing line-based reading and writing
7. **Character Operations** - Testing character-by-character I/O
8. **Error Handling** - Testing error conditions and edge cases
9. **File Handle Operations** - Testing OpenAsyncFromFH

## Requirements

- SAS/C compiler (sc)
- SAS/C linker (slink)
- asyncio.library (built from parent directory)
- Amiga OS 2.0+ with dos.library and utility.library
- Amiga shell (for running test scripts)

## Notes

- The tests use a buffer size of 8192 bytes as recommended in the documentation
- All tests include comprehensive trace output as per project requirements
- The test suite follows C89 standards and Amiga coding conventions
- Error codes are checked using dos.library/IoErr() where appropriate 