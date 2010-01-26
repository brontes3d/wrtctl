/*
 * Copyright (c) 2009, 3M
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the 3M nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Justin Bronder <jsbronder@brontes3d.com>
 *
 */

#include <Python.h>
#include <uci.h>

#include <config.h>
#include <wrtctl-net.h>

typedef struct const_map {
    const char *name;
    int val;
    const char *str;
} * const_map_t;

static struct const_map constants_map [] = {
/* UCI Errors from <uci.h> */
    { "UCI_OK",                 UCI_OK,             NULL },
    { "UCI_ERR_MEM",            UCI_ERR_MEM,        NULL },
    { "UCI_ERR_INVAL",          UCI_ERR_INVAL,      NULL },
    { "UCI_ERR_NOTFOUND",       UCI_ERR_NOTFOUND,   NULL },
    { "UCI_ERR_IO",             UCI_ERR_IO,         NULL },
    { "UCI_ERR_PARSE",          UCI_ERR_PARSE,      NULL },
    { "UCI_ERR_DUPLICATE",      UCI_ERR_DUPLICATE,  NULL },
    { "UCI_ERR_UNKNOWN",        UCI_ERR_UNKNOWN,    NULL },
    { "UCI_ERR_LAST",           UCI_ERR_LAST,       NULL },

/* Wrtctl Errors from <wrtctl-net.h> */
    { "NET_OK",                 NET_OK,             NULL },
    { "NET_ERR_MEM",            NET_ERR_MEM,        NULL },
    { "NET_ERR_FD",             NET_ERR_FD,         NULL },
    { "NET_ERR_INVAL",          NET_ERR_INVAL,      NULL },
    { "NET_ERR_CONNRESET",      NET_ERR_CONNRESET,  NULL },
    { "NET_ERR_PKTSZ",          NET_ERR_PKTSZ,      NULL },
    { "NET_ERR_NS",             NET_ERR_NS,         NULL },
    { "NET_ERR_TIMEOUT",        NET_ERR_TIMEOUT,    NULL },
    { "NET_ERR_TPL",            NET_ERR_TPL,        NULL },
    { "NET_ERR",                NET_ERR,            NULL },

/* Various Defaults */
    { "DEFAULT_KEY_PATH",       -1,                 DEFAULT_KEY_PATH },
    { "WRTCTLD_DEFAULT_PORT",   -1,                 WRTCTLD_DEFAULT_PORT },
    { "WRTCTLD_SSL_PORT",       -1,                 WRTCTLD_SSL_PORT },
    { "WRTCTL_SSL_PORT",        -1,                 WRTCTL_SSL_PORT },
    { NULL,                     -1,                 NULL },
};

PyMODINIT_FUNC init_wrtctl_const( void ){
    PyObject *module = NULL;
    const_map_t emi;

    if ( !(module = Py_InitModule("_wrtctl_const", NULL)) )
        return;

    for ( emi = constants_map; emi->name; emi++ ){
        if ( emi->val != -1 )
            PyModule_AddIntConstant(module, emi->name, emi->val);
        else
            PyModule_AddStringConstant(module, emi->name, emi->str);
    }
    return;
}

