import gzip
import zlib
import tarfile
import os
import io
import struct
import lzma
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

try:
    import zstandard as zstd
    HAS_ZSTD = True
except ImportError:
    HAS_ZSTD = False

class ACZip:
    """ACZip v2 archive: the same format as aczip.cpp (ACZ2 magic, then per file: path, original size,
    compressed size, zlib data). A file or a directory is archived; decompression recreates the paths."""

    MAGIC = b'ACZ2'

    @staticmethod
    def _files(path):
        if os.path.isfile(path):
            with open(path, 'rb') as f:
                return [(os.path.basename(path), f.read())]
        files = []
        for root, _dirs, names in os.walk(path):
            for name in names:
                full = os.path.join(root, name)
                with open(full, 'rb') as f:
                    files.append((os.path.relpath(full, path).replace(os.sep, '/'), f.read()))
        return files

    @staticmethod
    def build(path):
        """The archive bytes for a file or directory."""
        out = bytearray(ACZip.MAGIC)
        files = ACZip._files(path)
        out += struct.pack('<I', len(files))
        for name, data in files:
            raw = name.encode('utf-8')
            comp = zlib.compress(data, 6)
            out += struct.pack('<I', len(raw)) + raw
            out += struct.pack('<I', len(data)) + struct.pack('<I', len(comp)) + comp
        return bytes(out)

    @staticmethod
    def compress(path, parallel=True, output_path=None):
        """Archive path into output_path. Returns the archive size in bytes (-1 on failure), like the C side."""
        try:
            data = ACZip.build(path)
        except OSError:
            return -1
        if output_path is None:
            return data
        with open(output_path, 'wb') as f:
            f.write(data)
        return len(data)

    @staticmethod
    def decompress(archive_path, output_path):
        """Recreate the archive's files under output_path. Returns 0 on success, -1 on failure."""
        with open(archive_path, 'rb') as f:
            data = f.read()
        if len(data) < 8 or data[:4] != ACZip.MAGIC:
            return -1
        pos = 4
        count = struct.unpack_from('<I', data, pos)[0]; pos += 4
        for _ in range(count):
            plen = struct.unpack_from('<I', data, pos)[0]; pos += 4
            rel = data[pos:pos + plen].decode('utf-8'); pos += plen
            orig = struct.unpack_from('<I', data, pos)[0]; pos += 4
            comp_size = struct.unpack_from('<I', data, pos)[0]; pos += 4
            body = zlib.decompress(data[pos:pos + comp_size]); pos += comp_size
            if len(body) != orig:
                return -1
            # no absolute paths and no "..": nothing may land outside output_path
            parts = [p for p in rel.split('/') if p not in ('', '.', '..')]
            if not parts:
                parts = ['file']
            target = os.path.join(output_path, *parts)
            os.makedirs(os.path.dirname(target) or '.', exist_ok=True)
            with open(target, 'wb') as f:
                f.write(body)
        return 0

    @staticmethod
    def compress_hdd(path, output_path=None):
        """Compress optimized for HDD (sequential)"""
        return ACZip.compress(path, False, output_path)

    @staticmethod
    def compress_sata(path, output_path=None):
        """Compress optimized for SATA (balanced, parallel)"""
        return ACZip.compress(path, True, output_path)

    @staticmethod
    def get_ratio(original_size, compressed_size):
        """Calculate compression ratio"""
        if original_size == 0:
            return 0.0
        return (compressed_size * 100.0) / original_size


class ACTar:
    """TAR archive creation and extraction"""

    @staticmethod
    def create(path):
        """Create TAR archive from path"""
        tar_buffer = io.BytesIO()
        with tarfile.open(fileobj=tar_buffer, mode='w') as tar:
            tar.add(path, arcname=os.path.basename(path))
        return tar_buffer.getvalue()

    @staticmethod
    def extract(data, output_path):
        """Extract TAR archive"""
        tar_buffer = io.BytesIO(data)
        with tarfile.open(fileobj=tar_buffer, mode='r') as tar:
            tar.extractall(path=output_path)


class ACGzip:
    """GZIP compression and decompression"""

    @staticmethod
    def compress(data, level=6):
        """Compress data with gzip"""
        return gzip.compress(data, compresslevel=level)

    @staticmethod
    def decompress(data):
        """Decompress gzip data"""
        return gzip.decompress(data)


class ACXz:
    """XZ compression and decompression (LZMA2)"""

    @staticmethod
    def compress(data, preset=6):
        """Compress with XZ (LZMA2)
        preset: 0-9, higher = better compression but slower
        """
        return lzma.compress(data, preset=preset, filters=[{'id': lzma.FILTER_LZMA2}])

    @staticmethod
    def decompress(data):
        """Decompress XZ data"""
        return lzma.decompress(data)

    @staticmethod
    def compress_file(input_path, output_path, preset=6):
        """Compress file with XZ"""
        with open(input_path, 'rb') as f:
            data = f.read()
        compressed = ACXz.compress(data, preset)
        with open(output_path, 'wb') as f:
            f.write(compressed)

    @staticmethod
    def decompress_file(input_path, output_path):
        """Decompress XZ file"""
        with open(input_path, 'rb') as f:
            data = f.read()
        decompressed = ACXz.decompress(data)
        with open(output_path, 'wb') as f:
            f.write(decompressed)


class ACZstd:
    """Zstandard (zstd) compression and decompression"""

    @staticmethod
    def compress(data, level=3):
        """Compress with zstd
        level: 1-22, higher = better compression but slower
        """
        if not HAS_ZSTD:
            raise ImportError("zstandard not installed. Install with: pip install zstandard")

        cctx = zstd.ZstdCompressor(level=level)
        return cctx.compress(data)

    @staticmethod
    def decompress(data):
        """Decompress zstd data"""
        if not HAS_ZSTD:
            raise ImportError("zstandard not installed. Install with: pip install zstandard")

        dctx = zstd.ZstdDecompressor()
        return dctx.decompress(data)

    @staticmethod
    def compress_file(input_path, output_path, level=3):
        """Compress file with zstd"""
        with open(input_path, 'rb') as f:
            data = f.read()
        compressed = ACZstd.compress(data, level)
        with open(output_path, 'wb') as f:
            f.write(compressed)

    @staticmethod
    def decompress_file(input_path, output_path):
        """Decompress zstd file"""
        with open(input_path, 'rb') as f:
            data = f.read()
        decompressed = ACZstd.decompress(data)
        with open(output_path, 'wb') as f:
            f.write(decompressed)
