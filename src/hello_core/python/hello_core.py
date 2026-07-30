"""Thin ctypes wrapper over hello_core's C API (CLAUDE.md §9)."""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import sys


def _load_library() -> ctypes.CDLL:
    base = os.path.dirname(os.path.abspath(__file__))
    names = {
        "linux": "libhello_core_c_api.so",
        "darwin": "libhello_core_c_api.dylib",
        "win32": "hello_core_c_api.dll",
    }
    name = names.get(sys.platform, "libhello_core_c_api.so")
    for candidate in (os.path.join(base, name), name):
        try:
            return ctypes.CDLL(candidate)
        except OSError:
            continue
    raise OSError(f"could not locate {name}; build hello_core_c_api first")


_lib = _load_library()

_lib.hello_core_greeter_create.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
_lib.hello_core_greeter_create.restype = ctypes.c_int

_lib.hello_core_greeter_greet.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_uint,
    ctypes.POINTER(ctypes.c_uint),
]
_lib.hello_core_greeter_greet.restype = ctypes.c_int

_lib.hello_core_greeter_free.argtypes = [ctypes.c_void_p]
_lib.hello_core_greeter_free.restype = None

_STATUS_MESSAGES = {
    1: "null argument passed to hello_core C API",
    2: "unknown error in hello_core C API",
}


class HelloCoreError(RuntimeError):
    """Raised when a hello_core C API call returns a non-OK status."""


def _check(status: int) -> None:
    if status != 0:
        raise HelloCoreError(_STATUS_MESSAGES.get(status, f"unknown status code {status}"))


class Greeter:
    """Python wrapper around the hello_core::Greeter C API handle."""

    def __init__(self, name: str) -> None:
        handle = ctypes.c_void_p()
        _check(_lib.hello_core_greeter_create(name.encode("utf-8"), ctypes.byref(handle)))
        self._handle = handle

    def greet(self) -> str:
        buffer_size = 256
        buffer = ctypes.create_string_buffer(buffer_size)
        written = ctypes.c_uint()
        _check(
            _lib.hello_core_greeter_greet(
                self._handle, buffer, buffer_size, ctypes.byref(written)
            )
        )
        return buffer.raw[: written.value].decode("utf-8")

    def close(self) -> None:
        if self._handle:
            _lib.hello_core_greeter_free(self._handle)
            self._handle = ctypes.c_void_p()

    def __enter__(self) -> "Greeter":
        return self

    def __exit__(self, *_exc_info: object) -> None:
        self.close()

    def __del__(self) -> None:
        self.close()
