/*!The Tiny Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2011, ruki All rights reserved.
 *
 * \author		ruki
 * \file		strncpy.c
 *
 */

/* /////////////////////////////////////////////////////////
 * includes
 */
#include "prefix.h"

#if defined(TB_CONFIG_ARCH_x86)
# 	include "opt/x86/strncpy.c"
#elif defined(TB_CONFIG_ARCH_ARM)
# 	include "opt/arm/strncpy.c"
#elif defined(TB_CONFIG_ARCH_SH4)
# 	include "opt/sh4/strncpy.c"
#endif

/* /////////////////////////////////////////////////////////
 * interfaces 
 */

#ifndef TB_LIBC_STRING_OPT_STRNCPY
tb_char_t* tb_strncpy(tb_char_t* s1, tb_char_t const* s2, tb_size_t n)
{
	TB_ASSERT_RETURN_VAL(s1 && s2, TB_NULL);

	__tb_register__ tb_char_t* s = s1;

	while (n) 
	{
		if ((*s = *s2) != 0) s2++; // Need to fill tail with 0s.
		++s;
		--n;
	}

	return s;
}
#endif
