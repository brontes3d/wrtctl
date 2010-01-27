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
 *
 */

#include <Python.h>

#include <config.h>
#include <wrtctl-net.h>


static void * validObjectPointer( PyObject *pync ){
    void * p = NULL;

    if ( !PyCObject_Check(pync)
            || !(p = PyCObject_AsVoidPtr(pync)) ){
        PyErr_Format(PyExc_ValueError, "Invalid object pointer received.");
        return NULL;
    }
    return p;
}



static void deletenc(void *vptr) {
    if (vptr)
        free(vptr);
}

static PyObject* Py_alloc_client( PyObject *obj, PyObject *args ){    
    nc_t nc;
    int rc;
    
    if ( (rc = alloc_client(&nc, true, false)) != NET_OK ){
        char *errmsg = NULL;
        errno = EIO;
        if ( asprintf(&errmsg, "alloc_client() failed with error %d(%s)",
                rc, net_strerror(rc)) != -1 ){
            PyErr_SetFromErrnoWithFilename(PyExc_IOError, errmsg);
            free(errmsg);
        } else {
            PyErr_SetFromErrno(PyExc_IOError);
        }
        return NULL;
    }
    return PyCObject_FromVoidPtr((void*)nc, deletenc);
}


#ifdef ENABLE_STUNNEL

static void deletectx( void *vptr ){
    if (vptr)
        kill_stunnel( (stunnel_ctx_t *)&vptr );
}

static PyObject* Py_start_stunnel_client(PyObject *obj, PyObject *args) {    
    int rc;
    char * hostname     = NULL;
    char * key_path     = DEFAULT_KEY_PATH;
    char * port         = WRTCTLD_DEFAULT_PORT;
    char * wrtctl_port  = WRTCTL_SSL_PORT;
    char * wrtctld_port = WRTCTLD_SSL_PORT;
    stunnel_ctx_t ctx   = NULL;
    
    if ( !PyArg_ParseTuple(
            args,
            "s|ssss",
            &hostname,
            &key_path,
            &port,
            &wrtctl_port,
            &wrtctld_port) ){
        return NULL;
    }
    if ( (rc = start_stunnel_client(
            &ctx,
            hostname,
            key_path,
            port,
            wrtctl_port,
            wrtctld_port)) != 0 ){
        
        char *errmsg = NULL;
        errno = ENOMEM;
        if ( asprintf(&errmsg, 
                    "start_stunnel_client('%s', '%s', '%s', '%s', '%s') failed with error %d(%s)",
                    hostname, key_path, port, wrtctl_port, wrtctld_port, rc, net_strerror(rc) ) 
                    != -1 ) {
            PyErr_SetFromErrnoWithFilename(PyExc_OSError, errmsg);
            free(errmsg);
        } else {
            PyErr_SetFromErrno(PyExc_OSError);
        }
        return NULL;
    }
    return PyCObject_FromVoidPtr((void*)ctx, deletectx);
}


static PyObject * Py_kill_stunnel( PyObject *obj, PyObject *args ){
    PyObject * pyctx    = NULL;
    stunnel_ctx_t ctx   = NULL;

    if ( !PyArg_ParseTuple(args, "O", &pyctx) )
        return NULL;
    if ( !(ctx = (stunnel_ctx_t)validObjectPointer(pyctx)) )
        return NULL;

    kill_stunnel( &ctx );
    Py_RETURN_NONE;
}

#else
static PyObject* Py_start_stunnel_client(PyObject *obj, PyObject *args) {
    Py_RETURN_NONE;
}

static PyObject* Py_kill_stunnel(PyObject *obj, PyObject *args) {
    Py_RETURN_NONE;
}

#endif

static PyObject* Py_create_connection( PyObject *obj, PyObject *args ){
    PyObject * pync = NULL;
    char * hostname = NULL;
    char * port     = WRTCTLD_DEFAULT_PORT;
    nc_t nc         = NULL;
    int rc;

    if ( !PyArg_ParseTuple(args, "Os|s", &pync, &hostname, &port) )
        return NULL;
    if ( !(nc = (nc_t)validObjectPointer(pync)) )
        return NULL;
    if ( (rc = create_conn(nc, hostname, port)) != NET_OK ){
        char *errmsg = NULL;
        errno = EIO;

        if ( asprintf(&errmsg, 
                "create_conn('%s', '%s') failed with error %d(%s)",
                hostname, port, rc, net_strerror(rc) ) != -1 ){
            PyErr_SetFromErrnoWithFilename(PyExc_IOError, errmsg);
            free(errmsg);
        } else {
            PyErr_SetFromErrno(PyExc_IOError);
        }
       return NULL;
    }
    Py_RETURN_NONE;
}


static PyObject* Py_queue_net_command( PyObject *obj, PyObject *args ){
    PyObject *pync      = NULL;
    char *cmd_str       = NULL;
    nc_t nc             = NULL;
    packet_t sp         = NULL;
    int rc;
 
    if ( !PyArg_ParseTuple(args, "Os", &pync, &cmd_str) )
        return NULL;
    if ( !(nc = (nc_t)validObjectPointer(pync)) )
        return NULL;

    if ( (rc = line_to_packet(cmd_str, &sp)) != NET_OK ){
        char *errmsg = NULL;
        errno = EINVAL;

        if ( asprintf(&errmsg,
                "line_to_packet() failed with error %d(%s)", 
                rc, strerror(rc)) != -1 ){
            PyErr_SetFromErrnoWithFilename(PyExc_IOError, errmsg);
            free(errmsg);
        } else {
            PyErr_SetFromErrno(PyExc_IOError);
        }
        return NULL;
    }
    nc_add_packet(nc, sp);
    Py_RETURN_NONE;
}


