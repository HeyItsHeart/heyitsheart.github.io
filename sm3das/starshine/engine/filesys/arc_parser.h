#pragma once
// ─────────────────────────────────────────────
//  STARSHINE ENGINE — ARC (U8 Archive) Parser
//
//  Nintendo's U8 archive format is used throughout
//  GameCube and Wii games. Every stage, object, and
//  asset in Sunshine/Galaxy lives inside one.
//
//  Format reference:
//    Magic:    0x55AA382D ("U.8-")
//    Header:   big-endian, 32-byte
//    Node tree: directory + file nodes
//    String table: null-terminated names
//    Data block: raw file data
// ─────────────────────────────────────────────

#include "core_types.h"
#include <unordered_map>
#include <optional>

namespace Starshine {
namespace FileSys {

// ── Raw U8 header (on-disk layout, big-endian) ──
#pragma pack(push, 1)
struct U8Header {
    u32 magic;          // 0x55AA382D
    u32 rootNodeOffset; // offset to first node (usually 0x20)
    u32 headerSize;     // size of header + nodes + string table
    u32 dataOffset;     // offset where file data begins
    u8  padding[16];
};

struct U8Node {
    u8  type;           // 0x00 = file, 0x01 = directory
    u8  nameOffset[3];  // 3-byte offset into string table
    u32 dataOffset;     // for files: offset from data block start
                        // for dirs:  parent node index
    u32 size;           // for files: byte size
                        // for dirs:  index of first node after subtree
};
#pragma pack(pop)

// ── Parsed file entry ──────────────────────────
struct ArcFile {
    std::string      name;      // filename without path
    std::string      path;      // full path e.g. "/stage/bianco.bmd"
    const u8*        data;      // pointer into the archive buffer (no copy)
    u32              size;
};

// ── Parsed directory ──────────────────────────
struct ArcDir {
    std::string              name;
    std::string              path;
    std::vector<ArcDir>      subdirs;
    std::vector<ArcFile>     files;
};

// ── The main archive object ───────────────────
class Archive {
public:
    // Parse an in-memory U8 archive.
    // The buffer must remain alive as long as this Archive is used —
    // ArcFile::data points directly into it (zero-copy design).
    static std::optional<Archive> parse(std::vector<u8> buffer);

    // Look up a file by full path (case-insensitive, forward slashes)
    const ArcFile* findFile(const std::string& path) const;

    // Get root directory (for traversal)
    const ArcDir& root() const { return m_root; }

    // All files as a flat list (convenient for bulk loading)
    const std::vector<const ArcFile*>& allFiles() const { return m_flat; }

    // Debug: print directory tree to stdout
    void dump() const;

    // Total number of files
    size_t fileCount() const { return m_flat.size(); }

private:
    Archive() = default;

    std::vector<u8>                        m_buffer;
    ArcDir                                 m_root;
    std::unordered_map<std::string,size_t> m_index; // path → m_flatFiles index
    std::vector<const ArcFile*>            m_flat;
    std::vector<ArcFile>                   m_files; // owns the ArcFile objects

