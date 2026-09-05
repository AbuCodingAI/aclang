// AC ilib: aczip — Node ffi-napi FFI (libaczip.so / libaczip.dll)
// Inlined by AC->JS compiler when "use ilib aczip" is declared.
//
// AC has no raw byte-buffer type, so this wrapper keeps compressed data on the
// filesystem: compress() writes the result straight to output_path and returns the
// byte count (or -1 on failure); decompress() reads archive_path off disk, hands the
// bytes to ac_zip_decompress, and returns its status code. Every ACZipByteArray the C
// library hands back is released with ac_free_bytes() before returning, matching
// aczip_c.h's "caller MUST release with ac_free_bytes()" contract.
const _aczipFfi = require('ffi-napi'), _aczipRef = require('ref-napi'), _AczipStructType = require('ref-struct-di')(_aczipRef);
const _aczipFs = require('fs'), _aczipPath = require('path');

const _ACZipByteArray = _AczipStructType({ data: 'pointer', size: 'size_t' });

const _libDir = typeof _ac_aczip_lib_dir !== 'undefined' ? _ac_aczip_lib_dir
    : _aczipPath.join(process.cwd(), 'library', 'ilib', 'aczip');
const _libFile = process.platform === 'win32' ? 'aczip' : _aczipPath.join(_libDir, 'libaczip.so');
const _c = _aczipFfi.Library(_libFile, {
    'ac_zip_compress': [_ACZipByteArray, ['string', 'int']],
    'ac_zip_compress_hdd': [_ACZipByteArray, ['string']],
    'ac_zip_compress_sata': [_ACZipByteArray, ['string']],
    'ac_zip_decompress': ['int', ['pointer', 'size_t', 'string']],
    'ac_get_compression_ratio': ['double', ['size_t', 'size_t']],
    'ac_free_bytes': ['void', [_ACZipByteArray]],
});

function _writeResult(arr, outputPath) {
    if (arr.data.isNull() || arr.size === 0) { _c.ac_free_bytes(arr); return -1; }
    const buf = Buffer.from(_aczipRef.reinterpret(arr.data, arr.size));
    _aczipFs.writeFileSync(outputPath, buf);
    const n = buf.length;
    _c.ac_free_bytes(arr);
    return n;
}

const aczip = {
    compress: (path, parallel, outputPath) => _writeResult(_c.ac_zip_compress(path, parallel ? 1 : 0), outputPath),
    compress_hdd: (path, outputPath) => _writeResult(_c.ac_zip_compress_hdd(path), outputPath),
    compress_sata: (path, outputPath) => _writeResult(_c.ac_zip_compress_sata(path), outputPath),
    decompress: (archivePath, outputPath) => {
        const data = _aczipFs.readFileSync(archivePath);
        return _c.ac_zip_decompress(data, data.length, outputPath);
    },
    get_ratio: (original, compressed) => _c.ac_get_compression_ratio(original, compressed),
};
