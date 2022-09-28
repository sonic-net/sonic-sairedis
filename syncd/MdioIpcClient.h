#pragma once

#include <stdint.h>
#include <unistd.h>

extern "C" {
#include "sai.h"
}

/* Function declarations */
extern "C" {
sai_status_t mdio_read(uint64_t platform_context, uint32_t mdio_addr, uint32_t reg_addr,
        uint32_t number_of_registers, uint32_t *data);
sai_status_t mdio_write(uint64_t platform_context, uint32_t mdio_addr,
        uint32_t reg_addr, uint32_t number_of_registers, const uint32_t *data);

sai_status_t mdio_read_cl22(uint64_t platform_context, uint32_t mdio_addr, uint32_t reg_addr,
        uint32_t number_of_registers, uint32_t *data);
sai_status_t mdio_write_cl22(uint64_t platform_context, uint32_t mdio_addr,
        uint32_t reg_addr, uint32_t number_of_registers, const uint32_t *data);
}
