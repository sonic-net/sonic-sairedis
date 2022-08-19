/*
 * $Id: spi.h,v 1.1.2.1 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File:        spi.h
 * Purpose:     Base definitions for SPI routines
 * Requires:    
 */

#ifndef _SOC_SPI_H
#define _SOC_SPI_H


#include <soc/defs.h>
#include <soc/field.h>


/* SOC SPI Routines */
extern int soc_spi_read(int unit, uint32 addr, uint8 *buf, int len);
extern int soc_spi_write(int unit, uint32 addr, uint8 *buf, int len);


#endif	/* !_SOC_SPI_H */
