/*
 * test_sources.c — compiled as C (not C++) so that C-only constructs
 * (implicit void* cast, goto bypassing declarations) work correctly.
 * PlatformIO compiles all files in a test directory into one binary, so
 * these objects are linked together with the C++ test runner above.
 */
#include "../../src/atlogger_atlogger.c"
#include "../../src/atchops_base64.c"
