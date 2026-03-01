/* Copyright (C) 2026 Dry Ark LLC */
/* License: Fair Coding License 1.0+ */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "device_common.h"

static PyObject *TunTapDevice_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    TunTapDeviceObject *self = (TunTapDeviceObject *)PyType_GenericAlloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    self->fd = -1;
    self->is_closed = 1;
    self->ifname[0] = '\0';
#ifdef _WIN32
    self->wintun_module = NULL;
    self->adapter = NULL;
    self->session = NULL;
    self->read_wait_event = NULL;
    self->read_cancel_event = NULL;
    self->addr_value[0] = '\0';
    self->mtu_value = 1500;
#endif
    return (PyObject *)self;
}

static int TunTapDevice_init(PyObject *self, PyObject *args, PyObject *kwargs) {
    TunTapDeviceObject *obj = (TunTapDeviceObject *)self;
    obj->fd = -1;
    obj->is_closed = 1;
    obj->ifname[0] = '\0';
#ifdef _WIN32
    obj->wintun_module = NULL;
    obj->adapter = NULL;
    obj->session = NULL;
    obj->read_wait_event = NULL;
    obj->read_cancel_event = NULL;
    obj->addr_value[0] = '\0';
    obj->mtu_value = 1500;
#endif
    return pytun_backend_init((TunTapDeviceObject *)self, args, kwargs);
}

static void TunTapDevice_dealloc(PyObject *self) {
    pytun_backend_dealloc((TunTapDeviceObject *)self);
    PyObject *tp = (PyObject *)Py_TYPE(self);
    PyObject_Free(self);
    Py_DECREF(tp);
}

static PyMethodDef TunTapDevice_methods[] = {
    {"close", (PyCFunction)pytun_backend_close, METH_NOARGS, PyDoc_STR("Close device/session.")},
    {"up", (PyCFunction)(void *)(PyCFunctionWithKeywords)pytun_backend_up, METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Bring interface up (or start session on Windows).")},
    {"down", (PyCFunction)pytun_backend_down, METH_NOARGS, PyDoc_STR("Bring interface down.")},
    {"read", (PyCFunction)(void *)(PyCFunctionWithKeywords)pytun_backend_read, METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Read packet bytes from device.")},
    {"write", (PyCFunction)pytun_backend_write, METH_VARARGS, PyDoc_STR("Write packet bytes to device.")},
    {"fileno", (PyCFunction)pytun_backend_fileno, METH_NOARGS, PyDoc_STR("Return underlying file descriptor when available.")},
    {"persist", (PyCFunction)(void *)(PyCFunctionWithKeywords)pytun_backend_persist, METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Enable/disable persistence where supported.")},
    {"mq_attach", (PyCFunction)(void *)(PyCFunctionWithKeywords)pytun_backend_mq_attach, METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Attach/detach multiqueue where supported.")},
    {NULL, NULL, 0, NULL},
};

static PyGetSetDef TunTapDevice_getset[] = {
    {"name", (getter)pytun_backend_get_name, NULL, PyDoc_STR("Interface name"), NULL},
    {"addr", (getter)pytun_backend_get_addr, (setter)pytun_backend_set_addr, PyDoc_STR("Interface address"), NULL},
    {"mtu", (getter)pytun_backend_get_mtu, (setter)pytun_backend_set_mtu, PyDoc_STR("Interface MTU"), NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

static PyType_Slot TunTapDevice_slots[] = {
    {Py_tp_new, (void *)TunTapDevice_new},
    {Py_tp_init, (void *)TunTapDevice_init},
    {Py_tp_dealloc, (void *)TunTapDevice_dealloc},
    {Py_tp_methods, (void *)TunTapDevice_methods},
    {Py_tp_getset, (void *)TunTapDevice_getset},
    {Py_tp_doc, (void *)"Native TUN/TAP device object."},
    {0, 0},
};

static PyType_Spec TunTapDevice_spec = {
    .name = "py_tuntap_abi3._pytun.TunTapDevice",
    .basicsize = sizeof(TunTapDeviceObject),
    .itemsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .slots = TunTapDevice_slots,
};

static struct PyModuleDef pytun_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_pytun",
    .m_doc = "Native pytun backend built with CPython limited API.",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit__pytun(void) {
    PyObject *mod = PyModule_Create(&pytun_module);
    if (mod == NULL) {
        return NULL;
    }

    PyObject *device_type = PyType_FromSpec(&TunTapDevice_spec);
    if (device_type == NULL) {
        Py_DECREF(mod);
        return NULL;
    }

    if (PyModule_AddObject(mod, "TunTapDevice", device_type) < 0) {
        Py_DECREF(device_type);
        Py_DECREF(mod);
        return NULL;
    }

    if (PyModule_AddIntConstant(mod, "IFF_TUN", 0x0001) < 0 ||
        PyModule_AddIntConstant(mod, "IFF_TAP", 0x0002) < 0) {
        Py_DECREF(mod);
        return NULL;
    }

    return mod;
}
