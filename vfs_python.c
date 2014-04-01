#include "include/includes.h"
#include "../lib/util/tevent_unix.h"
#include "../lib/util/tevent_ntstatus.h"
#include "python2.7/Python.h"

PyObject *py_mod;

static void debug(const char *text)
{
    FILE *fp = fopen("/home/vortec/workspace/vfs_python/log", "a");

    if (fp)
    {
	fprintf(fp, "SAMBA: %s\n", text);
	fclose(fp);
    }
}

static void py_import_handler(const char *script_path)
{
    PyObject *py_dir, *py_imp_str, *py_imp_handle, *py_imp_dict, *py_imp_load_source;
    PyObject *py_function_name, *py_args_tuple;
    
    Py_Initialize();
    
    py_dir = PyString_FromString(script_path);
    py_imp_str = PyString_FromString("imp");
    py_imp_handle = PyImport_Import(py_imp_str);
    py_imp_dict = PyModule_GetDict(py_imp_handle);
    py_imp_load_source = PyDict_GetItemString(py_imp_dict, "load_source");
    py_function_name = PyString_FromString("connect");

    // TODO:
    // remove py_function_name, should import entire module
    py_args_tuple = PyTuple_New(2);  // may not be Py_DECREF'ed
    PyTuple_SetItem(py_args_tuple, 0, py_function_name);
    PyTuple_SetItem(py_args_tuple, 1, py_dir);
    
    py_mod = PyObject_CallObject(py_imp_load_source, py_args_tuple);
    
    Py_DECREF(py_args_tuple);
    Py_DECREF(py_imp_str);
    Py_DECREF(py_imp_handle);
    Py_DECREF(py_imp_dict);
    Py_DECREF(py_imp_load_source);
}

static int python_connect(vfs_handle_struct *handle,
                          const char *service,
                          const char *user)
{
    const char *script_path = lp_parm_const_string(SNUM(handle->conn), "python", "script", NULL);
    // todo: needs null-termination?
    const char *function_name = "connect";
    
    py_import_handler(script_path);
    
    int success = 1;
    PyObject *py_function_name = PyString_FromString(function_name);
    PyObject *py_func = PyObject_GetAttr(py_mod, py_function_name);
    Py_DECREF(py_function_name);
        
    if (py_func != NULL)
    {
	if (PyCallable_Check(py_func) == 1)
	{
	    // todo: callfunction expects char *, not const char *
	    PyObject *py_ret = PyObject_CallFunction(py_func, "ss", service, user);
	    success = PyObject_IsTrue(py_ret);
	    Py_DECREF(py_ret);
	}
	Py_DECREF(py_func);
    }
        
    if (success == 1)
    {
	return SMB_VFS_NEXT_CONNECT(handle, service, user);
    }
    else
    {
	return -1;
    }
}

static void python_disconnect(vfs_handle_struct *handle)
{
    SMB_VFS_NEXT_DISCONNECT(handle);
}



static struct vfs_fn_pointers vfs_python_fns = {
        .connect_fn = python_connect,
        .disconnect_fn = python_disconnect
};

#define vfs_python_init samba_init_module

NTSTATUS vfs_python_init(void)
{
    return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
                            "python",
                            &vfs_python_fns);
}
