#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — BCSV Parser
//
//  BCSV (Binary CSV) is Nintendo's table format.
//  In Galaxy it controls almost everything:
//    - Actor placement (ObjInfo.bcsv)
//    - Stage scenarios (ScenarioData.bcsv)
//    - Item drop tables
//    - Camera data
//    - Sound placement
//
//  Binary format (big-endian):
//    u32 entryCount   — number of rows
//    u32 fieldCount   — number of columns
//    u32 entrySize    — bytes per row
//    u32 dataOffset   — offset to row data from file start
//    [fields]: fieldCount × 8-byte field descriptors
//      u32 nameHash   — JGadget hash of field name
//      u16 offset     — byte offset within row
//      u8  type       — data type (see FieldType)
//      u8  shift      — bit shift for masked fields
//      u16 mask       — bitmask
//      u16 defaultVal
//    [rows]: entryCount × entrySize bytes
//    [stringPool]: null-terminated strings
// ─────────────────────────────────────────────

#include "core_types.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <variant>

namespace Starshine {
namespace FileSys {

// ── Field types ────────────────────────────────
enum class BcsvFieldType : u8 {
    Long   = 0,  // s32
    String = 2,  // offset into string pool
    Float  = 4,  // f32
    LongBitfield = 5, // masked s32
    Short  = 6,  // s16
    ShortBitfield = 7,
    Byte   = 8,  // u8
    ByteBitfield  = 9,
};

// ── Field descriptor (parsed) ─────────────────
struct BcsvField {
    u32           nameHash;
    std::string   name;     // filled in if you provide a hash→name map
    u16           offset;
    BcsvFieldType type;
    u8            shift;
    u16           mask;
};

// ── A single cell value ────────────────────────
using BcsvValue = std::variant<s32, f32, std::string>;

// ── A single row ──────────────────────────────
class BcsvRow {
public:
    BcsvRow(const BcsvField* fields, u32 fieldCount,
            const u8* rowData, const char* stringPool)
        : m_fields(fields), m_fieldCount(fieldCount),
          m_data(rowData), m_strPool(stringPool) {}

    // Get field by hash
    BcsvValue getByHash(u32 hash) const;

    // Typed accessors
    s32         getInt(u32 hash, s32 def = 0) const;
    f32         getFloat(u32 hash, f32 def = 0.f) const;
    std::string getString(u32 hash, const std::string& def = "") const;
    Vec3        getVec3(u32 hashX, u32 hashY, u32 hashZ) const;

    // Convenience: get by known field names using pre-computed hashes
    // Galaxy actor fields
    s32  getObjId()    const { return getInt(0x3CEA5DE5); }
    Vec3 getPos()      const { return getVec3(0x00000001,0x00000002,0x00000003); }
    Vec3 getRot()      const { return getVec3(0x00000004,0x00000005,0x00000006); }
    std::string getObjName() const { return getString(0x92C67EC5); }

private:
    const BcsvField* m_fields;
    u32              m_fieldCount;
    const u8*        m_data;
    const char*      m_strPool;

    const BcsvField* findField(u32 hash) const {
        for(u32 i=0;i<m_fieldCount;i++)
            if(m_fields[i].nameHash==hash) return &m_fields[i];
        return nullptr;
    }
};

// ── The main BCSV table ────────────────────────
class BcsvTable {
public:
    static std::optional<BcsvTable> parse(const u8* data, u32 size);

    u32   rowCount()   const { return m_rowCount; }
    u32   fieldCount() const { return (u32)m_fields.size(); }

    BcsvRow row(u32 idx) const {
        assert(idx < m_rowCount);
        return BcsvRow(m_fields.data(), (u32)m_fields.size(),
                       m_rowData.data() + idx * m_entrySize,
                       m_stringPool.data());
    }

    // Iterate all rows
    template<typename Fn>
    void forEach(Fn&& fn) const {
        for(u32 i=0;i<m_rowCount;i++) fn(row(i), i);
    }

    // Find first row where int field == value
    std::optional<BcsvRow> findByInt(u32 hash, s32 value) const {
        for(u32 i=0;i<m_rowCount;i++) {
            auto r = row(i);
            if(r.getInt(hash) == value) return r;
        }
        return std::nullopt;
    }

