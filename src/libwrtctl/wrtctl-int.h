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
 */

#ifndef __WRTCTL_INT
#define __WRTCTL_INT
#include "wrtctl-net.h"

/* Allocate a d_data pointer, caller must free the data.
 *  Returns a net_errno
 */
int     create_dd( dd_t *dd, int fd );

/* Free a d_data pointer and any contained pointers.  Sets
 * dd to NULL
 */
void    free_dd( dd_t *dd );

/* Sends every packet in a d_data's sendq.  If any packet fails to send,
 * the operation is halted and the error returned.
 */
int flush_sendq( dd_t dd );

/* Wrapper around recv, waits until all data has been read, also attempts to
 * bring in a full struct packet at a time, including associated data.
 */
int recv_packet( dd_t dd );


/* Sets up tpl to report errors to syslog and/or stderr depending on
 * wrtctl_verbose and wrtctl_enable_log
 */
void init_tpl_hook();

/* Module Handling:
 *  Used by wrtctld to allow selection of which commands should be handled.
 */
#define MOD_MAGIC_LEN 4
#define MOD_ERRSTR_LEN 512

typedef struct module_list * mlh_t;

typedef struct mod_data *md_t;

struct mod_data {
    char *  mod_name;
    char *  mod_magic_str;
    int     mod_version;
    void *  mod_ctx;
    void *  dlp;
    int     (*mod_handler)(void*, net_cmd_t, packet_t*);
    char *  mod_errstr;
    STAILQ_ENTRY(mod_data) mod_data_list;
};



char *  load_module     (mlh_t ml, md_t *mdp, char *module_name);
void    unload_module   (mlh_t ml, md_t md);
char *  mod_strerror    (int err);


/* Built in Daemon Module internals */
#define DAEMON_MODVER 1
#define DAEMON_MOD_NAME "daemon-cmds"
#define DAE_CTX_CAST(x,y) ns_t x = (ns_t)y
char mod_errstr[MOD_ERRSTR_LEN];
#endif
