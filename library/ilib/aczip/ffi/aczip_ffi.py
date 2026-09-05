# AC ilib: aczip — Python ctypes FFI (libaczip.so / libaczip.dll)
# Inlined by AC->PY compiler when "use ilib aczip" is declared.
#
# AC has no raw byte-buffer type, so this wrapper keeps compressed data on the
# filesystem: compress() writes the result straight to output_path and returns the
# byte count (or -1 on failure); decompress() reads archive_path off disk, hands the
# bytes to ac_zip_decompress, and returns its status code. Every ACZipByteArray the C
# library hands back is released with ac_free_bytes() before returning, matching
# aczip_c.h's "caller MUST release with ac_free_bytes()" contract.
import ctypes as _ct, os as _os, platform as _pl, types as _types

def _aczip_load():
    _base = globals().get('_ac_aczip_lib_dir',
                          _os.path.join(_os.getcwd(), 'library', 'ilib', 'aczip'))
    _name = 'aczip.dll' if _pl.system() == 'Windows' else 'libaczip.so'
    _p = _os.path.join(_base, _name)
    if _os.path.exists(_p):
        return _ct.CDLL(_os.path.abspath(_p))
    return None

_c = _aczip_load()

if _c:
    class _ACZipByteArray(_ct.Structure):
        _fields_ = [("data", _ct.POINTER(_ct.c_char)), ("size", _ct.c_size_t)]

    _c.ac_zip_compress.argtypes = [_ct.c_char_p, _ct.c_int]
    _c.ac_zip_compress.restype = _ACZipByteArray
    _c.ac_zip_compress_hdd.argtypes = [_ct.c_char_p]
    _c.ac_zip_compress_hdd.restype = _ACZipByteArray
    _c.ac_zip_compress_sata.argtypes = [_ct.c_char_p]
    _c.ac_zip_compress_sata.restype = _ACZipByteArray
    _c.ac_zip_decompress.argtypes = [_ct.c_char_p, _ct.c_size_t, _ct.c_char_p]
    _c.ac_zip_decompress.restype = _ct.c_int
    _c.ac_get_compression_ratio.argtypes = [_ct.c_size_t, _ct.c_size_t]
    _c.ac_get_compression_ratio.restype = _ct.c_double
    _c.ac_free_bytes.argtypes = [_ACZipByteArray]
    _c.ac_free_bytes.restype = None

    def _b(s): return s.encode() if isinstance(s, str) else s

    def _write_result(arr):
        if not arr.data or arr.size == 0:
            _c.ac_free_bytes(arr)
            return -1
        try:
            raw = _ct.string_at(arr.data, arr.size)
        finally:
            _c.ac_free_bytes(arr)
        return raw

    def aczip_compress(path, parallel, output_path):
        raw = _write_result(_c.ac_zip_compress(_b(path), 1 if parallel else 0))
        if raw == -1: return -1
        with open(output_path, 'wb') as f: f.write(raw)
        return len(raw)

    def aczip_compress_hdd(path, output_path):
        raw = _write_result(_c.ac_zip_compress_hdd(_b(path)))
        if raw == -1: return -1
        with open(output_path, 'wb') as f: f.write(raw)
        return len(raw)

    def aczip_compress_sata(path, output_path):
        raw = _write_result(_c.ac_zip_compress_sata(_b(path)))
        if raw == -1: return -1
        with open(output_path, 'wb') as f: f.write(raw)
        return len(raw)

    def aczip_decompress(archive_path, output_path):
        with open(archive_path, 'rb') as f: data = f.read()
        return _c.ac_zip_decompress(data, len(data), _b(output_path))

    def aczip_get_ratio(original, compressed):
        return _c.ac_get_compression_ratio(int(original), int(compressed))

else:
    import sys as _sys
    _sys.stderr.write("[aczip] WARNING: libaczip.so not found — stub mode\n")
    def aczip_compress(path, parallel, output_path): return -1
    def aczip_compress_hdd(path, output_path): return -1
    def aczip_compress_sata(path, output_path): return -1
    def aczip_decompress(archive_path, output_path): return -1
    def aczip_get_ratio(original, compressed): return 0.0

aczip = _types.SimpleNamespace(
    compress=aczip_compress, compress_hdd=aczip_compress_hdd,
    compress_sata=aczip_compress_sata, decompress=aczip_decompress,
    get_ratio=aczip_get_ratio,
)
