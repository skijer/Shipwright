#include <cstdint>
#include <initializer_list>

#include "test_require.h"

#include "../mods/transformation_masks/assets/mm_display_list_patch.h"

struct ResolveFixture {
    uint64_t nestedHash;
    uintptr_t nestedPointer;
    uint64_t vertexHash;
    uintptr_t vertexPointer;
    size_t vertexSize;
    uint64_t textureHash;
    uintptr_t texturePointer;
};

static uintptr_t ResolveResource(void* context, MmDisplayListReferenceKind kind, uint64_t hash, size_t* resourceSize) {
    ResolveFixture* fixture = static_cast<ResolveFixture*>(context);

    if (kind == MM_DISPLAY_LIST_REFERENCE_RENDER_MODE) {
        return hash == 0 ? UINT64_C(0x30000000) : hash == 2 ? UINT64_C(0x30000020) : 0;
    }

    if (kind == MM_DISPLAY_LIST_REFERENCE_NESTED && hash == fixture->nestedHash) {
        return fixture->nestedPointer;
    }
    if (kind == MM_DISPLAY_LIST_REFERENCE_VERTEX && hash == fixture->vertexHash) {
        *resourceSize = fixture->vertexSize;
        return fixture->vertexPointer;
    }
    if (kind == MM_DISPLAY_LIST_REFERENCE_TEXTURE && hash == fixture->textureHash) {
        return fixture->texturePointer;
    }
    return 0;
}

