/* Copyright (C) 2026 Dry Ark LLC */
/* License: Fair Coding License 1.0+ */

#ifndef PYTUN_DEVICE_COMMON_H
#define PYTUN_DEVICE_COMMON_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct {
    PyObject_HEAD
    int fd;
    int is_closed;
    char ifname[64];
#ifdef _WIN32
    HMODULE wintun_module;
    void *adapter;
    void *session;
    HANDLE read_wait_event;
    HANDLE read_cancel_event;
    char addr_value[64];
    unsigned int mtu_value;
#endif
} TunTapDeviceObject;

int pytun_backend_init(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs);
void pytun_backend_dealloc(TunTapDeviceObject *self);

PyObject *pytun_backend_close(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored));
PyObject *pytun_backend_up(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs);
PyObject *pytun_backend_down(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored));
PyObject *pytun_backend_read(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs);
PyObject *pytun_backend_write(TunTapDeviceObject *self, PyObject *args);
PyObject *pytun_backend_fileno(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored));
PyObject *pytun_backend_persist(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs);
PyObject *pytun_backend_mq_attach(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs);

PyObject *pytun_backend_get_name(TunTapDeviceObject *self, void *closure);
PyObject *pytun_backend_get_addr(TunTapDeviceObject *self, void *closure);
int pytun_backend_set_addr(TunTapDeviceObject *self, PyObject *value, void *closure);
PyObject *pytun_backend_get_mtu(TunTapDeviceObject *self, void *closure);
int pytun_backend_set_mtu(TunTapDeviceObject *self, PyObject *value, void *closure);

#endif
