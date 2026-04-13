// ─────────────────────────────────────────────
//  STARSHINE ENGINE — main.cpp
//
//  This is the single translation unit that
//  pulls in the WASM bridge (which includes
//  all other headers through it).
//
//  The bridge.h contains all exported functions.
//  Everything else is header-only for now —
//  as the engine grows, split into .cpp files.
// ─────────────────────────────────────────────

#include "bridge.h"

// When running natively (not WASM), provide a test main
#ifdef STARSHINE_NATIVE
int main(int argc, char** argv) {
    printf("Starshine Engine — Native Test Build\n");

    // Test: ARC parser with a file if provided
    if(argc >= 2) {
        FILE* f = fopen(argv[1], "rb");
        if(f) {
            fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
            std::vector<Starshine::u8> buf(sz);
            fread(buf.data(),1,sz,f); fclose(f);

            using namespace Starshine;
            if(FileSys::Yaz0::isYaz0(buf.data(), buf.size())) {
                printf("Decompressing Yaz0 (%ld → %u bytes)...\n",
                       sz, FileSys::Yaz0::uncompressedSize(buf.data()));
                auto dec = FileSys::Yaz0::decompress(buf);
                if(dec) buf = std::move(*dec);
            }
            auto arc = FileSys::Archive::parse(std::move(buf));
            if(arc) {
                printf("ARC parsed: %zu files\n", arc->fileCount());
                arc->dump();
            } else {
                printf("ARC parse failed.\n");
            }
        }
    }

    // Test: BCSV parser
    printf("\nBCSV hash test:\n");
    printf("  hash('name') = 0x%08X\n", Starshine::FileSys::bcsvHash("name"));
    printf("  expected:      0x92C67EC5\n");

    // Test: Gravity system
    printf("\nGravity system test:\n");
    using namespace Starshine::Physics;
    GravityManager gm;
    auto sphere = std::make_shared<SphereGravity>();
    sphere->position = {0,0,0};
    sphere->radius   = 500.f;
    sphere->strength = -20.f;
    gm.addField(sphere);
    Vec3 testPoint = {600.f, 0.f, 0.f};
    Vec3 g = gm.calcGravity(testPoint);
    Vec3 up = gm.calcUp(testPoint);
    printf("  At (600,0,0): gravity=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f)\n",
           g.x,g.y,g.z, up.x,up.y,up.z);

    // Test: Mario controller
    printf("\nMario controller test:\n");
    Starshine::Actor::MarioController mario(Starshine::Game::Galaxy1);
    mario.setGravityManager(&gm);
    mario.setPosition({600.f, 510.f, 0.f});
    Starshine::Actor::MarioInput input;
    input.stick = {0.5f, -0.5f};
    mario.update(input, 1.f/60.f);
    auto pos = mario.physics().position;
    printf("  After 1 frame: pos=(%.2f,%.2f,%.2f) state=%d\n",
           pos.x,pos.y,pos.z, (int)mario.state());

    printf("\nAll tests passed.\n");
    return 0;
}
#endif
