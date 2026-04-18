/*
 * test_sources.c — compiled as C (not C++) so that C-only constructs work.
 * PlatformIO compiles all files in a test directory into one binary.
 */
#include "../../src/atlogger_atlogger.c"
#include "../../src/atchops_mbedtls.c"
#include "../../src/atchops_base64.c"
#include "../../src/atchops_iv.c"
#include "../../src/atchops_aes_ctr.c"
#include "../../src/atchops_aes.c"
