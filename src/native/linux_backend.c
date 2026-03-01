/* Copyright (C) 2026 Dry Ark LLC */
/* License: Fair Coding License 1.0+ */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#ifdef __linux__

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "device_common.h"

static int ensure_open(TunTapDeviceObject *self) {
    if (self->is_closed || self->fd < 0) {
        PyErr_SetString(PyExc_OSError, "device is closed");
        return 0;
    }
    return 1;
}

int pytun_backend_init(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    const char *name = "";
    int flags = IFF_TUN;
    const char *dev = "/dev/net/tun";
    static char *kwlist[] = {"name", "flags", "dev", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sis", kwlist, &name, &flags, &dev)) {
        return -1;
    }

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    struct ifreq req;
    memset(&req, 0, sizeof(req));
    if (name != NULL && name[0] != '\0') {
        strncpy(req.ifr_name, name, IFNAMSIZ - 1);
    }
    req.ifr_flags = (short)flags;

    if (ioctl(fd, TUNSETIFF, &req) < 0) {
        int err = errno;
        close(fd);
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    self->fd = fd;
    self->is_closed = 0;
    strncpy(self->ifname, req.ifr_name, sizeof(self->ifname) - 1);
    self->ifname[sizeof(self->ifname) - 1] = '\0';
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

static int ioctl_flags(TunTapDeviceObject *self, int set_up) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, self->ifname, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFFLAGS, &req) < 0) {
        int err = errno;
        close(sock);
        errno = err;
        return -1;
    }
    if (set_up) {
        req.ifr_flags |= IFF_UP;
    } else {
        req.ifr_flags &= (short)~IFF_UP;
    }
    int rc = ioctl(sock, SIOCSIFFLAGS, &req);
    int err = errno;
    close(sock);
    errno = err;
    return rc;
}

PyObject *pytun_backend_up(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    if (!ensure_open(self)) {
        return NULL;
    }
    if (ioctl_flags(self, 1) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_RETURN_NONE;
}

PyObject *pytun_backend_down(TunTapDeviceObject *self, PyObject *Py_UNUSED(ignored)) {
    if (!ensure_open(self)) {
        return NULL;
    }
    if (ioctl_flags(self, 0) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
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
    int flag = 1;
    static char *kwlist[] = {"flag", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|p", kwlist, &flag)) {
        return NULL;
    }
    if (!ensure_open(self)) {
        return NULL;
    }
    if (ioctl(self->fd, TUNSETPERSIST, flag) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_RETURN_NONE;
}

PyObject *pytun_backend_mq_attach(TunTapDeviceObject *self, PyObject *args, PyObject *kwargs) {
    int flag = 1;
    static char *kwlist[] = {"flag", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|p", kwlist, &flag)) {
        return NULL;
    }
    if (!ensure_open(self)) {
        return NULL;
    }
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    req.ifr_flags = flag ? IFF_ATTACH_QUEUE : IFF_DETACH_QUEUE;
    if (ioctl(self->fd, TUNSETQUEUE, &req) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_RETURN_NONE;
}

PyObject *pytun_backend_get_name(TunTapDeviceObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->ifname);
}

static int run_ip_command(const char *ifname, const char *address) {
    char command[256];
    int written = snprintf(command, sizeof(command), "ip -6 addr replace %s/64 dev %s >/dev/null 2>&1", address, ifname);
    if (written <= 0 || written >= (int)sizeof(command)) {
        errno = EINVAL;
        return -1;
    }
    return system(command);
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
        if (strncmp(cur->ifa_name, self->ifname, IFNAMSIZ) != 0) {
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
    if (run_ip_command(self->ifname, addr) != 0) {
        Py_DECREF(addr_bytes);
        PyErr_SetString(PyExc_OSError, "failed to set IPv6 address");
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
    strncpy(req.ifr_name, self->ifname, IFNAMSIZ - 1);
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
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, self->ifname, IFNAMSIZ - 1);
    req.ifr_mtu = (int)mtu;
    if (ioctl(sock, SIOCSIFMTU, &req) < 0) {
        int err = errno;
        close(sock);
        errno = err;
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }
    close(sock);
    return 0;
}

#else
#error "linux_backend.c compiled on non-Linux platform"
#endif
