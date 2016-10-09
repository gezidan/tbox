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
 * Copyright (C) 2009 - 2017, ruki All rights reserved.
 *
 * @author      ruki
 * @file        poller_poll.c
 *
 */
/* //////////////////////////////////////////////////////////////////////////////////////
 * includes
 */
#include "prefix.h"
#include "../../container/container.h"
#include "../../algorithm/algorithm.h"
#include <sys/poll.h>
#include <fcntl.h>
#include <errno.h>
#ifndef TB_CONFIG_OS_ANDROID
#   include <sys/unistd.h>
#endif

/* //////////////////////////////////////////////////////////////////////////////////////
 * types
 */

// the poller poll type
typedef struct __tb_poller_poll_t
{
    // the pair sockets for spak, kill ..
    tb_socket_ref_t         pair[2];

    // the poll fds
    tb_vector_ref_t         pfds;

    // the user private data hash (socket => priv)
    tb_cpointer_t*          privhash;

    // the user private data hash size
    tb_size_t               privhash_size;
    
}tb_poller_poll_t, *tb_poller_poll_ref_t;

/* //////////////////////////////////////////////////////////////////////////////////////
 * private implementation
 */
static tb_bool_t tb_poller_walk_remove(tb_iterator_ref_t iterator, tb_cpointer_t item, tb_cpointer_t priv)
{
    // check
    tb_assert(priv);

    // the fd
    tb_long_t fd = (tb_long_t)priv;

    // the poll fd
    struct pollfd* pfd = (struct pollfd*)item;

    // remove it?
    return (pfd && pfd->fd == fd);
}
static tb_bool_t tb_poller_walk_modify(tb_iterator_ref_t iterator, tb_pointer_t item, tb_cpointer_t priv)
{
    // check
    tb_value_ref_t tuple = (tb_value_ref_t)priv;
    tb_assert(tuple);

    // the fd
    tb_long_t fd = tuple[0].l;

    // is this?
    struct pollfd* pfd = (struct pollfd*)item;
    if (pfd && pfd->fd == fd) 
    {
        // the events
        tb_size_t events = tuple[1].ul;

        // modify events
        pfd->events = 0;
        if (events & TB_POLLER_EVENT_RECV) pfd->events |= POLLIN;
        if (events & TB_POLLER_EVENT_SEND) pfd->events |= POLLOUT;

        // break
        return tb_false;
    }

    // ok
    return tb_true;
}
static tb_void_t tb_poller_privhash_set(tb_poller_poll_ref_t poller, tb_socket_ref_t sock, tb_cpointer_t priv)
{
    // check
    tb_assert(poller && sock);

    // the socket fd
    tb_long_t fd = tb_sock2fd(sock);
    tb_assert(fd > 0 && fd < TB_MAXS32);

    // not null?
    if (priv)
    {
        // no privhash? init it first
        tb_size_t need = fd + 1;
        if (!poller->privhash)
        {
            // init privhash
            poller->privhash = tb_nalloc0_type(need, tb_cpointer_t);
            tb_assert_and_check_return(poller->privhash);

            // init privhash size
            poller->privhash_size = need;
        }
        else if (need > poller->privhash_size)
        {
            // grow privhash
            poller->privhash = (tb_cpointer_t*)tb_ralloc(poller->privhash, need * sizeof(tb_cpointer_t));
            tb_assert_and_check_return(poller->privhash);

            // init growed space
            tb_memset(poller->privhash + poller->privhash_size, 0, (need - poller->privhash_size) * sizeof(tb_cpointer_t));

            // grow privhash size
            poller->privhash_size = need;
        }

        // save the user private data
        poller->privhash[fd] = priv;
    }
}
static __tb_inline__ tb_cpointer_t tb_poller_privhash_get(tb_poller_poll_ref_t poller, tb_socket_ref_t sock)
{
    // check
    tb_assert(poller && poller->privhash && sock);

    // the socket fd
    tb_long_t fd = tb_sock2fd(sock);
    tb_assert(fd > 0 && fd < TB_MAXS32);

    // get the user private data
    return fd < poller->privhash_size? poller->privhash[fd] : tb_null;
}
static __tb_inline__ tb_void_t tb_poller_privhash_del(tb_poller_poll_ref_t poller, tb_socket_ref_t sock)
{
    // check
    tb_assert(poller && poller->privhash && sock);

    // the socket fd
    tb_long_t fd = tb_sock2fd(sock);
    tb_assert(fd > 0 && fd < TB_MAXS32);

    // remove the user private data
    if (fd < poller->privhash_size) poller->privhash[fd] = tb_null;
}

