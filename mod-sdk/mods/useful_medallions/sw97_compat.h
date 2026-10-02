// Bridges the SW97 decomp's 1997 names onto Unbound's.
// Original actors: z64proto/sw97 team (Spaceworld '97 Experience).

#ifndef SW97_COMPAT_H
#define SW97_COMPAT_H

#include <math.h>

#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

typedef PlayState GlobalContext;

#define PLAYER GET_PLAYER(Effect_GetPlayState())

#define MATRIX_TO_MTX(dest, file, line) Matrix_ToMtx(dest, file, line)
#define GRAPH_ALLOC(gfxCtx, size) Graph_Alloc(gfxCtx, size)

#define func_800D20CC Matrix_MtxFToYXZRotS
#define func_80093D84 Gfx_SetupDL_25Opa
#define func_80093D18 Gfx_SetupDL_25Opa
#define func_8010BDBC Message_GetState
#define func_80106BC8 Message_ShouldAdvance
#define func_8010B720 Message_ContinueTextbox
#define func_800876C8 Magic_Reset
#define func_800937C0 Gfx_SetupDL_57
#define func_800773A8 Environment_AdjustLights
#define func_8002F7DC Player_PlaySfx
#define func_8004356C DynaPolyActor_IsPlayerOnTop
#define Effect_GetGlobalCtx Effect_GetPlayState
#define Matrix_RotateRPY Matrix_RotateZYX
#define Gfx_CallSetupDL Gfx_SetupDL
#define Audio_PlaySoundAtPosition SoundSource_PlaySfxAtFixedWorldPos

// Later decomp names the NEI tree already used; this fork still spells them the old way.
#define Actor_PlaySfx_Flagged func_8002F974
#define Camera_RequestSetting Camera_ChangeSetting
#define Rumble_Request func_800AA000
#define CAM_ID_MAIN MAIN_CAM
#define BGCHECKFLAG_GROUND 1

// The decomp's debug prints need a log level the SDK does not expose.
#undef LOG_STRING
#define LOG_STRING(string) ((void)0)
#undef osSyncPrintf
#define osSyncPrintf(...) ((void)0)

#define SW97_Matrix_RotateY_s(binang, mode) Matrix_RotateY(BINANG_TO_RAD(binang), mode)
#define SW97_Matrix_RotateX_s(binang, mode) Matrix_RotateX(BINANG_TO_RAD(binang), mode)
#define SW97_Matrix_RotateZ_s(binang, mode) Matrix_RotateZ(BINANG_TO_RAD(binang), mode)
#define SW97_Matrix_RotateY_f(degf, mode) Matrix_RotateY(DEG_TO_RAD(degf), mode)
#define SW97_Matrix_RotateX_f(degf, mode) Matrix_RotateX(DEG_TO_RAD(degf), mode)
#define SW97_Matrix_RotateZ_f(degf, mode) Matrix_RotateZ(DEG_TO_RAD(degf), mode)

#ifndef BINANG_TO_RADF
#define BINANG_TO_RADF(binang) ((f32)(binang) * (M_PI / 32768.0f))
#endif

MtxF* Matrix_GetCurrent(void);

// SW97 helper with no soh equivalent: projects along the current matrix's Z axis.
static inline void Matrix_MultZ(f32 scale, Vec3f* dst) {
    MtxF* current = Matrix_GetCurrent();

    dst->x = current->wx + current->zx * scale;
    dst->y = current->wy + current->zy * scale;
    dst->z = current->wz + current->zz * scale;
}

#endif // SW97_COMPAT_H
