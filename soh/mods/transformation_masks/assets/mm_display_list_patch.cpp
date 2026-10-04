#include "mm_display_list_patch.h"

namespace {

constexpr uint8_t kDisplayListHash = 0x31;
constexpr uint8_t kVertexHash = 0x32;
constexpr uint8_t kTextureHash = 0x20;
constexpr uint8_t kVertex = 0x01;
constexpr uint8_t kDisplayList = 0xDE;
constexpr uint8_t kSetTextureImage = 0x25; // retained filepath alias, full texture metadata
constexpr uint8_t kEndDisplayList = 0xDF;
constexpr size_t kVertexSize = 16;

bool IsTwoWordCommand(uint8_t opcode) {
    return opcode == 0x20 || opcode == 0x24 || opcode == 0x25 || opcode == 0x27 || opcode == 0x31 || opcode == 0x32 ||
           opcode == 0x33 || opcode == 0x35 || opcode == 0x36 || opcode == 0x42;
}

uint64_t ReadHash(const MmDisplayListCommand& payload) {
    return (static_cast<uint64_t>(payload.w0) << 32) | static_cast<uint32_t>(payload.w1);
}

} // namespace

bool MmDisplayList_SelectVertexResource(void* fastVertexPointer, size_t fastVertexSize, void* arrayPointer,
                                        size_t arraySize, bool arrayContainsVertices,
                                        MmDisplayListVertexResourceView* view) {
    if (view == nullptr) {
        return false;
    }
    *view = {};
    if (fastVertexPointer != nullptr && fastVertexSize != 0) {
        view->pointer = reinterpret_cast<uintptr_t>(fastVertexPointer);
        view->size = fastVertexSize;
        return true;
    }
    if (arrayContainsVertices && arrayPointer != nullptr && arraySize != 0) {
        view->pointer = reinterpret_cast<uintptr_t>(arrayPointer);
        view->size = arraySize;
        return true;
    }
    return false;
}

bool MmDisplayList_PatchCommands(MmDisplayListCommand* commands, size_t commandCount,
                                 MmDisplayListResolveResource resolveResource, void* context,
                                 MmDisplayListPatchStats* stats) {
    MmDisplayListPatchStats localStats = {};
    MmDisplayListPatchStats& result = stats != nullptr ? *stats : localStats;

    result = {};
    if (commands == nullptr || resolveResource == nullptr || commandCount == 0) {
        ++result.malformed;
        return false;
    }

    for (size_t i = 0; i < commandCount;) {
        MmDisplayListCommand& command = commands[i];
        const uint8_t opcode = static_cast<uint8_t>(command.w0 >> 24);

        if (opcode == kEndDisplayList) {
            return true;
        }
        if (IsTwoWordCommand(opcode) && i + 1 >= commandCount) {
            ++result.malformed;
            return false;
        }
        if (opcode == 0x3D && (command.w1 >> 24) == 0x0C) {
            // Opaque MM actors use Scene_SetRenderModeXlu(play, 0, 1):
            // indices 0 and 2 both terminate without changing geometry state.
            const uint32_t index = command.w1 & UINT32_C(0x00FFFFFF);
            size_t resourceSize = 0;
            if (index != 0 && index != 2) {
                ++result.malformed;
                return false;
            }
            const uintptr_t target =
                resolveResource(context, MM_DISPLAY_LIST_REFERENCE_RENDER_MODE, index, &resourceSize);
            if (target == 0) {
                ++result.unresolved;
            } else {
                command.w0 = (static_cast<uint32_t>(kDisplayList) << 24) | (command.w0 & UINT32_C(0x00010000));
                command.w1 = target;
                ++result.renderModePatched;
            }
            ++i;
            continue;
        }
        if (opcode == kDisplayListHash) {
            MmDisplayListCommand& payload = commands[i + 1];
            size_t resourceSize = 0;
            const uintptr_t nested =
                resolveResource(context, MM_DISPLAY_LIST_REFERENCE_NESTED, ReadHash(payload), &resourceSize);
            if (nested == 0) {
                ++result.unresolved;
            } else {
                const uint32_t pushBranchBit = command.w0 & UINT32_C(0x00010000);
                command.w0 = (static_cast<uint32_t>(kDisplayList) << 24) | pushBranchBit;
                command.w1 = nested;
                payload = {};
                ++result.nestedPatched;
            }
            i += 2;
            continue;
        }
        if (opcode == kTextureHash) {
            MmDisplayListCommand& payload = commands[i + 1];
            size_t resourceSize = 0;
            const uintptr_t texture =
                resolveResource(context, MM_DISPLAY_LIST_REFERENCE_TEXTURE, ReadHash(payload), &resourceSize);
            if (texture == 0) {
                ++result.unresolved;
            } else {
                command.w0 = (command.w0 & UINT32_C(0x00FFFFFF)) | (static_cast<uint32_t>(kSetTextureImage) << 24);
                command.w1 = texture;
                payload = {};
                ++result.texturesPatched;
            }
            i += 2;
            continue;
        }
        if (opcode == kVertexHash) {
            MmDisplayListCommand& payload = commands[i + 1];
            size_t resourceSize = 0;
            const uintptr_t vertexBase =
                resolveResource(context, MM_DISPLAY_LIST_REFERENCE_VERTEX, ReadHash(payload), &resourceSize);
            const size_t vertexCount = (command.w0 >> 12) & UINT32_C(0xFF);
            const uintptr_t byteOffset = command.w1;
            if (vertexBase == 0) {
                ++result.unresolved;
            } else if (byteOffset > resourceSize || byteOffset % kVertexSize != 0 ||
                       vertexCount > (resourceSize - byteOffset) / kVertexSize ||
                       byteOffset > UINTPTR_MAX - vertexBase) {
                ++result.malformed;
                return false;
            } else {
                /* Match gfx_vtx_hash_handler_custom: w1 is a byte offset into
                 * the resolved resource, not a vertex index or unused word. */
                command.w0 = (command.w0 & UINT32_C(0x00FFFFFF)) | (static_cast<uint32_t>(kVertex) << 24);
                command.w1 = vertexBase + byteOffset;
                payload = {};
                ++result.verticesPatched;
            }
            i += 2;
            continue;
        }
        i += IsTwoWordCommand(opcode) ? 2 : 1;
    }

    ++result.malformed;
    return false;
}
