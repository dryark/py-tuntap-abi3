/* Copyright (C) 2026 Dry Ark LLC */
/* License: Fair Coding License 1.0+ */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <string.h>

#include "device_common.h"

typedef void *WINTUN_ADAPTER_HANDLE;
typedef void *WINTUN_SESSION_HANDLE;
typedef BYTE *(__stdcall *WintunAllocateSendPacketFn)(WINTUN_SESSION_HANDLE, DWORD);
typedef WINTUN_ADAPTER_HANDLE(__stdcall *WintunCreateAdapterFn)(const WCHAR *, const WCHAR *, const GUID *);
typedef void(__stdcall *WintunCloseAdapterFn)(WINTUN_ADAPTER_HANDLE);
typedef void(__stdcall *WintunEndSessionFn)(WINTUN_SESSION_HANDLE);
typedef HANDLE(__stdcall *WintunGetReadWaitEventFn)(WINTUN_SESSION_HANDLE);
typedef BYTE *(__stdcall *WintunReceivePacketFn)(WINTUN_SESSION_HANDLE, DWORD *);
typedef void(__stdcall *WintunReleaseReceivePacketFn)(WINTUN_SESSION_HANDLE, const BYTE *);
typedef void(__stdcall *WintunSendPacketFn)(WINTUN_SESSION_HANDLE, const BYTE *);
typedef WINTUN_SESSION_HANDLE(__stdcall *WintunStartSessionFn)(WINTUN_ADAPTER_HANDLE, DWORD);

typedef struct {
    WintunCreateAdapterFn WintunCreateAdapter;
    WintunCloseAdapterFn WintunCloseAdapter;
    WintunStartSessionFn WintunStartSession;
    WintunEndSessionFn WintunEndSession;
    WintunAllocateSendPacketFn WintunAllocateSendPacket;
    WintunReceivePacketFn WintunReceivePacket;
    WintunReleaseReceivePacketFn WintunReleaseReceivePacket;
    WintunSendPacketFn WintunSendPacket;
    WintunGetReadWaitEventFn WintunGetReadWaitEvent;
} WintunApi;

static WintunApi api;

static int load_wintun_api(TunTapDeviceObject *self) {
    if (self->wintun_module != NULL) {
        return 1;
    }
    self->wintun_module = LoadLibraryW(L"wintun.dll");
    if (self->wintun_module == NULL) {
        return 0;
    }

    api.WintunCreateAdapter = (WintunCreateAdapterFn)GetProcAddress(self->wintun_module, "WintunCreateAdapter");
    api.WintunCloseAdapter = (WintunCloseAdapterFn)GetProcAddress(self->wintun_module, "WintunCloseAdapter");
    api.WintunStartSession = (WintunStartSessionFn)GetProcAddress(self->wintun_module, "WintunStartSession");
    api.WintunEndSession = (WintunEndSessionFn)GetProcAddress(self->wintun_module, "WintunEndSession");
    api.WintunAllocateSendPacket = (WintunAllocateSendPacketFn)GetProcAddress(self->wintun_module, "WintunAllocateSendPacket");
    api.WintunReceivePacket = (WintunReceivePacketFn)GetProcAddress(self->wintun_module, "WintunReceivePacket");
    api.WintunReleaseReceivePacket = (WintunReleaseReceivePacketFn)GetProcAddress(self->wintun_module, "WintunReleaseReceivePacket");
    api.WintunSendPacket = (WintunSendPacketFn)GetProcAddress(self->wintun_module, "WintunSendPacket");
    api.WintunGetReadWaitEvent = (WintunGetReadWaitEventFn)GetProcAddress(self->wintun_module, "WintunGetReadWaitEvent");

    if (api.WintunCreateAdapter == NULL || api.WintunCloseAdapter == NULL || api.WintunStartSession == NULL ||
        api.WintunEndSession == NULL || api.WintunAllocateSendPacket == NULL || api.WintunReceivePacket == NULL ||
        api.WintunReleaseReceivePacket == NULL || api.WintunSendPacket == NULL || api.WintunGetReadWaitEvent == NULL) {
        FreeLibrary(self->wintun_module);
        self->wintun_module = NULL;
        return 0;
    }

    return 1;
}

