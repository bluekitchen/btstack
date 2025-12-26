// Efficient in-place reader for binary data
class BufferedReader {
    constructor(port) {
        this.port = port;
        this.reader = port.readable.getReader();
        this.buffer = new Uint8Array(0); // leftover bytes from previous reads
    }

    /**
     * Fill a provided Uint8Array starting at 'offset' with 'length' bytes.
     * Returns a Promise that resolves when exactly 'length' bytes are read.
     * @param {Uint8Array} target - buffer to fill
     * @param {number} offset - start offset in target
     * @param {number} length - number of bytes to read
     */
    async readInto(target, offset = 0, length = target.length) {
        if (offset + length > target.length) {
            throw new RangeError("Target buffer is too small for requested read");
        }

        let filled = 0;

        // Use leftover bytes first
        if (this.buffer.length > 0) {
            const toCopy = Math.min(length, this.buffer.length);
            target.set(this.buffer.subarray(0, toCopy), offset);
            offset += toCopy;
            filled += toCopy;
            this.buffer = this.buffer.subarray(toCopy);
        }

        while (filled < length) {
            const { value, done } = await this.reader.read();
            if (done) {
                // port closed exit
                break;
            }

            if (value) {
                const needed = length - filled;

                if (value.length <= needed) {
                    // Can copy entire chunk
                    target.set(value, offset);
                    offset += value.length;
                    filled += value.length;
                } else {
                    // Only need part of this chunk; keep remainder in internal buffer
                    target.set(value.subarray(0, needed), offset);
                    offset += needed;
                    filled += needed;
                    this.buffer = value.subarray(needed); // save leftover for next read
                }
            }
        }

        return filled;
    }

    async close() {
        await this.reader.cancel();
        await this.reader.releaseLock();
    }
}

globalThis.BufferedReader = BufferedReader;
