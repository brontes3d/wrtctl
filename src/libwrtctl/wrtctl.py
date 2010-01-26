"""
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
 * Philip Lamoreaux <palamoreaux@mmm.com>
 * Justin Bronder <jsbronder@brontes3d.com>
 */
"""

import errno

import _wrtctl
from _wrtctl_const import *

class wrtctl(object):
    """Python class-style wrapper for _wrtctl C functionality."""

    def __init__(self, enable_log=True, verbose=False):
        self.wrtctlObject = _wrtctl.alloc_client(enable_log, verbose)
        self.ctxObject = None

    def start_stunnel_client(self,
            hostname,
            key_path =      DEFAULT_KEY_PATH,
            port =          WRTCTLD_DEFAULT_PORT,
            wrtctlPort =    WRTCTL_SSL_PORT,
            wrtctldPort =   WRTCTL_SSL_PORT ):
        self.ctxObject = _wrtctl.start_stunnel_client(hostname, key_path, port, wrtctlPort, wrtctldPort)

    def create_connection(self,
            hostname,
            port =      None,
            use_ssl =   SSL_ENABLED):
        if use_ssl:
            ssl_port = WRTCTLD_SSL_PORT
            if not port:
                port = WRTCTLD_DEFAULT_PORT
            if port == ssl_port:
                raise OSError(errno.EINVAL,
                    "port cannot be the same as the ssl port (%d)" % (ssl_port,))
            self.start_stunnel_client(hostname, port=port)
            _wrtctl.create_connection(self.wrtctlObject, 'localhost', port)
        else:
            if not port:
                port = WRTCTLD_DEFAULT_PORT
            _wrtctl.create_connection(self.wrtctlObject, hostname, port)

    def queue_net_command(self, commandStr): 
        _wrtctl.queue_net_command(self.wrtctlObject, commandStr)

    def wait_on_response(self, timeoutSec=10, flushSendQueue=True):
        """Return 0=got response, 1=timeout, -n=error."""
        return _wrtctl.wait_on_response(self.wrtctlObject, timeoutSec, flushSendQueue)
    
    def get_net_response(self):
        """Get and return (ID, subsystemStr, valueStr)."""
        return _wrtctl.get_net_response(self.wrtctlObject)