static PyObject* Py_wait_on_response(PyObject *obj, PyObject *args){
    PyObject *pync          = NULL;
    int timeoutSec          = 0;
    int flushSendQueue      = 1;
    struct timeval timeout  = { 0, 0 };
    nc_t nc                 = NULL;
    int rc; 

    if ( !PyArg_ParseTuple(args, "O|ii", &pync, &timeoutSec, &flushSendQueue) )
        return NULL;
    if ( !(nc = (nc_t)validObjectPointer(pync)) )
        return NULL;

    timeout.tv_sec = timeoutSec;
    rc = wait_on_response(nc, &timeout, flushSendQueue);
    if ( rc != NET_OK && rc != NET_ERR_TIMEOUT ){
        char *errmsg = NULL;
        errno = EIO;

        if ( asprintf(&errmsg,
                "wait_on_response(%d) failed with error %d(%s)",
                timeoutSec, rc, net_strerror(rc) ) != -1 ){
            PyErr_SetFromErrnoWithFilename(PyExc_IOError, errmsg);
            free(errmsg);
        } else {
            PyErr_SetFromErrno(PyExc_IOError);
        }
        return NULL;
    }
    return Py_BuildValue("i", rc);    
}


static PyObject* Py_get_net_response(PyObject *obj, PyObject *args){
    PyObject *pync  = NULL;
    PyObject *rv    = NULL;
    nc_t nc         = NULL;
    packet_t rp     = NULL;
    struct net_cmd ncmd;
    int rc;

    memset(&ncmd, 0, sizeof(struct net_cmd));

    if ( !PyArg_ParseTuple(args, "O", &pync) )
        return NULL;

    if ( !(nc = (nc_t)validObjectPointer(pync)) )
        return NULL;

    if ( !(rp = STAILQ_FIRST(&(nc->dd->recvq))) ){
        char *errmsg;
        errno = ENOMSG;

        if ( asprintf(&errmsg, 
                "No response from %s.",
                nc->dd->host) != -1 ){
            PyErr_SetFromErrnoWithFilename(PyExc_IOError, errmsg);
            free(errmsg);
        } else {
            PyErr_SetFromErrno(PyExc_IOError);
        }
        return NULL;
    }
    
    if ( (rc = unpack_net_cmd_packet(&ncmd, rp)) != NET_OK ){
        char *errmsg;
        errno = ENOMEM;
        if ( asprintf(&errmsg, 
                "unpack_net_cmd_packet: %s.",
                net_strerror(rc)) != -1 ){
            PyErr_SetFromErrnoWithFilename(PyExc_IOError, errmsg);
            free(errmsg);
        } else {
            PyErr_SetFromErrno(PyExc_IOError);
        }
        return NULL;
    }
    
    rv = Py_BuildValue("(iss)", ncmd.id, ncmd.subsystem, ncmd.value);
    free_net_cmd_strs(ncmd);
    STAILQ_REMOVE( &(nc->dd->recvq), rp, packet, packet_queue);
    free_packet(rp);
    
    return rv;
}


static int setDictItem( PyObject* dict, char *key, char *val, int ival ){
    PyObject *oVal  = NULL;

    if (val){
        if ( !(oVal = PyString_FromString(val)) ){
            errno = EINVAL;
            PyErr_SetFromErrnoWithFilename(PyExc_OSError,
                "setDictItem:  Failed to create string.");
            return -1;
        }
    } else {
        if ( !(oVal = PyInt_FromLong(ival)) ){
            errno = EINVAL;
            PyErr_SetFromErrnoWithFilename(PyExc_OSError,
                "setDictItem:  Failed to create long.");
            return -2;
        }
    }

    if ( PyDict_SetItemString(dict, key, oVal) ){
        PyErr_SetFromErrnoWithFilename(PyExc_OSError,
            "setDictItem:  Failed to set dictionary item.");
        return -3;
    }
    return 0;
}


static void cleanup_wrtctl() {
}


static PyMethodDef _wrtctl_funcs[] = {
        { "alloc_client",
            Py_alloc_client,        METH_NOARGS,
            "wco = _wrtctl.alloc_client()"
        },

        { "create_connection",
            Py_create_connection,   METH_VARARGS,
            "_wrtctl.create_connection(wco, hostname, port='" \
            WRTCTLD_DEFAULT_PORT"')"
        },

        { "queue_net_command",
            Py_queue_net_command,   METH_VARARGS,
            "_wrtctl.queue_net_command(wco, idInt, subsystemStr, valueStr='')"
        },

        { "wait_on_response",
            Py_wait_on_response,    METH_VARARGS,
            "_wrtctl.wait_on_response(wco, timeoutSec=0, flushSendQueue=True)"
        },

        { "get_net_response",
            Py_get_net_response,    METH_VARARGS,
            "(id, subsystemStr, valueStr) = _wrtctl.get_net_response(wco)"
        },

        { "start_stunnel_client",
            Py_start_stunnel_client, METH_VARARGS,
            "ctxobj = _wrtctl.start_stunnel_client(hostname, key_path='" \
            DEFAULT_KEY_PATH"', port='"WRTCTLD_DEFAULT_PORT"', wrtctlPort='" \
            WRTCTL_SSL_PORT"', wrtctld_port='"WRTCTLD_SSL_PORT"')" 
        },

        { "kill_stunnel",
            Py_kill_stunnel,    METH_VARARGS,
            "_wrtctl.kill_stunnel( ctxobj )"
        },

        { NULL, NULL }
};


PyMODINIT_FUNC init_wrtctl( void ) {
    Py_InitModule3("_wrtctl", _wrtctl_funcs, "OpenWRT Control");
    Py_AtExit(cleanup_wrtctl); // register a cleanup function at exit
}
