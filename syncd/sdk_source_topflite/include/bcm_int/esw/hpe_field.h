/*
 * $Id: hpe_field.h$
 * $Copyright: (c) 2020 Hewlett Packard Enterprise Development LP
 * <br> The contents of this software are proprietary and confidential
 * to Hewlett Packard Enterprise Development LP.  No part of this
 * program may be photocopied, reproduced, or translated into another
 * programming language without prior written consent of
 * Hewlett Packard Enterprise Development LP.$
 *
 * File:        hpe_field.h
 * Purpose:     HPE added data structures and functions to the Internal Field
 *              Processor data structures and definitions for the BCM library.
 */

#ifndef _HPE_FIELD_H
#define _HPE_FIELD_H

/*
 * Typedef:
 *     hpe_field_group_resources_t
 * Purpose:
 *     Field group resource utilization
 */
typedef struct hpe_field_group_resources_s {
    int reserved; /* Number of Field entries reserved */
} hpe_field_group_resources_t;

extern int
hpe_field_group_resources_get(int unit, bcm_field_group_t gid,
        hpe_field_group_resources_t *resources);

#endif
