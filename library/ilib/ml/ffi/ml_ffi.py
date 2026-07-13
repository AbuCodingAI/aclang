import ctypes
import os

_lib_path = os.path.join(globals().get("_ac_ml_lib_dir", os.path.dirname(__file__)), "libacml.so")
_lib = ctypes.CDLL(_lib_path)

_lib.ml_tensor.argtypes = [ctypes.c_double]
_lib.ml_tensor.restype = ctypes.c_int64
_lib.ml_grid.argtypes = [ctypes.c_int64, ctypes.c_int64, ctypes.c_double]
_lib.ml_grid.restype = ctypes.c_int64
_lib.ml_gradient_track.argtypes = [ctypes.c_int64, ctypes.c_int]
_lib.ml_gradient_track.restype = ctypes.c_int
_lib.ml_grad_wipe.argtypes = [ctypes.c_int64]
_lib.ml_grad_wipe.restype = ctypes.c_int
_lib.ml_weights.argtypes = [ctypes.c_double, ctypes.c_int64]
_lib.ml_weights.restype = ctypes.c_int
_lib.ml_take.argtypes = [ctypes.c_int64]
_lib.ml_take.restype = ctypes.c_double
_lib.ml_add.argtypes = [ctypes.c_int64, ctypes.c_int64]
_lib.ml_add.restype = ctypes.c_int64
_lib.ml_multiply.argtypes = [ctypes.c_int64, ctypes.c_int64]
_lib.ml_multiply.restype = ctypes.c_int64
_lib.ml_relu.argtypes = [ctypes.c_int64]
_lib.ml_relu.restype = ctypes.c_int64
_lib.ml_backward.argtypes = [ctypes.c_int64]
_lib.ml_backward.restype = ctypes.c_int

class ml:
    @staticmethod
    def tensor(value): return int(_lib.ml_tensor(float(value)))
    @staticmethod
    def grid(rows, cols, fill): return int(_lib.ml_grid(int(rows), int(cols), float(fill)))
    @staticmethod
    def gradient_track(tensor_id, enabled): return int(_lib.ml_gradient_track(int(tensor_id), int(enabled)))
    @staticmethod
    def grad_wipe(tensor_id): return int(_lib.ml_grad_wipe(int(tensor_id)))
    @staticmethod
    def weights(learning_rate, tensor_id): return int(_lib.ml_weights(float(learning_rate), int(tensor_id)))
    @staticmethod
    def take(tensor_id): return float(_lib.ml_take(int(tensor_id)))
    @staticmethod
    def add(a, b): return int(_lib.ml_add(int(a), int(b)))
    @staticmethod
    def multiply(a, b): return int(_lib.ml_multiply(int(a), int(b)))
    @staticmethod
    def relu(x): return int(_lib.ml_relu(int(x)))
    @staticmethod
    def backward(loss): return int(_lib.ml_backward(int(loss)))
