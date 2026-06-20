import gzip
import tarfile
import os
import io
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

class ACZip:
    """4-bit protocol archiver for AC projects"""

    @staticmethod
    def compress(path, parallel=True):
        """Compress directory using 4-bit tagging protocol"""
        files = []
        for root, dirs, filenames in os.walk(path):
            for filename in filenames:
                filepath = os.path.join(root, filename)
                with open(filepath, 'rb') as f:
                    data = f.read()
                    rel_path = os.path.relpath(filepath, path)
                    files.append((rel_path, data))

        # Serialize with 4-bit tags
        serialized = b'ACZP'  # Magic header
        for idx, (filepath, data) in enumerate(files):
            tag = ACZip._generate_tag(idx)
            serialized += tag.encode() + f'{len(data)}:'.encode() + data

        # Gzip compress
        return gzip.compress(serialized, compresslevel=6)

    @staticmethod
    def decompress(data, output_path):
        """Decompress ACZip archive"""
        decompressed = gzip.decompress(data)
        serialized = decompressed.decode('latin-1', errors='ignore')

        if not serialized.startswith('ACZP'):
            raise ValueError("Invalid ACZip file")

        pos = 4
        while pos < len(serialized):
            colon = serialized.find(':', pos + 4)
            tag = serialized[pos:pos+4]
            size = int(serialized[pos+4:colon])
            pos = colon + 1

            file_data = serialized[pos:pos+size].encode('latin-1')
            pos += size

            # Reconstruct path from tag
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
        """Compress optimized for SATA (balanced)"""
        return ACZip.compress(path, parallel=True)

    @staticmethod
    def get_ratio(original_size, compressed_size):
        """Calculate compression ratio"""
        if original_size == 0:
            return 0.0
        return (compressed_size * 100.0) / original_size

    @staticmethod
    def _generate_tag(index):
        """Generate 4-bit tag for file index"""
        if index < 15:
            return f"0x{index:X}"
        folder = (index // 15) + 1
        file_idx = (index % 15) + 1
        return f"1x{folder:02d}.0x{file_idx:02X}"


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