/*!The Treasure Box Library
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
 * Copyright (C) 2009 - 2015, ruki All rights reserved.
 *
 * @author		ruki
 * @file		count.c
 * @ingroup 	algorithm
 *
 */

/* //////////////////////////////////////////////////////////////////////////////////////
 * includes
 */
#include "count.h"
#include "for.h"

/* //////////////////////////////////////////////////////////////////////////////////////
 * implementation
 */
tb_size_t tb_count(tb_iterator_t* iterator, tb_size_t head, tb_size_t tail, tb_cpointer_t data, tb_iterator_comp_t comp)
{
	// check
	tb_assert_and_check_return_val(iterator && (tb_iterator_mode(iterator) & TB_ITERATOR_MODE_FORWARD), tail);

	// null?
	tb_check_return_val(head != tail, tail);

	// the comparer
	if (!comp) comp = tb_iterator_comp;

	// count
	tb_size_t count = 0;
	tb_for (tb_pointer_t, item, head, tail, iterator) 
		if (!comp(iterator, item, data)) count++;

	// ok?
	return count;
} 
tb_size_t tb_count_all(tb_iterator_t* iterator, tb_cpointer_t data, tb_iterator_comp_t comp)
{
	return tb_count(iterator, tb_iterator_head(iterator), tb_iterator_tail(iterator), data, comp);
}
