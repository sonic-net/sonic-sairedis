/**
 * @file MdioTestLib.c
 *
 * Stub MDIO access library for the GearboxMdioAccess unit tests. It exports
 * the two symbols syncd resolves (mdio_read/mdio_write) and records its own
 * load in a marker file, so the tests can observe how often it was dlopened.
 */

#include <stdio.h>

#include <stdint.h>

#include "sai.h"

#ifndef MDIO_TEST_LIB_MARKER
#define MDIO_TEST_LIB_MARKER "/tmp/mdiotestlib.loaded"
#endif

__attribute__((constructor)) static void mdioTestLibMarkLoaded(void)
{
    FILE *f = fopen(MDIO_TEST_LIB_MARKER, "a");

    if (f != NULL)
    {
        fputs("loaded\n", f);
        fclose(f);
    }
}

sai_status_t mdio_read(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _Out_ uint32_t *reg_val)
{
    if (reg_val == NULL)
    {
        return SAI_STATUS_FAILURE;
    }

    *reg_val = 0xA5A5A5A5;

    return SAI_STATUS_SUCCESS;
}

sai_status_t mdio_write(
        _In_ uint64_t platform_context,
        _In_ uint32_t device_addr,
        _In_ uint32_t start_reg_addr,
        _In_ uint32_t number_of_registers,
        _In_ const uint32_t *reg_val)
{
    return SAI_STATUS_SUCCESS;
}
