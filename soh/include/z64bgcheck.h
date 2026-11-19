#ifndef Z_BGCHECK_H
#define Z_BGCHECK_H

struct PlayState;
struct Actor;
struct DynaPolyActor;

#define COLPOLY_NORMAL_FRAC (1.0f / SHT_MAX)
#define COLPOLY_SNORMAL(x) ((s16)((x) * SHT_MAX))
#define COLPOLY_GET_NORMAL(n) ((n)*COLPOLY_NORMAL_FRAC)
#define COLPOLY_VTX_INDEX_MASK 0x1FFFFFFFu
#define COLPOLY_VIA_FLAGS_SHIFT 29
#define COLPOLY_VIA_FLAGS_MASK 0xE0000000u
#define COLPOLY_VIB_CONVEYOR (1u << 29)
#define COLPOLY_VIA_FLAG_TEST(vIA, flags) ((vIA) & (((flags)&7) << COLPOLY_VIA_FLAGS_SHIFT))
#define COLPOLY_VTX_INDEX(vI) ((vI)&COLPOLY_VTX_INDEX_MASK)

#define DYNAPOLY_INVALIDATE_LOOKUP (1 << 0)

#define BGACTOR_NEG_ONE -1
#define BGCHECK_SCENE 0x7FFF
#define BGACTOR_INVALID BGCHECK_SCENE
#define BGACTOR_INITIAL_MAX 64
#define DYNA_RETIRED_BUFFERS_MAX 32
#define BGCHECK_Y_MIN -2147483648.0f
#define BGCHECK_XYZ_ABSMAX 1048576.0f
#define BGCHECK_SUBDIV_OVERLAP 50
#define BGCHECK_SUBDIV_MIN 150.0f

#define FUNC_80041EA4_RESPAWN 5
#define FUNC_80041EA4_MOUNT_WALL 6
#define FUNC_80041EA4_STOP 8
#define FUNC_80041EA4_VOID_OUT 12


typedef struct {
    Vec3f scale;
    Vec3s rot;
    Vec3f pos;
} ScaleRotPos;

typedef struct {
    u16 type;
    union {
        u32 vtxData[3];
        struct {
            u32 flags_vIA;
            u32 flags_vIB;
            u32 vIC;
        };
    };
    Vec3s normal; // Unit normal vector
                  // Value ranges from -0x7FFF to 0x7FFF, representing -1.0 to 1.0; 0x8000 is invalid

    s32 dist; // Plane distance from origin along the normal.
} CollisionPoly;

typedef struct {
    /* 0x00 */ u16 cameraSType;
    /* 0x02 */ s16 numCameras;
    /* 0x04 */ Vec3s* camPosData;
} CamData;

typedef struct {
    s32 xMin;
    s32 ySurface;
    s32 zMin;
    s32 xLength;
    s32 zLength;
    s32 camera;
    s32 lightSetting;
    s32 room;
    u8 notSwimmable;
} WaterBox;

typedef enum FloorType {
    /*  0 */ FLOOR_TYPE_0,
    /*  1 */ FLOOR_TYPE_1,
    /*  2 */ FLOOR_TYPE_2,
    /*  3 */ FLOOR_TYPE_3,
    /*  4 */ FLOOR_TYPE_4,
    /*  5 */ FLOOR_TYPE_5,
    /*  6 */ FLOOR_TYPE_6,
    /*  7 */ FLOOR_TYPE_7,
    /*  8 */ FLOOR_TYPE_8,
    /*  9 */ FLOOR_TYPE_9,
    /* 10 */ FLOOR_TYPE_10,
    /* 11 */ FLOOR_TYPE_11,
    /* 12 */ FLOOR_TYPE_12
} FloorType;

typedef enum WallType {
    /*  0 */ WALL_TYPE_0,
    /*  1 */ WALL_TYPE_1,
    /*  2 */ WALL_TYPE_2,
    /*  3 */ WALL_TYPE_3,
    /*  4 */ WALL_TYPE_4,
    /*  5 */ WALL_TYPE_5,
    /*  6 */ WALL_TYPE_6,
    /*  7 */ WALL_TYPE_7,
    /*  8 */ WALL_TYPE_8,
    /*  9 */ WALL_TYPE_9,
    /* 10 */ WALL_TYPE_10,
    /* 11 */ WALL_TYPE_11,
    /* 12 */ WALL_TYPE_12,
    /* 32 */ WALL_TYPE_MAX = 32
} WallType;

#define WALL_FLAG_0 (1 << 0)
#define WALL_FLAG_LADDER (1 << 1)
#define WALL_FLAG_LADDER_TOP (1 << 2)
#define WALL_FLAG_CLIMBABLE (1 << 3)
#define WALL_FLAG_CRAWLSPACE_1 (1 << 4)
#define WALL_FLAG_CRAWLSPACE_2 (1 << 5)
#define WALL_FLAG_GRABBABLE (1 << 6) // Grabbable dynapoly such as blocks
#define WALL_FLAG_CRAWLSPACE (WALL_FLAG_CRAWLSPACE_1 | WALL_FLAG_CRAWLSPACE_2)

typedef enum FloorProperty {
    /*  0 */ FLOOR_PROPERTY_0,
    /*  5 */ FLOOR_PROPERTY_5 = 5,
    /*  6 */ FLOOR_PROPERTY_6,
    /*  7 */ FLOOR_PROPERTY_7,
    /*  8 */ FLOOR_PROPERTY_8,
    /*  9 */ FLOOR_PROPERTY_9,
    /* 11 */ FLOOR_PROPERTY_11 = 11,
    /* 12 */ FLOOR_PROPERTY_12
} FloorProperty;

