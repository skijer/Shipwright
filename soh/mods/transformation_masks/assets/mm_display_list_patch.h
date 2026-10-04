#ifndef MM_DISPLAY_LIST_PATCH_H
#define MM_DISPLAY_LIST_PATCH_H

#include <cstddef>
#include <cstdint>

struct MmDisplayListCommand {
    uint32_t w0;
    uintptr_t w1;
};

enum MmDisplayListReferenceKind {
    MM_DISPLAY_LIST_REFERENCE_NESTED,
    MM_DISPLAY_LIST_REFERENCE_VERTEX,
    // MM segment-0x0C render-mode command index (0 or 2), not a hash.
    MM_DISPLAY_LIST_REFERENCE_RENDER_MODE,
    MM_DISPLAY_LIST_REFERENCE_TEXTURE,
};

struct MmDisplayListPatchStats {
    size_t nestedPatched;
    size_t verticesPatched;
    size_t unresolved;
    size_t malformed;
    size_t renderModePatched;
    size_t texturesPatched;
};

struct MmDisplayListVertexResourceView {
    uintptr_t pointer;
    size_t size;
};

bool MmDisplayList_SelectVertexResource(void* fastVertexPointer, size_t fastVertexSize, void* arrayPointer,
                                        size_t arraySize, bool arrayContainsVertices,
                                        MmDisplayListVertexResourceView* view);

using MmDisplayListResolveResource = uintptr_t (*)(void* context, MmDisplayListReferenceKind kind, uint64_t hash,
                                                   size_t* resourceSize);

bool MmDisplayList_PatchCommands(MmDisplayListCommand* commands, size_t commandCount,
                                 MmDisplayListResolveResource resolveResource, void* context,
                                 MmDisplayListPatchStats* stats);

#endif
