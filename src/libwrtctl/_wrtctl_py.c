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

#include "wrtctl-net.h"


static nc_t validObjectPointer(PyObject *pync) {
    nc_t nc;
    if (!PyCObject_Check(pync) || !(nc = (nc_t)PyCObject_AsVoidPtr(pync))) {
	PyErr_Format(PyExc_ValueError, "Invalid nc_t object pointer received.");
        return NULL;
    }
    return nc;
}



static void deletenc(void *vptr) {
    if (vptr)
        free(vptr);
}

static PyObject* Py_alloc_client(PyObject *obj, PyObject *args) {    
    bool enable_log = true;
    bool verbose    = false;
    if (!PyArg_ParseTuple(args, "|ii", &enable_log, &verbose))
	return NULL;
    nc_t nc;
    int rc;
    if ( (rc = alloc_client(&nc, enable_log, verbose)) != NET_OK) {
        PyErr_Format(PyExc_EnvironmentError, "alloc_client() failed with error %d(%s)", rc, net_strerror(rc));
	return NULL;
    }
    return PyCObject_FromVoidPtr((void*)nc, deletenc);
}


static void deletectx(void *vptr) {
    if (vptr)
        kill_stunnel((stunnel_ctx_t *)&vptr);
}

static PyObject* Py_start_stunnel_client(PyObject *obj, PyObject *args) {    
    char *hostname;
    char *key_path     = DEFAULT_KEY_PATH;
    char *port         = WRTCTLD_DEFAULT_PORT;
    char *wrtctl_port  = WRTCTL_SSL_PORT;
    char *wrtctld_port = WRTCTLD_SSL_PORT;
    if (!PyArg_ParseTuple(args, "s|ssss", &hostname, &key_path, &port, &wrtctl_port, &wrtctld_port))
	return NULL;
    stunnel_ctx_t ctx;
    int rc;
    if ( (rc = start_stunnel_client(&ctx, hostname, key_path, port, wrtctl_port, wrtctld_port)) != 0) {
        PyErr_Format(PyExc_EnvironmentError, "start_stunnel_client('%s', '%s', '%s', '%s', '%s') failed with error %d(%s)", hostname, key_path, port, wrtctl_port, wrtctld_port, rc, net_strerror(rc));
	return NULL;
    }
    return PyCObject_FromVoidPtr((void*)ctx, deletectx);
}


static PyObject* Py_create_connection(PyObject *obj, PyObject *args) {
    PyObject *pync;
    char *hostname;
    char *port = WRTCTLD_DEFAULT_PORT;
    if (!PyArg_ParseTuple(args, "Os|s", &pync, &hostname, &port))
	return NULL;
    nc_t nc;
    if (!(nc = validObjectPointer(pync)))
	return NULL;
    int rc;
    if ( (rc = create_conn(nc, hostname, port)) != NET_OK) {
        PyErr_Format(PyExc_EnvironmentError, "create_conn('%s', '%s') failed with error %d(%s)", hostname, port, rc, net_strerror(rc));
	return NULL;
    }
    Py_RETURN_NONE;
}


static PyObject* Py_queue_net_command(PyObject *obj, PyObject *args) {
    PyObject *pync;
    int id;
    char *subsystem;
    char *value = "";
    if (!PyArg_ParseTuple(args, "Ois|s", &pync, &id, &subsystem, &value))
	return NULL;
    nc_t nc;
    if (!(nc = validObjectPointer(pync)))
	return NULL;
    packet_t sp;
    int rc;
    if ( (rc = create_net_cmd_packet(&sp, id, subsystem, value)) != NET_OK) {
        PyErr_Format(PyExc_EnvironmentError, "create_net_cmd_packet(%d, '%s', '%s') failed with error %d(%s)", id, subsystem, value, rc, net_strerror(rc));
	return NULL;
    }
    nc_add_packet(nc, sp);
    Py_RETURN_NONE;
}


static PyObject* Py_wait_on_response(PyObject *obj, PyObject *args) {
    // return 
    PyObject *pync;
    int timeoutSec      = 0;
    bool flushSendQueue = true;
    if (!PyArg_ParseTuple(args, "O|ii", &pync, &timeoutSec, &flushSendQueue))
	return NULL;
    nc_t nc;
    if (!(nc = validObjectPointer(pync)))
	return NULL;
    struct timeval timeout = { 0, 0 };
    timeout.tv_sec = timeoutSec;
    int rc = wait_on_response(nc, &timeout, flushSendQueue);
    if (rc != NET_OK   &&  rc != NET_ERR_TIMEOUT) {
        PyErr_Format(PyExc_EnvironmentError, "wait_on_response(%d) failed with error %d(%s)", timeoutSec, rc, net_strerror(rc));
	return NULL;
    }
    return Py_BuildValue("i", rc);    
}


