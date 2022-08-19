/*
 * $Id: $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * Broadcom System Log (BSL)
 *
 * BSL functions for EXTERNAL (application) usage.
 */

#ifndef _SHR_BSLEXT_H_
#define _SHR_BSLEXT_H_

#include <shared/bsltypes.h>

extern void
bsl_config_t_init(bsl_config_t *config);

extern int
bsl_init(bsl_config_t *config);

#endif /* _SHR_BSLEXT_H_ */

