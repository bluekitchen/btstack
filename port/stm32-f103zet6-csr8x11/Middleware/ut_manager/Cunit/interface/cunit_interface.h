/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  Copyright (C) 2004  Anil Kumar, Jerry St.Clair
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CUNIT_INTERFACE_H_
#define CUNIT_INTERFACE_H_
#include "CUnit.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cunit_test_case_func_t)(void);        /**< Signature for a testing function in a test case. */

typedef enum {
	CUNIT_SUCCESS = 0,
	CUNIT_FAIL = 1
} cunit_status_t;

cunit_status_t cunit_init(void);

void cunit_deinit(void);

cunit_status_t cunit_add_test_case(const char *case_name, cunit_test_case_func_t test_case);

cunit_status_t cunit_test_start(void);

#ifdef __cplusplus
}
#endif

#endif  /* CUNIT_INTERFACE_H_ */