/* //////////////////////////////////////////////////////////////////////////////////////
 * implementation
 */
tb_poller_ref_t tb_poller_init(tb_size_t maxn)
{
    // check
    tb_assert_and_check_return_val(maxn, tb_null);

    // done
    tb_bool_t               ok = tb_false;
    tb_poller_poll_ref_t    poller = tb_null;
    do
    {
        // make poller
        poller = tb_malloc0_type(tb_poller_poll_t);
        tb_assert_and_check_break(poller);

        // init poll
        poller->pfds = tb_vector_init(tb_align8((maxn >> 3) + 1), tb_element_mem(sizeof(struct pollfd), tb_null, tb_null));
        tb_assert_and_check_break(poller->pfds);

        // init pair sockets
        if (!tb_socket_pair(TB_SOCKET_TYPE_TCP, poller->pair)) break;

        // insert pair socket first
        if (!tb_poller_insert((tb_poller_ref_t)poller, poller->pair[1], TB_POLLER_EVENT_RECV, tb_null)) break;  

        // ok
        ok = tb_true;

    } while (0);

    // failed?
    if (!ok)
    {
        // exit it
        if (poller) tb_poller_exit((tb_poller_ref_t)poller);
        poller = tb_null;
    }

    // ok?
    return (tb_poller_ref_t)poller;
}
tb_void_t tb_poller_exit(tb_poller_ref_t self)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return(poller);

    // exit pair sockets
    if (poller->pair[0]) tb_socket_exit(poller->pair[0]);
    if (poller->pair[1]) tb_socket_exit(poller->pair[1]);
    poller->pair[0] = tb_null;
    poller->pair[1] = tb_null;

    // exit privhash
    if (poller->privhash) tb_free(poller->privhash);
    poller->privhash        = tb_null;
    poller->privhash_size   = 0;

    // close pfds
    if (poller->pfds) tb_vector_exit(poller->pfds);
    poller->pfds = tb_null;

    // free it
    tb_free(poller);
}
tb_void_t tb_poller_clear(tb_poller_ref_t self)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return(poller);

    // clear pfds
    if (poller->pfds) tb_vector_clear(poller->pfds);

    // spak it
    if (poller->pair[0]) tb_socket_send(poller->pair[0], (tb_byte_t const*)"p", 1);
}
tb_void_t tb_poller_kill(tb_poller_ref_t self)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return(poller);

    // kill it
    if (poller->pair[0]) tb_socket_send(poller->pair[0], (tb_byte_t const*)"k", 1);
}
tb_void_t tb_poller_spak(tb_poller_ref_t self)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return(poller);

    // post it
    if (poller->pair[0]) tb_socket_send(poller->pair[0], (tb_byte_t const*)"p", 1);
}
tb_bool_t tb_poller_support(tb_poller_ref_t self, tb_size_t events)
{
    // all supported events 
    tb_size_t events_supported = TB_POLLER_EVENT_ALL;

    // is supported?
    return (events_supported & events) == events;
}
tb_bool_t tb_poller_insert(tb_poller_ref_t self, tb_socket_ref_t sock, tb_size_t events, tb_cpointer_t priv)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return_val(poller && poller->pfds && sock, tb_false);

    // init events
    struct pollfd pfd = {0};
    if (events & TB_POLLER_EVENT_RECV) pfd.events |= POLLIN;
    if (events & TB_POLLER_EVENT_SEND) pfd.events |= POLLOUT;

    // save fd, TODO uses binary search
    pfd.fd = tb_sock2fd(sock);
    tb_vector_insert_tail(poller->pfds, &pfd);

    // bind user private data to socket
    tb_poller_privhash_set(poller, sock, priv);

    // spak it
    if (poller->pair[0] && events) tb_socket_send(poller->pair[0], (tb_byte_t const*)"p", 1);

    // ok
    return tb_true;
}
tb_bool_t tb_poller_remove(tb_poller_ref_t self, tb_socket_ref_t sock)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return_val(poller && poller->pfds && sock, tb_false);

    // remove this socket and events, TODO uses binary search
    tb_remove_first_if(poller->pfds, tb_poller_walk_remove, (tb_cpointer_t)(tb_long_t)tb_sock2fd(sock));

    // remove user private data from this socket
    tb_poller_privhash_del(poller, sock);

    // spak it
    if (poller->pair[0]) tb_socket_send(poller->pair[0], (tb_byte_t const*)"p", 1);

    // ok
    return tb_true;
}
tb_bool_t tb_poller_modify(tb_poller_ref_t self, tb_socket_ref_t sock, tb_size_t events, tb_cpointer_t priv)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return_val(poller && poller->pfds && sock, tb_false);

    // modify events, TODO uses binary search
    tb_value_t tuple[2];
    tuple[0].l       = tb_sock2fd(sock);
    tuple[1].ul      = events;
    tb_walk_all(poller->pfds, tb_poller_walk_modify, tuple);

    // modify user private data to socket
    tb_poller_privhash_set(poller, sock, priv);

    // spak it
    if (poller->pair[0] && events) tb_socket_send(poller->pair[0], (tb_byte_t const*)"p", 1);

    // ok
    return tb_true;
}
tb_long_t tb_poller_wait(tb_poller_ref_t self, tb_poller_event_func_t func, tb_long_t timeout)
{
    // check
    tb_poller_poll_ref_t poller = (tb_poller_poll_ref_t)self;
    tb_assert_and_check_return_val(poller && poller->pfds && func, -1);

    // loop
    tb_long_t wait = 0;
    tb_bool_t stop = tb_false;
    tb_hong_t time = tb_mclock();
    while (!wait && !stop && (timeout < 0 || tb_mclock() < time + timeout))
    {
        // pfds
        struct pollfd*  pfds = (struct pollfd*)tb_vector_data(poller->pfds);
        tb_size_t       pfdm = tb_vector_size(poller->pfds);
        tb_assert_and_check_return_val(pfds && pfdm, -1);

        // wait
        tb_long_t cfdn = poll(pfds, pfdm, timeout);
        tb_assert_and_check_return_val(cfdn >= 0, -1);

        // timeout?
        tb_check_return_val(cfdn, 0);

        // sync
        tb_size_t i = 0;
        for (i = 0; i < pfdm; i++)
        {
            // the sock
            tb_socket_ref_t sock = tb_fd2sock(pfds[i].fd);
            tb_assert_and_check_return_val(sock, -1);

            // the poll events
            tb_size_t poll_events = pfds[i].revents;
            tb_check_continue(poll_events);

            // spak?
            if (sock == poller->pair[1] && (poll_events & POLLIN))
            {
                // read spak
                tb_char_t spak = '\0';
                if (1 != tb_socket_recv(poller->pair[1], (tb_byte_t*)&spak, 1)) return -1;

                // killed?
                if (spak == 'k') return -1;

                // stop to wait
                stop = tb_true;

                // continue it
                continue ;
            }

            // skip spak
            tb_check_continue(sock != poller->pair[1]);

            // init events
            tb_size_t events = TB_POLLER_EVENT_NONE;
            if (poll_events & POLLIN) events |= TB_POLLER_EVENT_RECV;
            if (poll_events & POLLOUT) events |= TB_POLLER_EVENT_SEND;
            if ((poll_events & POLLHUP) && !(events & (TB_POLLER_EVENT_RECV | TB_POLLER_EVENT_SEND))) 
                events |= TB_POLLER_EVENT_RECV | TB_POLLER_EVENT_SEND;

            // call event function
            func(sock, events, tb_poller_privhash_get(poller, sock));
        }
    }

    // ok
    return wait;
}
