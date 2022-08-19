/*
* $Id: $
* $Copyright: (c) 2020 Broadcom.
* Broadcom Proprietary and Confidential. All rights reserved.$
*
* $File:  flexport_common.h
*/


#ifndef FLEXPORT_COMMON_H
#define FLEXPORT_COMMON_H


#define EMULATION_FACTOR      10000


typedef int (*phase_callback_type)(int, soc_port_schedule_state_t *);

extern int soc_detach_flex_phase (
    int                  unit,
    phase_callback_type *flex_phases,
    phase_callback_type  phase,
    int                 *phase_pos
    );


extern int soc_attach_flex_phase (
    int                  unit,
    phase_callback_type *flex_phases,
    phase_callback_type  phase,
    int                  phase_pos
    );


extern int soc_check_flex_phase (
    int                  unit,
    phase_callback_type *flex_phases,
    phase_callback_type  phase,
    int                 *present
    );

#endif /* FLEXPORT_COMMON_H */
