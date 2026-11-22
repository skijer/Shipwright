#include "sm64_build.h"
#include "load_model_data.h"

#include <stdlib.h>
#include <string.h>

#include "gfx_macros.h"
#include "load_tex_data.h"
#include "decomp/mario/model_lists.h"
#include "decomp/tools/libmio0.h"
#include "decomp/tools/utils.h"

#define MARIO_MODEL_ROM_OFFSET 0x114750
#define MARIO_MODEL_SEGMENT 0x04
#define ROM_COMMAND_SIZE 8
#define ROM_VERTEX_SIZE 16
#define MAX_WORDS_PER_COMMAND 5
#define STUB_WORDS 3

enum RomOpcode {
    ROM_OP_MOVEMEM = 0x03,
    ROM_OP_VERTEX = 0x04,
    ROM_OP_DISPLAY_LIST = 0x06,
    ROM_OP_END_DISPLAY_LIST = 0xB8,
    ROM_OP_TEXTURE = 0xBB,
    ROM_OP_TRIANGLE = 0xBF,
    ROM_OP_SET_TILE_SIZE = 0xF2,
    ROM_OP_SET_TEXTURE_IMAGE = 0xFD,
};

#define ROM_MOVEMEM_FIRST_LIGHT 0x86
#define ROM_DISPLAY_LIST_BRANCH 1

struct ConvertedBlock {
    uint32_t offset;
    uint32_t count;
    void* data;
};

#define MARIO_MODEL_LIST(name, offset) Gfx name[STUB_WORDS];
MARIO_MODEL_LISTS
#undef MARIO_MODEL_LIST

static const struct {
    Gfx* stub;
    uint32_t offset;
} kModelLists[] = {
#define MARIO_MODEL_LIST(name, offset) { name, offset },
    MARIO_MODEL_LISTS
#undef MARIO_MODEL_LIST
};

static uint8_t* s_segment;
static uint32_t s_segmentSize;
static struct ConvertedBlock* s_blocks;
static uint32_t s_blockCount;
static uint32_t s_blockCapacity;

static void write_stub(Gfx* stub, Gfx* list) {
    if (list == NULL) {
        stub[0] = GFXCMD_EndDisplayList;
        return;
    }
    stub[0] = GFXCMD_SubDisplayList;
    stub[1] = (Gfx)list;
    stub[2] = GFXCMD_EndDisplayList;
}

static void* find_block(uint32_t offset, uint32_t count) {
    for (uint32_t i = 0; i < s_blockCount; i++) {
        if (s_blocks[i].offset == offset && s_blocks[i].count == count) {
            return s_blocks[i].data;
        }
    }
    return NULL;
}

static void* remember_block(uint32_t offset, uint32_t count, void* data) {
    if (s_blockCount == s_blockCapacity) {
        uint32_t capacity = (s_blockCapacity == 0) ? 256 : s_blockCapacity * 2;
        struct ConvertedBlock* blocks = realloc(s_blocks, capacity * sizeof(*blocks));

        if (blocks == NULL) {
            free(data);
            return NULL;
        }
        s_blocks = blocks;
        s_blockCapacity = capacity;
    }
    s_blocks[s_blockCount].offset = offset;
    s_blocks[s_blockCount].count = count;
    s_blocks[s_blockCount].data = data;
    s_blockCount++;
    return data;
}

// Every pointer the ROM's lists hold is a segment 04 address; anything else is not Mario's.
static int resolve_segment_address(uint32_t address, uint32_t size, uint32_t* outOffset) {
    uint32_t offset = address & 0x00FFFFFF;

    if ((address >> 24) != MARIO_MODEL_SEGMENT || offset + size > s_segmentSize) {
        return 0;
    }
    *outOffset = offset;
    return 1;
}

static Vtx* convert_vertices(uint32_t address, uint32_t count) {
    uint32_t offset;

    if (!resolve_segment_address(address, count * ROM_VERTEX_SIZE, &offset)) {
        return NULL;
    }

    Vtx* converted = find_block(offset, count);

    if (converted != NULL) {
        return converted;
    }
    converted = calloc(count, sizeof(Vtx));
    if (converted == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < count; i++) {
        uint8_t* raw = s_segment + offset + i * ROM_VERTEX_SIZE;

        converted[i].v.ob[0] = (float)read_s16_be(raw + 0);
        converted[i].v.ob[1] = (float)read_s16_be(raw + 2);
        converted[i].v.ob[2] = (float)read_s16_be(raw + 4);
        converted[i].v.flag = (unsigned short)read_u16_be(raw + 6);
        converted[i].v.tc[0] = (short)read_s16_be(raw + 8);
        converted[i].v.tc[1] = (short)read_s16_be(raw + 10);
        memcpy(converted[i].v.cn, raw + 12, 4);
    }
    return remember_block(offset, count, converted);
}

// libsm64 knows only the textures of its atlas; the metal one stands in for any other.
static intptr_t find_texture_index(uint32_t address) {
    uint32_t offset = address & 0x00FFFFFF;

    for (intptr_t i = 0; i < NUM_USED_TEXTURES; i++) {
        if ((uint32_t)mario_tex_offsets[i] == offset) {
            return i;
        }
    }
    return mario_texture_metal;
}

