const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const tar = require('tar');

class ACZip {
    /**
     * Compress directory using 4-bit tagging protocol
     */
    static compress(dirPath, parallel = true) {
        const files = [];

        // Collect all files
        const walk = (dir) => {
            const items = fs.readdirSync(dir);
            for (const item of items) {
                const fullPath = path.join(dir, item);
                const stat = fs.statSync(fullPath);
                if (stat.isFile()) {
                    const data = fs.readFileSync(fullPath);
                    const relPath = path.relative(dirPath, fullPath);
                    files.push({ path: relPath, data });
                } else if (stat.isDirectory()) {
                    walk(fullPath);
                }
            }
        };
        walk(dirPath);

        // Serialize with 4-bit tags
        let serialized = Buffer.from('ACZP');  // Magic header
        for (let idx = 0; idx < files.length; idx++) {
            const { path: filepath, data } = files[idx];
            const tag = ACZip._generateTag(idx);
            serialized = Buffer.concat([
                serialized,
                Buffer.from(tag),
                Buffer.from(`${data.length}:`),
                data
            ]);
        }

        // Gzip compress
        return new Promise((resolve, reject) => {
            zlib.gzip(serialized, { level: 6 }, (err, compressed) => {
                if (err) reject(err);
                else resolve(compressed);
            });
        });
    }

    /**
     * Decompress ACZip archive
     */
    static decompress(data, outputPath) {
        return new Promise((resolve, reject) => {
            zlib.gunzip(data, (err, decompressed) => {
                if (err) return reject(err);

                const serialized = decompressed.toString('latin1');
                if (!serialized.startsWith('ACZP')) {
                    return reject(new Error('Invalid ACZip file'));
                }

                let pos = 4;
                const promises = [];

                while (pos < serialized.length) {
                    const colonPos = serialized.indexOf(':', pos + 4);
                    const tag = serialized.substring(pos, pos + 4);
                    const size = parseInt(serialized.substring(pos + 4, colonPos));
                    pos = colonPos + 1;

                    const fileData = Buffer.from(
                        serialized.substring(pos, pos + size),
                        'latin1'
                    );
                    pos += size;

                    const filePath = path.join(outputPath, tag);
                    const dir = path.dirname(filePath);

                    promises.push(
                        new Promise((res, rej) => {
                            fs.mkdir(dir, { recursive: true }, (err) => {
                                if (err) rej(err);
                                else {
                                    fs.writeFile(filePath, fileData, (err) => {
                                        if (err) rej(err);
                                        else res();
                                    });
                                }
                            });
                        })
                    );
                }

                Promise.all(promises).then(resolve).catch(reject);
            });
        });
    }

    /**
     * Compress optimized for HDD
     */
    static compressHDD(dirPath) {
        return ACZip.compress(dirPath, false);
    }

    /**
     * Compress optimized for SATA
     */
    static compressSATA(dirPath) {
        return ACZip.compress(dirPath, true);
    }

    /**
     * Calculate compression ratio
     */
    static getRatio(originalSize, compressedSize) {
        if (originalSize === 0) return 0;
        return (compressedSize * 100.0) / originalSize;
    }

    /**
     * Generate 4-bit tag for file index
     */
    static _generateTag(index) {
        if (index < 15) {
            return `0x${index.toString(16).toUpperCase()}`;
        }
        const folder = Math.floor(index / 15) + 1;
        const fileIdx = (index % 15) + 1;
        return `1x${folder.toString().padStart(2, '0')}.0x${fileIdx.toString(16).toUpperCase().padStart(2, '0')}`;
    }
}

class ACTar {
    /**
     * Create TAR archive
     */
    static create(dirPath) {
        return new Promise((resolve, reject) => {
            const chunks = [];
            const stream = tar.create(
                { gzip: false },
                [dirPath]
            );

            stream.on('data', (chunk) => chunks.push(chunk));
            stream.on('end', () => resolve(Buffer.concat(chunks)));
            stream.on('error', reject);
        });
    }

    /**
     * Extract TAR archive
     */
    static extract(data, outputPath) {
        return new Promise((resolve, reject) => {
            const stream = tar.extract({ cwd: outputPath });
            stream.on('finish', resolve);
            stream.on('error', reject);
            stream.end(data);
        });
    }
}

class ACGzip {
    /**
     * Compress with gzip
     */
    static compress(data, level = 6) {
        return new Promise((resolve, reject) => {
            zlib.gzip(data, { level }, (err, compressed) => {
                if (err) reject(err);
                else resolve(compressed);
            });
        });
    }

    /**
     * Decompress gzip data
     */
    static decompress(data) {
        return new Promise((resolve, reject) => {
            zlib.gunzip(data, (err, decompressed) => {
                if (err) reject(err);
                else resolve(decompressed);
            });
        });
    }
}

module.exports = { ACZip, ACTar, ACGzip };