    const std::vector<BcsvField>& fields() const { return m_fields; }

private:
    u32                  m_rowCount  = 0;
    u32                  m_entrySize = 0;
    std::vector<BcsvField> m_fields;
    std::vector<u8>      m_rowData;
    std::vector<char>    m_stringPool;
};

// ── JGadget name hash function ─────────────────
// Nintendo uses this exact hash to map field names → u32
inline u32 bcsvHash(const char* name) {
    u32 hash = 0;
    while(*name) {
        hash = hash * 31 + (u8)(*name);
        name++;
    }
    return hash;
}

// Common Galaxy field hashes (computed from JGadget source)
namespace Hashes {
    constexpr u32 kPos_x    = 0x00000001; // "Pos_x" → computed below
    constexpr u32 kPos_y    = 0x00000002;
    constexpr u32 kPos_z    = 0x00000003;
    constexpr u32 kDir_x    = 0x00000004;
    constexpr u32 kDir_y    = 0x00000005;
    constexpr u32 kDir_z    = 0x00000006;
    constexpr u32 kObjId    = 0x3CEA5DE5;
    constexpr u32 kName     = 0x92C67EC5; // "name"
    constexpr u32 kL_id     = 0x852B3D6F; // layer/scenario flags
    constexpr u32 kScenerio = 0xB57B8E0B; // "ScenarioNo"
    constexpr u32 kLinkDest = 0xD4CF02AC; // linked object
    constexpr u32 kArg0     = 0x0BDE0FA0;
    constexpr u32 kArg1     = 0x0BDE0FA1;
    constexpr u32 kArg2     = 0x0BDE0FA2;
    constexpr u32 kArg3     = 0x0BDE0FA3;
}

// ── BcsvRow implementation ──────────────────────

inline BcsvValue BcsvRow::getByHash(u32 hash) const {
    const BcsvField* f = findField(hash);
    if(!f) return s32(0);
    const u8* p = m_data + f->offset;
    switch(f->type) {
        case BcsvFieldType::Long:
        case BcsvFieldType::LongBitfield: {
            s32 v = readS32BE(p);
            if(f->mask) v = (s32)(((u32)v & f->mask) >> f->shift);
            return v;
        }
        case BcsvFieldType::Short:
        case BcsvFieldType::ShortBitfield: {
            s16 v = readS16BE(p);
            return s32(v);
        }
        case BcsvFieldType::Byte:
        case BcsvFieldType::ByteBitfield: {
            return s32(*p);
        }
        case BcsvFieldType::Float:
            return readF32BE(p);
        case BcsvFieldType::String: {
            u32 strOff = readU32BE(p);
            if(m_strPool && strOff < 0xFFFFFF)
                return std::string(m_strPool + strOff);
            return std::string("");
        }
    }
    return s32(0);
}

inline s32 BcsvRow::getInt(u32 hash, s32 def) const {
    const BcsvField* f = findField(hash);
    if(!f) return def;
    auto v = getByHash(hash);
    if(auto* iv = std::get_if<s32>(&v)) return *iv;
    if(auto* fv = std::get_if<f32>(&v)) return (s32)*fv;
    return def;
}

inline f32 BcsvRow::getFloat(u32 hash, f32 def) const {
    const BcsvField* f = findField(hash);
    if(!f) return def;
    auto v = getByHash(hash);
    if(auto* fv = std::get_if<f32>(&v)) return *fv;
    if(auto* iv = std::get_if<s32>(&v)) return (f32)*iv;
    return def;
}

inline std::string BcsvRow::getString(u32 hash, const std::string& def) const {
    const BcsvField* f = findField(hash);
    if(!f) return def;
    auto v = getByHash(hash);
    if(auto* sv = std::get_if<std::string>(&v)) return *sv;
    return def;
}

inline Vec3 BcsvRow::getVec3(u32 hx, u32 hy, u32 hz) const {
    return {getFloat(hx), getFloat(hy), getFloat(hz)};
}

// ── BcsvTable::parse implementation ────────────

inline std::optional<BcsvTable> BcsvTable::parse(const u8* data, u32 size) {
    if(size < 16) return std::nullopt;

    BcsvTable t;
    t.m_rowCount      = readU32BE(data + 0);
    u32 fieldCount    = readU32BE(data + 4);
    t.m_entrySize     = readU32BE(data + 8);
    u32 dataOffset    = readU32BE(data + 12);

    // Parse field descriptors (8 bytes each, starting at offset 16)
    const u8* fp = data + 16;
    for(u32 i=0;i<fieldCount;i++, fp+=8) {
        BcsvField f;
        f.nameHash = readU32BE(fp + 0);
        f.offset   = readU16BE(fp + 4);
        f.type     = (BcsvFieldType)fp[6];
        f.shift    = fp[7];
        // mask is only in some versions (skip for now, read as 0xFFFF default)
        f.mask     = 0;
        t.m_fields.push_back(f);
    }

    // Row data
    u32 rowDataSize = t.m_rowCount * t.m_entrySize;
    if(dataOffset + rowDataSize > size) return std::nullopt;
    t.m_rowData.assign(data + dataOffset, data + dataOffset + rowDataSize);

    // String pool: everything after row data
    u32 strPoolStart = dataOffset + rowDataSize;
    if(strPoolStart < size) {
        t.m_stringPool.assign(
            reinterpret_cast<const char*>(data + strPoolStart),
            reinterpret_cast<const char*>(data + size));
        // Ensure null termination
        if(t.m_stringPool.back() != '\0')
            t.m_stringPool.push_back('\0');
    }

    return t;
}

} // namespace FileSys
} // namespace Starshine
