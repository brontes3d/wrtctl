//////////////////////////////////////////////////////////////////////////
//
// File: wrtctl_py.c
//
// Description: python interface layer for _wrtctl
//
// Current Author: Justin Bronder
//
// Copyright 2009 Brontes Technologies, Inc.
//
//////////////////////////////////////////////////////////////////////////

#include <Python.h>

#include <config.h>
#include <wrtctl-net.h>


static nc_t validObjectPointer( PyObject *pync ){
    nc_t nc;
    if ( !PyCObject_Check(pync) \
            || !(nc = (nc_t)PyCObject_AsVoidPtr(pync)) ){
        PyErr_Format(PyExc_ValueError, "Invalid nc_t object pointer received.");
        return NULL;
    }
    return nc;
}



static void deletenc(void *vptr) {
    if (vptr)
        free(vptr);
}

static PyObject* Py_alloc_client( PyObject *obj, PyObject *args ){    
    bool enable_log = true;
    bool verbose    = false;
    nc_t nc;
    int rc;
    
    if ( !PyArg_ParseTuple(args, "|ii", &enable_log, &verbose) )
        return NULL;
    if ( (rc = alloc_client(&nc, enable_log, verbose)) != NET_OK ){
        PyErr_Format(PyExc_EnvironmentError, 
            "alloc_client() failed with error %d(%s)",
            rc, net_strerror(rc) );
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

        PyErr_Format(
            PyExc_EnvironmentError,
            "start_stunnel_client('%s', '%s', '%s', '%s', '%s') failed with error %d(%s)",
            hostname, key_path, port, wrtctl_port, wrtctld_port, rc, net_strerror(rc) );
        return NULL;
    }
    return PyCObject_FromVoidPtr((void*)ctx, deletectx);
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
    if ( !(nc = validObjectPointer(pync)) )
        return NULL;
    if ( (rc = create_conn(nc, hostname, port)) != NET_OK ){
        PyErr_Format(
            PyExc_EnvironmentError,
            "create_conn('%s', '%s') failed with error %d(%s)",
            hostname, port, rc, net_strerror(rc) );
        return NULL;
    }
    Py_RETURN_NONE;
}


static PyObject* Py_queue_net_command( PyObject *obj, PyObject *args ){
    PyObject *pync      = NULL;
    int id;
    char *subsystem     = NULL;
    char *value         = "";
    nc_t nc             = NULL;
    packet_t sp         = NULL;
    int rc;
 
    if ( !PyArg_ParseTuple(args, "Ois|s", &pync, &id, &subsystem, &value) )
        return NULL;
    if ( !(nc = validObjectPointer(pync)) )
        return NULL;
    if ( (rc = create_net_cmd_packet(&sp, id, subsystem, value)) != NET_OK ){
        PyErr_Format(
            PyExc_EnvironmentError,
            "create_net_cmd_packet(%d, '%s', '%s') failed with error %d(%s)", 
            id, subsystem, value, rc, net_strerror(rc)) ;
        return NULL;
    }
    nc_add_packet(nc, sp);
    Py_RETURN_NONE;
}


static PyObject* Py_wait_on_response(PyObject *obj, PyObject *args){
    PyObject *pync          = NULL;
    int timeoutSec          = 0;
    bool flushSendQueue     = true;
    struct timeval timeout  = { 0, 0 };
    nc_t nc                 = NULL;
    int rc; 

    if ( !PyArg_ParseTuple(args, "O|ii", &pync, &timeoutSec, &flushSendQueue) )
        return NULL;
    if ( !(nc = validObjectPointer(pync)) )
        return NULL;

    timeout.tv_sec = timeoutSec;
    rc = wait_on_response(nc, &timeout, flushSendQueue);
    if ( rc != NET_OK && rc != NET_ERR_TIMEOUT ){
        PyErr_Format(
            PyExc_EnvironmentError,
            "wait_on_response(%d) failed with error %d(%s)",
            timeoutSec, rc, net_strerror(rc) );
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

    if ( !(nc = validObjectPointer(pync)) )
        return NULL;

    if ( !(rp = STAILQ_FIRST(&(nc->dd->recvq))) ){
        PyErr_Format(
            PyExc_EnvironmentError,
            "No response from %s",
            nc->dd->host );
        return NULL;
    }
    
    if ( (rc = unpack_net_cmd_packet(&ncmd, rp)) != NET_OK ){
        PyErr_Format(
            PyExc_EnvironmentError, 
            "unpack_net_cmd_packet() failed with error %d(%s)",
            rc, net_strerror(rc) );
        return NULL;
    }
    
    rv = Py_BuildValue("(iss)", ncmd.id, ncmd.subsystem, ncmd.value);
    free_net_cmd_strs(ncmd);
    
    return rv;
}


static int setDictItem( PyObject* dict, char *key, char *val, int ival ){
    PyObject *oVal  = NULL;

    if (val){
        if ( !(oVal = PyString_FromString(val)) ){
            PyErr_Format(
                PyExc_EnvironmentError,
                "Could not create item string '%s'",
                val );
            return -1;
        }
    } else {
        if ( !(oVal = PyInt_FromLong(ival)) ){
            PyErr_Format(
                PyExc_EnvironmentError,
                "Could not create item int '%d'",
                ival );
            return -2;
        }
    }

    if ( PyDict_SetItemString(dict, key, oVal) ){
        PyErr_Format(
            PyExc_EnvironmentError,
            "Could not set dictionary item '%s'",
            key );
        return -3;
    }
    return 0;
}


static PyObject* Py_get_param_defaults(PyObject *obj, PyObject *args) {
    PyObject* dict = PyDict_New();
    if (setDictItem(dict,           "DEFAULT_KEY_PATH",     DEFAULT_KEY_PATH    , 0)
            || setDictItem(dict,    "WRTCTLD_DEFAULT_PORT", WRTCTLD_DEFAULT_PORT, 0)
            || setDictItem(dict,    "WRTCTL_SSL_PORT",      WRTCTL_SSL_PORT     , 0)
            || setDictItem(dict,    "WRTCTLD_SSL_PORT",     WRTCTLD_SSL_PORT    , 0)
            || setDictItem(dict,    "NET_ERR_TIMEOUT",      NULL                , NET_ERR_TIMEOUT)
            || setDictItem(dict,    "NET_OK",               NULL                , NET_OK         ) ){
        return NULL;
    }
    return dict;    
}



static void cleanup_wrtctl() {
}


static PyMethodDef _wrtctl_funcs[] = {
        { "alloc_client",
            Py_alloc_client,        METH_VARARGS,
            "wco = _wrtctl.alloc_client(log=True, verbose=False)"
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

        { "get_param_defaults",
            Py_get_param_defaults,  METH_NOARGS,
            "{ <default values> } = _wrtctl.get_param_defaults()"
        },

#ifdef ENABLE_STUNNEL
        { "start_stunnel_client",
            Py_start_stunnel_client, METH_VARARGS,
            "ctxobj = _wrtctl.start_stunnel_client(hostname, key_path='" \
            DEFAULT_KEY_PATH"', port='"WRTCTLD_DEFAULT_PORT"', wrtctlPort='" \
            WRTCTL_SSL_PORT"', wrtctldPort='"WRTCTLD_SSL_PORT"')" 
        },
#endif

        { NULL, NULL }
};


PyMODINIT_FUNC init_wrtctl( void ) {
    Py_InitModule3("_wrtctl", _wrtctl_funcs, "OpenWRT Control");
    Py_AtExit(cleanup_wrtctl); // register a cleanup function at exit
}
