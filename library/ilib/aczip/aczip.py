import gzip
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
    """ACZip v2 - Fast archiver with per-file compression"""

    @staticmethod
    def compress(path, parallel=True):
        """Compress directory: each file compressed individually, then packaged"""
        files = []
        for root, dirs, filenames in os.walk(path):
            for filename in filenames:
                filepath = os.path.join(root, filename)
                with open(filepath, 'rb') as f:
                    data = f.read()
                    rel_path = os.path.relpath(filepath, path)
                    files.append((rel_path, data))

        # Compress each file individually
        compressed_files = []

        if parallel and len(files) > 1:
            with ThreadPoolExecutor() as executor:
                results = executor.map(
                    lambda f: (f[0], ACZstd.compress(f[1], level=3)),
                    files
                )
                compressed_files = list(results)
        else:
            compressed_files = [(f[0], ACZstd.compress(f[1], level=3)) for f in files]

        # Build v2 format
        result = b'ACZ2'  # Magic header v2
        result += struct.pack('<I', len(compressed_files))  # File count

        for idx, (filepath, compressed) in enumerate(compressed_files):
            tag = ACZip._generate_tag(idx)
            result += tag.encode()  # 4-byte tag
            result += struct.pack('<I', len(compressed))  # Compressed size
            result += compressed  # Compressed data

        return result

    @staticmethod
    def decompress(data, output_path):
        """Decompress ACZip v2 archive"""
        if len(data) < 8 or data[:4] != b'ACZ2':
            raise ValueError("Invalid ACZip file")

        pos = 4
        file_count = struct.unpack('<I', data[pos:pos+4])[0]
        pos += 4

        os.makedirs(output_path, exist_ok=True)

        for _ in range(file_count):
            # Tag
            tag = data[pos:pos+4].decode()
            pos += 4

            # Compressed size
            comp_size = struct.unpack('<I', data[pos:pos+4])[0]
            pos += 4

            # Compressed data
            compressed = data[pos:pos+comp_size]
            pos += comp_size

            # Decompress
            file_data = gzip.decompress(compressed)

            # Write file
            file_path = os.path.join(output_path, tag)
            os.makedirs(os.path.dirname(file_path), exist_ok=True)

            with open(file_path, 'wb') as f:
                f.write(file_data)

    @staticmethod
    def compress_hdd(path):
        """Compress optimized for HDD (sequential)"""
        return ACZip.compress(path, parallel=False)

    @staticmethod
    def compress_sata(path):
        """Compress optimized for SATA (balanced, parallel)"""
        return ACZip.compress(path, parallel=True)

    @staticmethod
    def get_ratio(original_size, compressed_size):
        """Calculate compression ratio"""
        if original_size == 0:
            return 0.0
        return (compressed_size * 100.0) / original_size

    @staticmethod
    def _generate_tag(index):
        """Generate 4-byte tag for file index"""
        if index < 15:
            return f"0x{index:X}".ljust(4)[:4]
        folder = (index // 15) + 1
        file_idx = (index % 15) + 1
        return f"1x{folder:02d}.0x{file_idx:02X}"[:4]


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
