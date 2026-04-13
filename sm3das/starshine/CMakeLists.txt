cmake_minimum_required(VERSION 3.16)
project(Starshine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Source files ────────────────────────────────
set(ENGINE_SOURCES
    engine/wasm/main.cpp          # entry point (includes bridge.h)
)

add_executable(starshine ${ENGINE_SOURCES})

target_include_directories(starshine PRIVATE engine)

# ── Emscripten flags ────────────────────────────
if(EMSCRIPTEN)
    # Exported functions (all our C bridge functions)
    set(EXPORTED_FUNCTIONS
        "_starshine_init"
        "_starshine_resize"
        "_starshine_frame"
        "_starshine_input"
        "_starshine_load_arc"
        "_starshine_add_gravity"
        "_starshine_clear_gravity"
        "_starshine_set_mario_pos"
        "_starshine_get_mario_pos"
        "_starshine_get_mario_state"
        "_starshine_get_fludd_water"
        "_starshine_dump_scene"
        "_starshine_shutdown"
        "_malloc"
        "_free"
    )
    string(JOIN "," EXPORTED_FUNCTIONS_STR ${EXPORTED_FUNCTIONS})

    target_link_options(starshine PRIVATE
        # Output as JS module usable with Emscripten's Module pattern
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME='StarshineWasm'"
        # Exported C functions
        "SHELL:-s EXPORTED_FUNCTIONS='[${EXPORTED_FUNCTIONS_STR}]'"
        # Runtime exports we need from JS
        "SHELL:-s EXPORTED_RUNTIME_METHODS=['FS','MEMFS','UTF8ToString','stringToUTF8','lengthBytesUTF8']"
        # WebGL2 support (= GLES3)
        "SHELL:-s USE_WEBGL2=1"
        "SHELL:-s FULL_ES3=1"
        "SHELL:-s MIN_WEBGL_VERSION=2"
        "SHELL:-s MAX_WEBGL_VERSION=2"
        # Memory: 256MB initial, can grow to 512MB
        "SHELL:-s INITIAL_MEMORY=268435456"
        "SHELL:-s MAXIMUM_MEMORY=536870912"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        # Virtual filesystem for loading ARC files
        "SHELL:-s FORCE_FILESYSTEM=1"
        # Async loading support
        "SHELL:-s ASYNCIFY=1"
        # Pthreads for potential future audio/decode threads
        # "SHELL:-s USE_PTHREADS=1"
        # Output file
        "SHELL:-o starshine.js"
    )

    # Optimization flags
    target_compile_options(starshine PRIVATE
        -O3
        -fno-exceptions     # smaller binary, we use optional<> instead
        # -fno-rtti         # uncomment if no dynamic_cast needed
    )

    # Source maps for debugging (remove for release)
    # target_link_options(starshine PRIVATE "SHELL:-g")

endif()

# ── Native debug build (no Emscripten) ─────────
# Build natively to test parsers without browser
if(NOT EMSCRIPTEN)
    target_compile_options(starshine PRIVATE -O0 -g -fsanitize=address)
    target_link_options(starshine PRIVATE -fsanitize=address)
    target_compile_definitions(starshine PRIVATE STARSHINE_NATIVE=1)
endif()
