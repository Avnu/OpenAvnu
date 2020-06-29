 /*
  * Copyright (c) <2013>, Intel Corporation.
  *
  * This program is free software; you can redistribute it and/or modify it
  * under the terms and conditions of the GNU Lesser General Public License,
  * version 2.1, as published by the Free Software Foundation.
  *
  * This program is distributed in the hope it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
  * more details.
  *
  * You should have received a copy of the GNU Lesser General Public License along with
  * this program; if not, write to the Free Software Foundation, Inc.,
  * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  *
 */

#ifndef __AVB_H__
#define __AVB_H__

#include "avb_gptp.h"
#include "avb_srp.h"
#include "avb_avtp.h"
#ifndef AVB_FEATURE_IGB
/* IGB has not been disabled, so assume it is enabled. */
#define AVB_FEATURE_IGB 1
#include "avb_igb.h"
#endif
#ifndef AVB_FEATURE_ATL
/* IGB has not been disabled, so assume it is enabled. */
#define AVB_FEATURE_ATL 1
#include "avb_atl.h"
#endif

#endif		/*  __AVB_H__ */