static int ensure_session(TunTapDeviceObject *self) {
    if (self->session == NULL) {
        PyErr_SetString(PyExc_OSError, "adapter session is not started; call up()");
        return 0;
    }
    return 1;
}

int pytun_backend_init(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    const char *name = "pywintun";
    static char *kwlist[] = {"name", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s", kwlist, &name)) {
        return -1;
    }

    if (!load_wintun_api(self)) {
        PyErr_SetString(PyExc_ImportError, "failed to load wintun.dll; ensure DLL directory is configured");
        return -1;
    }

    WCHAR adapter_name[128];
    int n = MultiByteToWideChar(CP_UTF8, 0, name, -1, adapter_name, (int)(sizeof(adapter_name) / sizeof(adapter_name[0])));
    if (n <= 0) {
        PyErr_SetString(PyExc_ValueError, "invalid adapter name");
        return -1;
    }

    self->adapter = api.WintunCreateAdapter(adapter_name, L"WinTun", NULL);
    if (self->adapter == NULL) {
        PyErr_SetFromWindowsErr(0);
        return -1;
    }

    strncpy(self->ifname, name, sizeof(self->ifname) - 1);
    self->ifname[sizeof(self->ifname) - 1] = '\0';
    self->is_closed = 0;
    self->addr_value[0] = '\0';
    self->mtu_value = 1500;
    return 0;
}

void pytun_backend_dealloc(TunTapDeviceObject *self) {
    if (self->read_cancel_event != NULL) {
        CloseHandle(self->read_cancel_event);
        self->read_cancel_event = NULL;
    }
    if (self->session != NULL) {
        api.WintunEndSession((WINTUN_SESSION_HANDLE)self->session);
        self->session = NULL;
    }
    if (self->adapter != NULL) {
        api.WintunCloseAdapter((WINTUN_ADAPTER_HANDLE)self->adapter);
        self->adapter = NULL;
    }
    if (self->wintun_module != NULL) {
        FreeLibrary(self->wintun_module);
        self->wintun_module = NULL;
    }
    self->is_closed = 1;
}

PyObject *pytun_backend_close(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored)) {
    pytun_backend_dealloc(self);
    Py_RETURN_NONE;
}

PyObject *pytun_backend_up(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    DWORD capacity = 0x400000;
    static char *kwlist[] = {"capacity", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|k", kwlist, &capacity)) {
        return NULL;
    }
    if (self->is_closed || self->adapter == NULL) {
        PyErr_SetString(PyExc_OSError, "adapter is closed");
        return NULL;
    }
    if (self->session != NULL) {
        Py_RETURN_NONE;
    }
    self->session = api.WintunStartSession((WINTUN_ADAPTER_HANDLE)self->adapter, capacity);
    if (self->session == NULL) {
        PyErr_SetFromWindowsErr(0);
        return NULL;
    }
    self->read_wait_event = api.WintunGetReadWaitEvent((WINTUN_SESSION_HANDLE)self->session);
    self->read_cancel_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    Py_RETURN_NONE;
}

PyObject *pytun_backend_down(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->read_cancel_event != NULL) {
        CloseHandle(self->read_cancel_event);
        self->read_cancel_event = NULL;
    }
    if (self->session != NULL) {
        api.WintunEndSession((WINTUN_SESSION_HANDLE)self->session);
        self->session = NULL;
    }
    Py_RETURN_NONE;
}

