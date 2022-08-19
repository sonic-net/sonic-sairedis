/** \file dnxc_ha.h
 * General HA routines.
 */
/*
 * $Id: $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 */

#ifndef _DNXC_HA_H
/* { */
#define _DNXC_HA_H

/*
 * Include files
 * {
 */

#include <soc/types.h>
#include <sal/core/alloc.h>
#include <soc/error.h>
#include <assert.h>
#include <sal/core/thread.h>
#include <shared/utilex/utilex_seq.h>

/*
 * }
 */

/*
 * types
 * {
 */

/*
 * }
 */

/*
 * MACROs
 * {
 */

/*
 * }
 */

/**
 * \brief
 *  Init HA utilities module
 * \param [in] unit    - Device Id
 * \return
 *   int - Error Type
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_init(
    int unit);

/**
 * \brief
 *  Deinit HA utilities module
 * \param [in] unit    - Device Id
 * \return
 *   int - Error Type
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_deinit(
    int unit);

/**
 * \brief
 *  Set current thread as init/deinit thread
 *  If thread is not set it is assumed that the main thread is current init/deinit thread
 * \param [in] unit    - Device Id
 * \return
 *   int - Error Type
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_tid_set(
    int unit);

/**
 * \brief
 *  Clear currently set init/deinit thread
 * \param [in] unit    - Device Id
 * \return
 *   int - Error Type
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_tid_clear(
    int unit);

/**
 * \brief
 *  Change status if appl is currently deinitializing.
 *  Used for sanity checking.
 * \param [in] unit    - Device Id
 * \param [in] is_start    - Indicates if beginning or end of region
 * \return
 *   int - Error Type
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_appl_deinit_state_change(
    int unit,
    uint8 is_start);

/**
 * \brief
 *  Query if HW access is disabled
 * \param [in] unit    - Device Id 
 * \param [in] access_type - Access Type
 * \return
 *   int - TRUE if disabled FALSE if enabled.
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_is_access_disabled(
    int unit,
    utilex_seq_allow_access_e access_type);

/**
 * \brief
 *  Temporarily allow HW writes for current thread even if it's
 *  generally disabled
 * \param [in] unit    - Device Id 
 * \param [in] access_type - Access Type
 * \return
 *   int - Error ID.
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_tmp_allow_access_enable(
    int unit,
    utilex_seq_allow_access_e access_type);

/**
 * \brief
 *  disallow HW writes for current thread, to be used tho revert
 *  dnxc_allow_hw_write_enable after done writing
 * \param [in] unit    - Device Id
 * \param [in] access_type - Access Type
 * \return
 *   int - Error ID.
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_tmp_allow_access_disable(
    int unit,
    utilex_seq_allow_access_e access_type);

/* } */

/**
 * \brief
 *  function returns indication if appl is during deinit
 * \param [in] unit    - Device Id
 * \param [out] is_deinit - is appl during deinit
 * \return
 *   int - Error ID.
 * \remark
 *   * None
 * \see
 *   * None
 */
int dnxc_ha_appl_is_deinit(
    int unit,
    uint8 *is_deinit);
#endif /* _DNXC_HA_H */
