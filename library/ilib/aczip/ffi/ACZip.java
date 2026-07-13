import com.sun.jna.*;
import com.sun.jna.ptr.IntByReference;
import java.io.File;

public class ACZip {
    public interface ACZipLib extends Library {
        ACZipLib INSTANCE = Native.load("aczip", ACZipLib.class);

        class ByteArray extends Structure {
            public Pointer data;
            public long size;

            @Override
            protected java.util.List<String> getFieldOrder() {
                return java.util.Arrays.asList("data", "size");
            }
        }

        ByteArray ac_zip_compress(String path, int parallel);
        int ac_zip_decompress(byte[] data, long size, String outputPath);
        ByteArray ac_zip_compress_hdd(String path);
        ByteArray ac_zip_compress_sata(String path);
        double ac_get_compression_ratio(long original, long compressed);
        void ac_free_bytes(ByteArray arr);
    }

    public static byte[] compress(String path, boolean parallel) throws Exception {
        ByteArray result = ACZipLib.INSTANCE.ac_zip_compress(
            path,
            parallel ? 1 : 0
        );

        byte[] bytes = result.data.getByteArray(0, (int)result.size);
        ACZipLib.INSTANCE.ac_free_bytes(result);
        return bytes;
    }

    public static void decompress(byte[] data, String outputPath) throws Exception {
        int ret = ACZipLib.INSTANCE.ac_zip_decompress(data, data.length, outputPath);
        if (ret != 0) {
            throw new Exception("Decompression failed");
        }
    }

    public static byte[] compressHDD(String path) throws Exception {
        ByteArray result = ACZipLib.INSTANCE.ac_zip_compress_hdd(path);
        byte[] bytes = result.data.getByteArray(0, (int)result.size);
        ACZipLib.INSTANCE.ac_free_bytes(result);
        return bytes;
    }

    public static byte[] compressSATA(String path) throws Exception {
        ByteArray result = ACZipLib.INSTANCE.ac_zip_compress_sata(path);
        byte[] bytes = result.data.getByteArray(0, (int)result.size);
        ACZipLib.INSTANCE.ac_free_bytes(result);
        return bytes;
    }

    public static double getRatio(long originalSize, long compressedSize) {
        return ACZipLib.INSTANCE.ac_get_compression_ratio(originalSize, compressedSize);
    }
}
