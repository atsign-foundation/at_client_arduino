/*
 * test_sources.c — compiled as C (not C++) so that C-only constructs work.
 * PlatformIO compiles all files in a test directory into one binary.
 */
#include "../../src/atlogger_atlogger.c"
#include "../../src/atclient_socket.c"
#include "../../src/atclient_socket_mbedtls.c"
#include "../../src/atclient_atdirectory.c"
