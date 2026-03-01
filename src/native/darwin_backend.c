/* Copyright (C) 2026 Dry Ark LLC */
/* License: Fair Coding License 1.0+ */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#ifdef __APPLE__

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_utun.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "device_common.h"

#ifndef SYSPROTO_CONTROL
#define SYSPROTO_CONTROL 2
#endif

#ifndef AF_SYS_CONTROL
#define AF_SYS_CONTROL 2
#endif

static int ensure_open(TunTapDeviceObject *self) {
    if (self->is_closed || self->fd < 0) {
        PyErr_SetString(PyExc_OSError, "device is closed");
        return 0;
    }
    return 1;
}

static int run_ifconfig(const char *ifname, const char *arg1, const char *arg2, const char *arg3) {
    char cmd[512];
    if (arg2 == NULL) {
        int written = snprintf(cmd, sizeof(cmd), "ifconfig %s %s >/dev/null 2>&1", ifname, arg1);
        if (written <= 0 || written >= (int)sizeof(cmd)) {
            errno = EINVAL;
            return -1;
        }
    } else if (arg3 == NULL) {
        int written = snprintf(cmd, sizeof(cmd), "ifconfig %s %s %s >/dev/null 2>&1", ifname, arg1, arg2);
        if (written <= 0 || written >= (int)sizeof(cmd)) {
            errno = EINVAL;
            return -1;
        }
    } else {
        int written = snprintf(cmd, sizeof(cmd), "ifconfig %s %s %s %s >/dev/null 2>&1", ifname, arg1, arg2, arg3);
        if (written <= 0 || written >= (int)sizeof(cmd)) {
            errno = EINVAL;
            return -1;
        }
    }
    return system(cmd);
}

int pytun_backend_init(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    int preferred_num = -1;
    static char *kwlist[] = {"preferred_num", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &preferred_num)) {
        return -1;
    }

    int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    struct ctl_info info;
    memset(&info, 0, sizeof(info));
    strlcpy(info.ctl_name, UTUN_CONTROL_NAME, sizeof(info.ctl_name));
    if (ioctl(fd, CTLIOCGINFO, &info) < 0) {
        int err = errno;
        close(fd);
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    struct sockaddr_ctl addr;
    memset(&addr, 0, sizeof(addr));
    addr.sc_len = sizeof(addr);
    addr.sc_family = AF_SYSTEM;
    addr.ss_sysaddr = AF_SYS_CONTROL;
    addr.sc_id = info.ctl_id;

    int connected = 0;
    if (preferred_num >= 0) {
        addr.sc_unit = (unsigned int)preferred_num + 1;
        connected = connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0;
    } else {
        for (unsigned int i = 1; i <= 255; i++) {
            addr.sc_unit = i;
            if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                connected = 1;
                break;
            }
        }
    }

    if (!connected) {
        int err = errno;
        close(fd);
        errno = err == 0 ? ENODEV : err;
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    socklen_t ifname_len = (socklen_t)sizeof(self->ifname);
    if (getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, self->ifname, &ifname_len) < 0) {
        int err = errno;
        close(fd);
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    self->fd = fd;
    self->is_closed = 0;
    return 0;
}

void pytun_backend_dealloc(TunTapDeviceObject *self) {
    if (!self->is_closed && self->fd >= 0) {
        close(self->fd);
        self->fd = -1;
        self->is_closed = 1;
    }
}

PyObject *pytun_backend_close(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored)) {
    if (!self->is_closed && self->fd >= 0) {
        if (close(self->fd) < 0) {
            PyErr_SetFromErrno(PyExc_OSError);
            return NULL;
        }
    }
    self->fd = -1;
    self->is_closed = 1;
    Py_RETURN_NONE;
}

