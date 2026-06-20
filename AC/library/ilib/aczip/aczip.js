const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const tar = require('tar');

class ACZip {
    /**
     * Compress directory: each file compressed individually in parallel
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

        // Compress each file individually
        return new Promise((resolve, reject) => {
            const compressedFiles = [];
            let completed = 0;

            if (parallel && files.length > 1) {
                // Parallel compression
                files.forEach((file, idx) => {
                    zlib.gzip(file.data, { level: 6 }, (err, compressed) => {
                        if (err) return reject(err);

                        compressedFiles[idx] = {
                            tag: ACZip._generateTag(idx),
                            compressed
                        };

                        if (++completed === files.length) {
                            resolve(ACZip._buildArchive(compressedFiles));
                        }
                    });
                });
            } else {
                // Sequential compression
                const compress = (idx) => {
                    if (idx >= files.length) {
                        return resolve(ACZip._buildArchive(compressedFiles));
                    }

                    zlib.gzip(files[idx].data, { level: 6 }, (err, compressed) => {
                        if (err) return reject(err);

                        compressedFiles.push({
                            tag: ACZip._generateTag(idx),
                            compressed
                        });

                        compress(idx + 1);
                    });
                };
                compress(0);
            }
        });
    }

    /**
     * Decompress ACZip v2 archive
     */
    static decompress(data, outputPath) {
        return new Promise((resolve, reject) => {
            // Check magic header
            if (data.length < 8 || data.toString('utf8', 0, 4) !== 'ACZ2') {
                return reject(new Error('Invalid ACZip file'));
            }

            let pos = 4;
            const fileCount = data.readUInt32LE(pos);
            pos += 4;

            fs.mkdir(outputPath, { recursive: true }, (err) => {
                if (err) return reject(err);

                const promises = [];

                for (let i = 0; i < fileCount; i++) {
                    // Tag
                    const tag = data.toString('utf8', pos, pos + 4);
                    pos += 4;

                    // Compressed size
                    const compSize = data.readUInt32LE(pos);
                    pos += 4;

                    // Compressed data
                    const compressed = data.slice(pos, pos + compSize);
                    pos += compSize;

                    // Decompress
                    promises.push(
                        new Promise((res, rej) => {
                            zlib.gunzip(compressed, (err, decompressed) => {
                                if (err) return rej(err);

                                const filePath = path.join(outputPath, tag);
                                fs.mkdir(path.dirname(filePath), { recursive: true }, (err) => {
                                    if (err) return rej(err);
                                    fs.writeFile(filePath, decompressed, (err) => {
                                        if (err) rej(err);
                                        else res();
                                    });
                                });
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
     * Generate 4-byte tag for file index
     */
    static _generateTag(index) {
        if (index < 15) {
            return `0x${index.toString(16).toUpperCase()}`.padEnd(4, ' ');
        }
        const folder = Math.floor(index / 15) + 1;
        const fileIdx = (index % 15) + 1;
        return `1x${folder.toString().padStart(2, '0')}.0x${fileIdx.toString(16).toUpperCase().padStart(2, '0')}`.slice(0, 4);
    }

    /**
     * Build v2 archive from compressed files
     */
    static _buildArchive(compressedFiles) {
        let result = Buffer.from('ACZ2');
        result = Buffer.concat([result, Buffer.alloc(4)]);
        result.writeUInt32LE(compressedFiles.length, 4);

        for (const file of compressedFiles) {
            const tag = Buffer.from(file.tag, 'utf8');
            const sizeBuffer = Buffer.alloc(4);
            sizeBuffer.writeUInt32LE(file.compressed.length);

            result = Buffer.concat([result, tag, sizeBuffer, file.compressed]);
        }

        return result;
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
