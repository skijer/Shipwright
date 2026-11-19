#pragma once

#include <stdint.h>
#include <vector>
#include <ship/resource/Resource.h>
#include "z64math.h"

namespace SOH {

// Must mirror CollisionPoly in soh/include/z64bgcheck.h
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

// Must mirror WaterBox in soh/include/z64bgcheck.h
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

typedef struct {
    /* 0x00 */ u16 cameraSType;
    /* 0x02 */ s16 numCameras;
    /* 0x04 */ Vec3s* camPosData;
} CamData;

// Must mirror SurfaceType in soh/include/z64bgcheck.h
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

SurfaceType UnpackSurfaceType(uint32_t data0, uint32_t data1);
void UnpackWaterBoxProperties(WaterBox& waterBox, uint32_t properties);

// Must mirror CollisionHeader in soh/include/z64bgcheck.h
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
} CollisionHeaderData;        // original name: BGDataInfo

class CollisionHeader : public Ship::Resource<CollisionHeaderData> {
  public:
    using Resource::Resource;

    CollisionHeader() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    CollisionHeaderData* GetPointer();
    size_t GetPointerSize();

    CollisionHeaderData collisionHeaderData;

    std::vector<Vec3i> vertices;

    std::vector<CollisionPoly> polygons;

    uint32_t surfaceTypesCount;
    std::vector<SurfaceType> surfaceTypes;

    uint32_t camDataCount;
    std::vector<CamData> camData;
    std::vector<int32_t> camPosDataIndices;

    int32_t camPosCount;
    Vec3s camPosDataZero;
    std::vector<Vec3s> camPosData;

    std::vector<WaterBox> waterBoxes;
};
}; // namespace SOH
