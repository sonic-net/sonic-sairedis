/* 
 * $Id: applrefIface.h,v 1.3 Broadcom SDK $
 *
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 */
#ifndef APPLREFIFACE_H
#define APPLREFIFACE_H

/** \struct to invoke a function in applref */
typedef struct ApplRefFunction {
  char *pname;        /**< name of the function */
  char *pbriefdescr;  /**< brief description of the function */
  char *pdescr;       /**< description of the function */
  void *pf;           /**< address of the function */

  /** to keep things symbol, we don't want to implement 
      inferior function call with dummy frame, 
      so a pre-defined dispatch function  is enough to 
      convert char string to correct argument types. */

  int (*dispatch)(void*, int, char *[]); /**< dispatch function */
} ApplRefFunction_t;


#endif