int main() {
    uint8_t arrayVertices[64] = {};
    MmDisplayListVertexResourceView vertexView = {};

    REQUIRE(MmDisplayList_SelectVertexResource(nullptr, 0, arrayVertices, sizeof(arrayVertices), true, &vertexView));
    REQUIRE(vertexView.pointer == reinterpret_cast<uintptr_t>(arrayVertices));
    REQUIRE(vertexView.size == sizeof(arrayVertices));
    REQUIRE(!MmDisplayList_SelectVertexResource(nullptr, 0, arrayVertices, sizeof(arrayVertices), false, &vertexView));

    constexpr uint64_t nestedHash = UINT64_C(0x0123456789ABCDEF);
    constexpr uint64_t vertexHash = UINT64_C(0xFEDCBA9876543210);
    constexpr uint64_t textureHash = UINT64_C(0x54865CA6217340F0);
    ResolveFixture fixture = { nestedHash, UINT64_C(0x12345000), vertexHash,          UINT64_C(0x20000000),
                               0x100,      textureHash,          UINT64_C(0x40000000) };
    MmDisplayListCommand commands[] = {
        { UINT32_C(0x31010000), 0 },
        { UINT32_C(0x01234567), UINT32_C(0x89ABCDEF) },
        { UINT32_C(0x32001002), UINT32_C(0x20) },
        { UINT32_C(0xFEDCBA98), UINT32_C(0x76543210) },
        { UINT32_C(0xDF000000), 0 },
    };
    MmDisplayListPatchStats stats = {};

    REQUIRE(MmDisplayList_PatchCommands(commands, 5, ResolveResource, &fixture, &stats));
    REQUIRE(commands[0].w0 == UINT32_C(0xDE010000));
    REQUIRE(commands[0].w1 == fixture.nestedPointer);
    REQUIRE(commands[1].w0 == 0);
    REQUIRE(commands[1].w1 == 0);
    // The renderer adds first-word w1 as a BYTE offset into the resolved array.
    REQUIRE(commands[2].w0 == UINT32_C(0x01001002));
    REQUIRE(commands[2].w1 == fixture.vertexPointer + 0x20);
    REQUIRE(commands[3].w0 == 0);
    REQUIRE(commands[3].w1 == 0);
    REQUIRE(stats.nestedPatched == 1);
    REQUIRE(stats.verticesPatched == 1);
    REQUIRE(stats.unresolved == 0);
    REQUIRE(stats.malformed == 0);

    // Exact texture-hash command shape used by gSkullKidTorsoDL. A strict MM
    // graph must not leave this for the global archive resolver: mm.o2r is
    // loaded through the MM archive path and the global lookup returns null.
    MmDisplayListCommand texture[] = {
        { UINT32_C(0x20000000), UINT32_C(0xBEEFBEEF) },
        { UINT32_C(0x54865CA6), UINT32_C(0x217340F0) },
        { UINT32_C(0xDF000000), 0 },
    };
    stats = {};
    REQUIRE(MmDisplayList_PatchCommands(texture, 3, ResolveResource, &fixture, &stats));
    // The strict resolver returns a retained filepath alias for full metadata.
    REQUIRE(texture[0].w0 == UINT32_C(0x25000000));
    REQUIRE(texture[0].w1 == fixture.texturePointer);
    REQUIRE(texture[1].w0 == 0);
    REQUIRE(texture[1].w1 == 0);

    MmDisplayListCommand unresolved[] = {
        { UINT32_C(0x31000000), 0 },
        { UINT32_C(0x11111111), UINT32_C(0x22222222) },
        { UINT32_C(0xDF000000), 0 },
    };
    stats = {};
    REQUIRE(MmDisplayList_PatchCommands(unresolved, 3, ResolveResource, &fixture, &stats));
    REQUIRE(unresolved[0].w0 == UINT32_C(0x31000000));
    REQUIRE(stats.unresolved == 1);

    MmDisplayListCommand outOfBounds[] = {
        { UINT32_C(0x32011004), UINT32_C(0xF0) },
        { UINT32_C(0xFEDCBA98), UINT32_C(0x76543210) },
        { UINT32_C(0xDF000000), 0 },
    };
    stats = {};
    REQUIRE(!MmDisplayList_PatchCommands(outOfBounds, 3, ResolveResource, &fixture, &stats));
    REQUIRE(stats.malformed == 1);
    REQUIRE(outOfBounds[0].w0 == UINT32_C(0x32011004));

    for (uintptr_t offset : { uintptr_t(0x100), uintptr_t(0x101), UINTPTR_MAX }) {
        MmDisplayListCommand overrun[] = {
            { UINT32_C(0x32001002), offset },
            { UINT32_C(0xFEDCBA98), UINT32_C(0x76543210) },
            { UINT32_C(0xDF000000), 0 },
        };
        REQUIRE(!MmDisplayList_PatchCommands(overrun, 3, ResolveResource, &fixture, &stats));
    }
    MmDisplayListCommand lastVertex[] = {
        { UINT32_C(0x32001002), 0xF0 },
        { UINT32_C(0xFEDCBA98), UINT32_C(0x76543210) },
        { UINT32_C(0xDF000000), 0 },
    };
    REQUIRE(MmDisplayList_PatchCommands(lastVertex, 3, ResolveResource, &fixture, &stats));
    REQUIRE(lastVertex[0].w1 == fixture.vertexPointer + 0xF0);

    MmDisplayListCommand malformed[] = { { UINT32_C(0x31000000), 0 } };
    stats = {};
    REQUIRE(!MmDisplayList_PatchCommands(malformed, 1, ResolveResource, &fixture, &stats));
    REQUIRE(stats.malformed == 1);
    // Exact MM segment-0x0C render-mode call; opaque indices must resolve
    // explicitly, independently of inherited segment state.
    MmDisplayListCommand cull[] = {
        { UINT32_C(0x3D000000), UINT32_C(0x0C000002) },
        { UINT32_C(0xDF000000), 0 },
    };
    stats = {};
    REQUIRE(MmDisplayList_PatchCommands(cull, 2, ResolveResource, &fixture, &stats));
    REQUIRE(cull[0].w0 == UINT32_C(0xDE000000));
    REQUIRE(cull[0].w1 == UINT64_C(0x30000020));
    REQUIRE(stats.renderModePatched == 1);
    MmDisplayListCommand backCull[] = {
        { UINT32_C(0x3D010000), UINT32_C(0x0C000000) },
        { UINT32_C(0xDF000000), 0 },
    };
    REQUIRE(MmDisplayList_PatchCommands(backCull, 2, ResolveResource, &fixture, &stats));
    REQUIRE(backCull[0].w0 == UINT32_C(0xDE010000));
    REQUIRE(backCull[0].w1 == UINT64_C(0x30000000));
    MmDisplayListCommand invalidCull[] = {
        { UINT32_C(0x3D000000), UINT32_C(0x0C000003) },
        { UINT32_C(0xDF000000), 0 },
    };
    REQUIRE(!MmDisplayList_PatchCommands(invalidCull, 2, ResolveResource, &fixture, &stats));
    REQUIRE(stats.malformed == 1);
    // A hash payload that looks like G_DL_INDEX is data, not a cull call.
    MmDisplayListCommand payload[] = {
        { UINT32_C(0x33000000), 0 },
        { UINT32_C(0x3D000000), UINT32_C(0x0C000002) },
        { UINT32_C(0xDF000000), 0 },
    };
    REQUIRE(MmDisplayList_PatchCommands(payload, 3, ResolveResource, &fixture, &stats));
    REQUIRE(payload[1].w0 == UINT32_C(0x3D000000));
    REQUIRE(stats.renderModePatched == 0);
    return 0;
}