static uint32_t count_commands(uint32_t offset) {
    uint32_t count = 0;

    for (uint32_t at = offset; at + ROM_COMMAND_SIZE <= s_segmentSize; at += ROM_COMMAND_SIZE) {
        uint8_t opcode = s_segment[at];

        count++;
        if (opcode == ROM_OP_END_DISPLAY_LIST ||
            (opcode == ROM_OP_DISPLAY_LIST && s_segment[at + 1] == ROM_DISPLAY_LIST_BRANCH)) {
            break;
        }
    }
    return count;
}

static Gfx* convert_list(uint32_t address);

static Gfx* convert_command(Gfx* out, uint32_t word0, uint32_t word1) {
    switch (word0 >> 24) {
        case ROM_OP_MOVEMEM: {
            uint32_t offset;

            if (resolve_segment_address(word1, sizeof(Light), &offset)) {
                *out++ = GFXCMD_Light;
                *out++ = (Gfx)(s_segment + offset);
                *out++ = (((word0 >> 16) & 0xFF) - ROM_MOVEMEM_FIRST_LIGHT) / 2 + 1;
            }
            return out;
        }
        case ROM_OP_VERTEX: {
            uint32_t count = ((word0 >> 20) & 0xF) + 1;
            Vtx* vertices = convert_vertices(word1, count);

            if (vertices != NULL) {
                *out++ = GFXCMD_VertexData;
                *out++ = (Gfx)vertices;
                *out++ = count;
                *out++ = (word0 >> 16) & 0xF;
            }
            return out;
        }
        case ROM_OP_TRIANGLE:
            *out++ = GFXCMD_Triangle;
            *out++ = ((word1 >> 16) & 0xFF) / 10;
            *out++ = ((word1 >> 8) & 0xFF) / 10;
            *out++ = (word1 & 0xFF) / 10;
            *out++ = word1 >> 24;
            return out;
        case ROM_OP_TEXTURE:
            *out++ = GFXCMD_Texture;
            *out++ = word1 >> 16;
            *out++ = word1 & 0xFFFF;
            *out++ = word0 & 0xFF;
            return out;
        case ROM_OP_SET_TEXTURE_IMAGE:
            *out++ = GFXCMD_SetTextureImage;
            *out++ = find_texture_index(word1);
            return out;
        case ROM_OP_SET_TILE_SIZE:
            *out++ = GFXCMD_SetTileSize;
            *out++ = (word0 >> 12) & 0xFFF;
            *out++ = word0 & 0xFFF;
            *out++ = (word1 >> 12) & 0xFFF;
            *out++ = word1 & 0xFFF;
            return out;
        case ROM_OP_DISPLAY_LIST: {
            Gfx* list = convert_list(word1);

            if (list != NULL) {
                *out++ = GFXCMD_SubDisplayList;
                *out++ = (Gfx)list;
            }
            if (((word0 >> 16) & 0xFF) == ROM_DISPLAY_LIST_BRANCH) {
                *out++ = GFXCMD_EndDisplayList;
            }
            return out;
        }
        case ROM_OP_END_DISPLAY_LIST:
            *out++ = GFXCMD_EndDisplayList;
            return out;
        default:
            return out;
    }
}

// The list is remembered before its commands are read, so a list that calls itself ends instead of recursing.
static Gfx* convert_list(uint32_t address) {
    uint32_t offset;

    if (!resolve_segment_address(address, ROM_COMMAND_SIZE, &offset)) {
        return NULL;
    }

    Gfx* converted = find_block(offset, 0);

    if (converted != NULL) {
        return converted;
    }

    uint32_t commandCount = count_commands(offset);

    converted = remember_block(offset, 0, calloc(commandCount * MAX_WORDS_PER_COMMAND + 1, sizeof(Gfx)));
    if (converted == NULL) {
        return NULL;
    }

    Gfx* out = converted;

    for (uint32_t i = 0; i < commandCount; i++) {
        uint8_t* raw = s_segment + offset + i * ROM_COMMAND_SIZE;

        out = convert_command(out, read_u32_be(raw), read_u32_be(raw + 4));
    }
    *out = GFXCMD_EndDisplayList;
    return converted;
}

void load_mario_model_from_rom(const uint8_t* rom) {
    mio0_header_t header;

    unload_mario_model();
    if (!mio0_decode_header(rom + MARIO_MODEL_ROM_OFFSET, &header)) {
        return;
    }
    s_segment = malloc(header.dest_size);
    if (s_segment == NULL) {
        return;
    }
    s_segmentSize = header.dest_size;
    mio0_decode(rom + MARIO_MODEL_ROM_OFFSET, s_segment, NULL);
    for (uint32_t i = 0; i < sizeof(kModelLists) / sizeof(kModelLists[0]); i++) {
        write_stub(kModelLists[i].stub, convert_list((MARIO_MODEL_SEGMENT << 24) | kModelLists[i].offset));
    }
}

void unload_mario_model(void) {
    for (uint32_t i = 0; i < sizeof(kModelLists) / sizeof(kModelLists[0]); i++) {
        write_stub(kModelLists[i].stub, NULL);
    }
    for (uint32_t i = 0; i < s_blockCount; i++) {
        free(s_blocks[i].data);
    }
    free(s_blocks);
    free(s_segment);
    s_blocks = NULL;
    s_blockCount = 0;
    s_blockCapacity = 0;
    s_segment = NULL;
    s_segmentSize = 0;
}