static PyObject* Py_get_net_response(PyObject *obj, PyObject *args) {
    PyObject *pync;
    if (!PyArg_ParseTuple(args, "O", &pync))
	return NULL;
    nc_t nc;
    if (!(nc = validObjectPointer(pync)))
	return NULL;
    packet_t rp;
    if ( !(rp = STAILQ_FIRST(&(nc->dd->recvq))) ) {
        PyErr_Format(PyExc_EnvironmentError, "No response from %s", nc->dd->host);
	return NULL;
    }
    struct net_cmd ncmd;
    int rc;
    if ( (rc = unpack_net_cmd_packet(&ncmd, rp)) != NET_OK) {
        PyErr_Format(PyExc_EnvironmentError, "unpack_net_cmd_packet() failed with error %d(%s)", rc, net_strerror(rc));
	return NULL;
    }
    PyObject *rv = Py_BuildValue("(iss)", ncmd.id, ncmd.subsystem, ncmd.value);
//    free_net_cmd(ncmd); *************************************************************************** todo: implement when available ******************************************
    return rv;
}


int setDictItem(PyObject* dict, char *key, char *val) {
    PyObject *oKey, *oVal;
    if ( !(oKey = PyString_FromString(key)) ) {
        PyErr_Format(PyExc_EnvironmentError, "get_param_defaults() could not create key string '%s'", key);
        return 1;
    }
    if ( !(oVal = PyString_FromString(val)) ) {
        PyErr_Format(PyExc_EnvironmentError, "get_param_defaults() could not create item string '%s'", val);
        return 2;
    }
    if (PyDict_SetItem(dict, oKey, oVal)) {
        PyErr_Format(PyExc_EnvironmentError, "get_param_defaults() could not set dictionary item '%s':'%s'", key, val);
        return 3;
    }
    return 0;
}


static PyObject* Py_get_param_defaults(PyObject *obj, PyObject *args) {
    PyObject* dict = PyDict_New();
    if (setDictItem(dict, "DEFAULT_KEY_PATH"    , DEFAULT_KEY_PATH    )  ||
        setDictItem(dict, "WRTCTLD_DEFAULT_PORT", WRTCTLD_DEFAULT_PORT)  ||
        setDictItem(dict, "WRTCTL_SSL_PORT"     , WRTCTL_SSL_PORT     )  ||
        setDictItem(dict, "WRTCTLD_SSL_PORT"    , WRTCTLD_SSL_PORT    )  ||
        setDictItem(dict, "NET_ERR_TIMEOUT"     , NET_ERR_TIMEOUT     )  ||
        setDictItem(dict, "NET_OK"              , NET_OK              ) )
        return NULL;
    return dict;    
}



static void cleanup_wrtctl() {
}


static PyMethodDef _wrtctl_funcs[] = {
        { "alloc_client"        , Py_alloc_client        , METH_VARARGS, "wco = _wrtctl.alloc_client(log=True, verbose=False)" },
        { "start_stunnel_client", Py_start_stunnel_client, METH_VARARGS, "ctxobj = _wrtctl.start_stunnel_client(hostname, key_path='"DEFAULT_KEY_PATH"', port='"WRTCTLD_DEFAULT_PORT"', wrtctlPort='"WRTCTL_SSL_PORT"', wrtctldPort='"WRTCTLD_SSL_PORT"')" },
        { "create_connection"   , Py_create_connection   , METH_VARARGS, "_wrtctl.create_connection(wco, hostname, port='"WRTCTLD_DEFAULT_PORT"')" },
        { "queue_net_command"   , Py_queue_net_command   , METH_VARARGS, "_wrtctl.queue_net_command(wco, idInt, subsystemStr, valueStr='')" },
        { "wait_on_response"    , Py_wait_on_response    , METH_VARARGS, "_wrtctl.wait_on_response(wco, timeoutSec=0, flushSendQueue=True)" },
        { "get_net_response"    , Py_get_net_response    , METH_VARARGS, "(id, subsystemStr, valueStr) = _wrtctl.get_net_response(wco)" },
        { "get_param_defaults"  , Py_get_param_defaults  , METH_NOARGS , "{ <default values> } = _wrtctl.get_param_defaults()" },
        { NULL, NULL }
};


PyMODINIT_FUNC /*DSO_EXPORTED*/ init_wrtctl( void ) {
    Py_InitModule3("_wrtctl", _wrtctl_funcs, "OpenWRT Control");
    Py_AtExit(cleanup_wrtctl); // register a cleanup function at exit
}
