#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — Yaz0 Decompressor
//
//  Every ARC file in Sunshine and Galaxy is wrapped
//  in Yaz0 compression before being loaded. This must
//  run before the ARC parser sees any data.
//
//  Yaz0 format:
//    Magic:       "Yaz0" (4 bytes)
//    Uncompressed size: u32 big-endian
//    Padding:     8 bytes
//    Data:        compressed stream
//
//  Compression algorithm: LZSS variant
//    - Group header byte: 8 bits, MSB first
//      1 = literal byte follows
//      0 = back-reference (2-3 bytes)
//    - Back-reference: 2 bytes
//      High nibble of byte0 = copy length - 2 (if 0, read extra length byte)
//      Remaining 12 bits = copy offset from current pos
// ─────────────────────────────────────────────

#include "core_types.h"
#include <vector>
#include <optional>

namespace Starshine {
namespace FileSys {

class Yaz0 {
public:
    // Returns decompressed data, or nullopt if not Yaz0 / corrupt
    static std::optional<std::vector<u8>> decompress(const u8* src, size_t srcSize) {
        if(srcSize < 16) return std::nullopt;

        // Check magic "Yaz0"
        if(src[0]!='Y' || src[1]!='a' || src[2]!='z' || src[3]!='0')
            return std::nullopt;

        u32 uncompSize = readU32BE(src + 4);
        std::vector<u8> dst(uncompSize);

        const u8* in  = src + 16;   // skip 16-byte header
        const u8* end = src + srcSize;
        u8*       out = dst.data();
        u8*       outEnd = out + uncompSize;

        while(out < outEnd && in < end) {
            u8 groupHeader = *in++;

            for(int bit = 7; bit >= 0 && out < outEnd && in < end; bit--) {
                if(groupHeader & (1 << bit)) {
                    // Literal byte
                    *out++ = *in++;
                } else {
                    // Back-reference
                    if(in + 1 >= end) break;
                    u8 b0 = *in++;
                    u8 b1 = *in++;

                    u32 offset = (((u32)(b0 & 0x0F) << 8) | b1) + 1;
                    u32 length;
                    u8  nibble = (b0 >> 4) & 0x0F;

                    if(nibble == 0) {
                        // Extra length byte follows
                        if(in >= end) break;
                        length = (u32)(*in++) + 0x12;
                    } else {
                        length = (u32)nibble + 2;
                    }

                    // Copy from already-written output
                    const u8* copyFrom = out - offset;
                    if(copyFrom < dst.data()) break; // corrupt

                    for(u32 i = 0; i < length && out < outEnd; i++)
                        *out++ = copyFrom[i];
                }
            }
        }

        return dst;
    }

    // Convenience: decompress a vector
    static std::optional<std::vector<u8>> decompress(const std::vector<u8>& src) {
        return decompress(src.data(), src.size());
    }

    // Check if data starts with Yaz0 magic without decompressing
    static bool isYaz0(const u8* data, size_t size) {
        return size >= 4 &&
               data[0]=='Y' && data[1]=='a' && data[2]=='z' && data[3]=='0';
    }

    // Get uncompressed size from header without decompressing
    static u32 uncompressedSize(const u8* data) {
        return readU32BE(data + 4);
    }
};

} // namespace FileSys
} // namespace Starshine