PyObject *pytun_backend_up(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    if (!ensure_open(self)) {
        return NULL;
    }
    if (run_ifconfig(self->ifname, "up", NULL, NULL) != 0) {
        PyErr_SetString(PyExc_OSError, "ifconfig up failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyObject *pytun_backend_down(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored)) {
    if (!ensure_open(self)) {
        return NULL;
    }
    if (run_ifconfig(self->ifname, "down", NULL, NULL) != 0) {
        PyErr_SetString(PyExc_OSError, "ifconfig down failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyObject *pytun_backend_read(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    int size = 0;
    static char *kwlist[] = {"size", "timeout", NULL};
    PyObject *timeout = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|O", kwlist, &size, &timeout)) {
        return NULL;
    }
    (void)timeout;
    if (!ensure_open(self)) {
        return NULL;
    }
    if (size <= 0) {
        PyErr_SetString(PyExc_ValueError, "size must be > 0");
        return NULL;
    }
    char *buf = PyMem_Malloc((size_t)size);
    if (buf == NULL) {
        return PyErr_NoMemory();
    }
    ssize_t n = read(self->fd, buf, (size_t)size);
    if (n < 0) {
        int err = errno;
        PyMem_Free(buf);
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    PyObject *out = PyBytes_FromStringAndSize(buf, n);
    PyMem_Free(buf);
    return out;
}

PyObject *pytun_backend_write(TunTapDeviceObject *self, PyObject *args) {
    const char *buf = NULL;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "y#", &buf, &size)) {
        return NULL;
    }
    if (!ensure_open(self)) {
        return NULL;
    }
    ssize_t n = write(self->fd, buf, (size_t)size);
    if (n < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    return PyLong_FromSsize_t(n);
}

PyObject *pytun_backend_fileno(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored)) {
    if (!ensure_open(self)) {
        return NULL;
    }
    return PyLong_FromLong(self->fd);
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
    struct ifaddrs *ifa = NULL;
    if (getifaddrs(&ifa) != 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    for (struct ifaddrs *cur = ifa; cur != NULL; cur = cur->ifa_next) {
        if (cur->ifa_addr == NULL || cur->ifa_addr->sa_family != AF_INET6) {
            continue;
        }
        if (strcmp(cur->ifa_name, self->ifname) != 0) {
            continue;
        }
        char out[INET6_ADDRSTRLEN];
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)cur->ifa_addr;
        if (inet_ntop(AF_INET6, &sin6->sin6_addr, out, sizeof(out)) != NULL) {
            freeifaddrs(ifa);
            return PyUnicode_FromString(out);
        }
    }
    freeifaddrs(ifa);
    Py_RETURN_NONE;
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
    struct in6_addr v6;
    if (inet_pton(AF_INET6, addr, &v6) != 1) {
        Py_DECREF(addr_bytes);
        PyErr_SetString(PyExc_ValueError, "Bad IPv6 address");
        return -1;
    }
    if (run_ifconfig(self->ifname, "inet6", addr, "prefixlen 64") != 0) {
        Py_DECREF(addr_bytes);
        PyErr_SetString(PyExc_OSError, "ifconfig inet6 failed");
        return -1;
    }
    Py_DECREF(addr_bytes);
    return 0;
}

PyObject *pytun_backend_get_mtu(TunTapDeviceObject *self, void *closure) {
    (void)closure;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strlcpy(req.ifr_name, self->ifname, sizeof(req.ifr_name));
    if (ioctl(sock, SIOCGIFMTU, &req) < 0) {
        int err = errno;
        close(sock);
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    close(sock);
    return PyLong_FromLong(req.ifr_mtu);
}

int pytun_backend_set_mtu(TunTapDeviceObject *self, PyObject *value, void *closure) {
    (void)closure;
    long mtu = PyLong_AsLong(value);
    if (mtu == -1 && PyErr_Occurred()) {
        return -1;
    }
    if (mtu <= 0) {
        PyErr_SetString(PyExc_ValueError, "Bad MTU, should be > 0");
        return -1;
    }
    char mtu_str[32];
    snprintf(mtu_str, sizeof(mtu_str), "%ld", mtu);
    if (run_ifconfig(self->ifname, "mtu", mtu_str, NULL) != 0) {
        PyErr_SetString(PyExc_OSError, "ifconfig mtu failed");
        return -1;
    }
    return 0;
}

#else
#error "darwin_backend.c compiled on non-macOS platform"
#endif