typedef enum FloorEffect {
    /*  0 */ FLOOR_EFFECT_0,
    /*  1 */ FLOOR_EFFECT_1, // Slope/sliding
    /*  2 */ FLOOR_EFFECT_2 // Transition
} FloorEffect;

typedef struct {
    s32 camera;
    s32 exit;
    s32 lightSetting;
    u8 floorType;
    u8 wallFlags;
    u8 wallType;
    u8 floorProperty;
    u8 isSoft;
    u8 isHorseBlocked;
    u8 material;
    u8 floorEffect;
    u8 echo;
    u8 canHookshot;
    u8 conveyorSpeed;
    u8 conveyorDirection;
    u8 isWallDamage;
} SurfaceType;

#ifdef __cplusplus
extern "C" {
#endif
SurfaceType SurfaceType_Unpack(u32 data0, u32 data1);
void WaterBox_UnpackProperties(WaterBox* waterBox, u32 properties);
#ifdef __cplusplus
}
#endif

typedef struct {
    Vec3i minBounds; // minimum coordinates of poly bounding box
    Vec3i maxBounds; // maximum coordinates of poly bounding box
    u32 numVertices;
    Vec3i* vtxList;
    u32 numPolygons;
    CollisionPoly* polyList;
    SurfaceType* surfaceTypeList;
    CamData* cameraDataList;
    u16 numWaterBoxes;
    WaterBox* waterBoxes;
    size_t cameraDataListLen; // OTRTODO: Added to allow for bounds checking the cameraDataList.
} CollisionHeader; // original name: BGDataInfo

typedef struct {
    s32 polyId;
    u32 next; // next SSNode index
} SSNode;

typedef struct {
    u32 head; // first SSNode index
} SSList;

typedef struct {
    u32 max;          // original name: short_slist_node_size
    u32 count;        // original name: short_slist_node_last_index
    SSNode* tbl;      // original name: short_slist_node_tbl
    u8* polyCheckTbl; // points to an array of bytes, one per static poly. Zero initialized when starting a
                      // bg check, and set to 1 if that poly has already been tested.
} SSNodeList;

typedef struct {
    SSNode* tbl;
    s32 count;
    s32 max;
} DynaSSNodeList;

typedef struct {
    SSList floor;
    SSList wall;
    SSList ceiling;
} StaticLookup;

typedef struct {
    u32 polyStartIndex;
    SSList ceiling;
    SSList wall;
    SSList floor;
} DynaLookup;

typedef struct {
    struct Actor* actor;
    CollisionHeader* colHeader;
    DynaLookup dynaLookup;
    u32 vtxStartIndex;
    ScaleRotPos prevTransform;
    ScaleRotPos curTransform;
    Sphere16 boundingSphere;
    f32 minY;
    f32 maxY;
} BgActor;

typedef struct {
    u8 bitFlag;
    BgActor* bgActors;
    u16* bgActorFlags;
    s32 bgActorMax;
    // Actors keep CollisionPoly* into superseded buffers, so they stay allocated until BgCheck_Free.
    void* retiredBuffers[DYNA_RETIRED_BUFFERS_MAX];
    s32 retiredCount;
    CollisionPoly* polyList;
    Vec3i* vtxList;
    DynaSSNodeList polyNodes;
    s32 polyNodesMax;
    s32 polyListMax;
    s32 vtxListMax;
} DynaCollisionContext;

typedef struct CollisionContext {
    CollisionHeader* colHeader; // scene's static collision
    Vec3f minBounds;            // minimum coordinates of collision bounding box
    Vec3f maxBounds;            // maximum coordinates of collision bounding box
    Vec3i subdivAmount;         // x, y, z subdivisions of the scene's static collision
    Vec3f subdivLength;         // x, y, z subdivision worldspace lengths
    Vec3f subdivLengthInv;      // inverse of subdivision length
    StaticLookup* lookupTbl;    // 3d array of length subdivAmount
    SSNodeList polyNodes;
    DynaCollisionContext dyna;
} CollisionContext;

typedef struct {
    /* 0x00 */ struct PlayState* play;
    /* 0x04 */ struct CollisionContext* colCtx;
    /* 0x08 */ u16 xpFlags;
    /* 0x0C */ CollisionPoly** resultPoly;
    /* 0x10 */ f32 yIntersect;
    /* 0x14 */ Vec3f* pos;
    /* 0x18 */ s32* bgId;
    /* 0x1C */ struct Actor* actor;
    /* 0x20 */ u32 unk_20;
    /* 0x24 */ f32 chkDist;
    /* 0x28 */ DynaCollisionContext* dyna;
    /* 0x2C */ SSList* ssList;
} DynaRaycast;

typedef struct {
    /* 0x00 */ struct CollisionContext* colCtx;
    /* 0x04 */ u16 xpFlags;
    /* 0x08 */ DynaCollisionContext* dyna;
    /* 0x0C */ SSList* ssList;
    /* 0x10 */ Vec3f* posA;
    /* 0x14 */ Vec3f* posB;
    /* 0x18 */ Vec3f* posResult;
    /* 0x1C */ CollisionPoly** resultPoly;
    /* 0x20 */ s32 chkOneFace; // bccFlags & 0x8
    /* 0x24 */ f32* distSq;    // distance from posA to poly squared
    /* 0x28 */ f32 chkDist;    // distance from poly
} DynaLineTest;

#endif
