# AsyncIO Library Unit Tests

This directory contains comprehensive unit tests for the asyncio.library.

## Build Configuration

The unit tests support two build configurations:

### Shared Library Version (`test_asyncio_shared`)
- Links against the shared asyncio.library
- Uses register-based calling conventions
- Compiled with `ASIO_SHARED_LIB` define
- Tests the library through the standard Amiga library interface

### Static Library Version (`test_asyncio_static`)
- Links against the static asyncio.lib
- Uses stack-based calling conventions
- Compiled without `ASIO_SHARED_LIB` define
- Tests the library functions directly

## Building

To build both versions:
```bash
smake all
```

To build only the shared library version:
```bash
smake test_asyncio_shared
```

To build only the static library version:
```bash
smake test_asyncio_static
```

## Running Tests

To run both test versions:
```bash
smake test
```

Or manually:
```bash
./run_tests
```

## Test Coverage

The tests cover all functions described in asyncio.doc:

- **File Operations**: OpenAsync, CloseAsync, OpenAsyncFromFH
- **Read Operations**: ReadAsync, ReadCharAsync, ReadLineAsync, PeekAsync
- **Write Operations**: WriteAsync, WriteCharAsync, WriteLineAsync
- **Seek Operations**: SeekAsync
- **Line Operations**: FGetsAsync, FGetsLenAsync
- **Error Handling**: Various error conditions and edge cases

## Test Data

Tests use Latin-1 compatible test data with short lines to ensure compatibility with the Amiga environment.

## Output

Both test versions produce identical results when the library functions work correctly. Any differences between shared and static versions indicate potential issues with the calling conventions or linking. 