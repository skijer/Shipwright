#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/overlays/actors/ovl_En_Viewer/z_en_viewer.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_mm_actor.h"
#include "tests/test_require.h"

static MtxF matrix;
static MtxF* sCurrentMatrix = &matrix;
void FrameInterpolation_RecordMatrixTranslate(f32 x, f32 y, f32 z, u8 mode) {
}
void FrameInterpolation_RecordMatrixScale(f32 x, f32 y, f32 z, u8 mode) {
}
/* Only sine is a boundary here; all matrix operations and the actor callback
 * below are extracted verbatim from production. Quadrants have exact values. */
f32 Math_SinS(s16 angle) {
    return sinf((float)angle * (3.14159265358979323846f / 32768.0f));
}
/* PRODUCTION_MATRIX_FUNCTIONS */
/* PRODUCTION_TATL_CALLBACK */

int main(void) {
    EnViewer actor = { 0 };
    Gfx list[1] = { 0 };
    Gfx* dList = list;
    Gfx* output = list;
    Vec3f pos = { 2, 3, 4 };
    Vec3s rot = { 5, 6, 7 };
    const float actorScales[] = { 0.005f, 0.01f, 0.02f };
    const u16 phases[] = { 0, 0x4000, 0xC000 };
    const float pulseFactors[] = { 1.0f, 1.188f, 0.828f };
    for (unsigned a = 0; a < 3; ++a) {
        for (unsigned p = 0; p < 3; ++p) {
            actor.actor.scale.x = actorScales[a];
            actor.staticState.tatlPulsePhase = phases[p];
            SkinMatrix_SetTranslate(&matrix, 13, -27, 91);
            matrix.xx = 0;
            matrix.xy = 7;
            matrix.yx = -3;
            matrix.yy = 0;
            matrix.zz = 9;
            MtxF before = matrix;
            REQUIRE(!EnViewer_StaticTatlOverrideLimbDraw(NULL, 7, &dList, &pos, &rot, &actor, &output));
            REQUIRE(memcmp(&before, &matrix, sizeof(matrix)) == 0);
            REQUIRE(!EnViewer_StaticTatlOverrideLimbDraw(NULL, 8, &dList, &pos, &rot, &actor, &output));
            REQUIRE(matrix.xw == 13 && matrix.yw == -27 && matrix.zw == 91);
            const float expected = actorScales[a] * 1.5f * pulseFactors[p];
            REQUIRE(fabsf(matrix.xx - expected) < 0.000001f);
            REQUIRE(matrix.xx == matrix.yy && matrix.yy == matrix.zz && matrix.ww == 1);
            REQUIRE(matrix.xy == 0 && matrix.xz == 0 && matrix.yx == 0 && matrix.yz == 0 && matrix.zx == 0 &&
                    matrix.zy == 0 && matrix.wx == 0 && matrix.wy == 0 && matrix.wz == 0);
            REQUIRE(dList == list && output == list && pos.x == 2 && pos.y == 3 && pos.z == 4);
            REQUIRE(rot.x == 5 && rot.y == 6 && rot.z == 7);
            REQUIRE(actor.actor.scale.x == actorScales[a]);
        }
    }
    puts("PASS: actual Tatl callback and production matrix math preserve origin and reset body scale only");
    return 0;
}