PyObject *pytun_backend_read(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    DWORD timeout = INFINITE;
    static char *kwlist[] = {"size", "timeout", NULL};
    PyObject *size_obj = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Ok", kwlist, &size_obj, &timeout)) {
        return NULL;
    }
    (void)size_obj;
    if (!ensure_session(self)) {
        return NULL;
    }

    for (;;) {
        DWORD packet_size = 0;
        BYTE *packet = api.WintunReceivePacket((WINTUN_SESSION_HANDLE)self->session, &packet_size);
        if (packet != NULL) {
            PyObject *out = PyBytes_FromStringAndSize((const char *)packet, packet_size);
            api.WintunReleaseReceivePacket((WINTUN_SESSION_HANDLE)self->session, packet);
            return out;
        }
        DWORD err = GetLastError();
        if (err != ERROR_NO_MORE_ITEMS) {
            PyErr_SetFromWindowsErr((int)err);
            return NULL;
        }
        HANDLE handles[2] = {self->read_wait_event, self->read_cancel_event};
        DWORD res = WaitForMultipleObjects(2, handles, FALSE, timeout);
        if (res == WAIT_OBJECT_0 + 1) {
            Py_RETURN_NONE;
        }
        if (res == WAIT_TIMEOUT) {
            PyErr_SetString(PyExc_TimeoutError, "read timed out");
            return NULL;
        }
        if (res != WAIT_OBJECT_0) {
            PyErr_SetFromWindowsErr(0);
            return NULL;
        }
    }
}

PyObject *pytun_backend_write(TunTapDeviceObject *self, PyObject *args) {
    const char *buf = NULL;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "y#", &buf, &size)) {
        return NULL;
    }
    if (!ensure_session(self)) {
        return NULL;
    }
    BYTE *packet = api.WintunAllocateSendPacket((WINTUN_SESSION_HANDLE)self->session, (DWORD)size);
    if (packet == NULL) {
        PyErr_SetFromWindowsErr(0);
        return NULL;
    }
    memcpy(packet, buf, (size_t)size);
    api.WintunSendPacket((WINTUN_SESSION_HANDLE)self->session, packet);
    return PyLong_FromSsize_t(size);
}

PyObject *pytun_backend_fileno(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored)) {
    (void)self;
    Py_RETURN_NONE;
}

PyObject *pytun_backend_persist(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    (void)self;
    (void)args;
    (void)kwargs;
    Py_RETURN_NONE;
}

PyObject *pytun_backend_mq_attach(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    (void)self;
    (void)args;
    (void)kwargs;
    Py_RETURN_NONE;
}

PyObject *pytun_backend_get_name(TunTapDeviceObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->ifname);
}

PyObject *pytun_backend_get_addr(TunTapDeviceObject *self, void *closure) {
    (void)closure;
    if (self->addr_value[0] == '\0') {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(self->addr_value);
}

int pytun_backend_set_addr(TunTapDeviceObject *self, PyObject *value, void *closure) {
    (void)closure;
    if (value == NULL) {
        PyErr_SetString(PyExc_AttributeError, "addr cannot be deleted");
        return -1;
    }
    PyObject *addr_bytes = PyUnicode_AsUTF8String(value);
    if (addr_bytes == NULL) {
        return -1;
    }
    const char *addr = PyBytes_AsString(addr_bytes);
    if (addr == NULL) {
        Py_DECREF(addr_bytes);
        return -1;
    }
    if (strlen(addr) >= sizeof(self->addr_value)) {
        Py_DECREF(addr_bytes);
        PyErr_SetString(PyExc_ValueError, "address value too long");
        return -1;
    }
    strncpy(self->addr_value, addr, sizeof(self->addr_value) - 1);
    self->addr_value[sizeof(self->addr_value) - 1] = '\0';
    Py_DECREF(addr_bytes);
    return 0;
}

PyObject *pytun_backend_get_mtu(TunTapDeviceObject *self, void *closure) {
    (void)closure;
    return PyLong_FromUnsignedLong(self->mtu_value);
}

int pytun_backend_set_mtu(TunTapDeviceObject *self, PyObject *value, void *closure) {
    (void)closure;
    unsigned long mtu = PyLong_AsUnsignedLong(value);
    if (mtu == (unsigned long)-1 && PyErr_Occurred()) {
        return -1;
    }
    if (mtu == 0) {
        PyErr_SetString(PyExc_ValueError, "Bad MTU, should be > 0");
        return -1;
    }
    self->mtu_value = (unsigned int)mtu;
    return 0;
}

#else
#error "windows_backend.c compiled on non-Windows platform"
#endif