    static std::string normalizePath(const std::string& p);
    void buildIndex();
    static void dumpDir(const ArcDir& dir, int depth);
};

// ── Implementation ─────────────────────────────

inline std::optional<Archive> Archive::parse(std::vector<u8> buffer) {
    if(buffer.size() < sizeof(U8Header)) return std::nullopt;

    const u8* base = buffer.data();
    const U8Header* hdr = reinterpret_cast<const U8Header*>(base);

    // Validate magic
    if(readU32BE(base) != 0x55AA382D) {
        // Try Yaz0-decompressed — caller should decompress first
        return std::nullopt;
    }

    u32 rootNodeOffset  = readU32BE(base + 4);
    u32 headerSize      = readU32BE(base + 8);
    u32 dataOffset      = readU32BE(base + 12);

    const U8Node* nodes = reinterpret_cast<const U8Node*>(base + rootNodeOffset);

    // Root node tells us total node count
    u32 totalNodes = readU32BE(reinterpret_cast<const u8*>(&nodes[0].size));

    // String table immediately follows the node array
    const char* strTable = reinterpret_cast<const char*>(
        base + rootNodeOffset + totalNodes * sizeof(U8Node));

    // Data region
    const u8* dataBase = base + dataOffset;

    Archive arc;
    arc.m_buffer = std::move(buffer);
    base = arc.m_buffer.data();
    nodes = reinterpret_cast<const U8Node*>(base + rootNodeOffset);
    strTable = reinterpret_cast<const char*>(
        base + rootNodeOffset + totalNodes * sizeof(U8Node));
    dataBase = base + dataOffset;

    // Build tree recursively
    // We use a stack-based approach instead of recursion to avoid stack overflow
    // on deeply nested archives (Galaxy has some nasty ones)
    struct Frame {
        u32      nodeIdx;
        u32      endIdx;
        ArcDir*  dir;
        std::string pathPrefix;
    };

    arc.m_root.name = "";
    arc.m_root.path = "/";

    std::vector<Frame> stack;
    stack.push_back({0, totalNodes, &arc.m_root, "/"});

    u32 i = 1; // start at 1 (skip root)
    while(i < totalNodes) {
        const U8Node& node = nodes[i];

        // Pop frames that are done
        while(!stack.empty() && i >= stack.back().endIdx)
            stack.pop_back();

        ArcDir* currentDir = stack.empty() ? &arc.m_root : stack.back().dir;
        std::string prefix = stack.empty() ? "/" : stack.back().pathPrefix;

        // Read name
        u32 nameOff = (u32(node.nameOffset[0]) << 16) |
                      (u32(node.nameOffset[1]) << 8)  |
                       u32(node.nameOffset[2]);
        std::string name = strTable + nameOff;

        if(node.type == 0x01) {
            // Directory node
            u32 endNode = readU32BE(reinterpret_cast<const u8*>(&node.size));
            ArcDir dir;
            dir.name = name;
            dir.path = prefix + name + "/";
            currentDir->subdirs.push_back(std::move(dir));
            stack.push_back({i, endNode, &currentDir->subdirs.back(), prefix + name + "/"});
            i++;
        } else {
            // File node
            u32 fileOffset = readU32BE(reinterpret_cast<const u8*>(&node.dataOffset));
            u32 fileSize   = readU32BE(reinterpret_cast<const u8*>(&node.size));
            ArcFile f;
            f.name = name;
            f.path = prefix + name;
            f.data = dataBase + fileOffset;
            f.size = fileSize;
            arc.m_files.push_back(std::move(f));
            currentDir->files.push_back(arc.m_files.back());
            i++;
        }
    }

    arc.buildIndex();
    return arc;
}

inline void Archive::buildIndex() {
    m_flat.clear();
    m_index.clear();
    // Collect all files into flat list
    std::function<void(const ArcDir&)> collect = [&](const ArcDir& dir) {
        for(const auto& f : dir.files) {
            // Find it in m_files to get a stable pointer
            for(auto& mf : m_files) {
                if(mf.path == f.path) {
                    m_index[normalizePath(mf.path)] = m_flat.size();
                    m_flat.push_back(&mf);
                    break;
                }
            }
        }
        for(const auto& sub : dir.subdirs)
            collect(sub);
    };
    collect(m_root);
}

inline const ArcFile* Archive::findFile(const std::string& path) const {
    auto it = m_index.find(normalizePath(path));
    if(it == m_index.end()) return nullptr;
    return m_flat[it->second];
}

inline std::string Archive::normalizePath(const std::string& p) {
    std::string r = p;
    // Lowercase
    for(auto& c : r) c = (char)tolower((unsigned char)c);
    // Ensure leading slash
    if(r.empty() || r[0] != '/') r = "/" + r;
    return r;
}

inline void Archive::dump() const {
    dumpDir(m_root, 0);
}

inline void Archive::dumpDir(const ArcDir& dir, int depth) {
    std::string indent(depth*2, ' ');
    printf("%s[%s]\n", indent.c_str(), dir.name.empty() ? "/" : dir.name.c_str());
    for(const auto& f : dir.files)
        printf("%s  %s (%u bytes)\n", indent.c_str(), f.name.c_str(), f.size);
    for(const auto& sub : dir.subdirs)
        dumpDir(sub, depth+1);
}

} // namespace FileSys
} // namespace Starshine
