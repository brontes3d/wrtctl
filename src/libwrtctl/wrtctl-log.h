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

#ifndef __WRTCTL_LOG_H
#define __WRTCTL_LOG_H
#include <stdbool.h>
extern bool wrtctl_verbose;
extern bool wrtctl_enable_log;

#define info(str...) \
    if ( wrtctl_verbose ) { \
        if ( wrtctl_enable_log )  syslog(LOG_INFO, str); \
        printf(str); \
    }
    
#define log(str...) \
    if ( wrtctl_enable_log )  syslog(LOG_NOTICE, str); \
    if ( wrtctl_verbose ) printf(str);
    
#define err(str...) \
    if ( wrtctl_enable_log ) syslog(LOG_ERR, str); \
    if ( wrtctl_verbose ) fprintf(stderr, __func__);fprintf(stderr,": " str);

#define err_rc(rc, str...) \
    if ( rc != 0 ) err(str);

#endif
