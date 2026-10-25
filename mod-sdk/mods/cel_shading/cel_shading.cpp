// Cel-lighting policy adapted from roborich's wind-waker-style-cel-shading branch.
// Renderer transport lives in Fast3D; this standalone mod owns selection, scoping and settings.
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "soh/frame_interpolation.h"
#include "soh/ModApi/ModApi.h"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "regs.h"
// The mod imports the C engine declarations above, without global.h's host-only C++ console.
#define GLOBAL_H
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
int32_t CVarGetInteger(const char* name, int32_t defaultValue);
float CVarGetFloat(const char* name, float defaultValue);
}

#define CVAR(name) "gMods.CelShading." name
static const SOHModApi* sApi;
static const char* const sHooks[] = { "OnPlayDrawBegin", "OnActorDraw", "OnActorDrawEnd", "OnActorDestroy", "OnSceneInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, ARRAY_COUNT(sHooks) };
struct KeyState { float dir[3], col[3], velocity[3] = {}; u32 frame; };
static std::unordered_map<Actor*, KeyState> sKeys;
static bool sEnabled, sPoints, sItems, sDebug;
static LightInfo* sNaviGlow;
static LightInfo* sNaviNoGlow;
static float sRange, sTransition;
static u32 sFrame;

static f32 ToonSmoothDamp(f32 current, f32 target, f32* vel, f32 smoothTime, f32 dt) {
    if (smoothTime < 0.0001f) {
        smoothTime = 0.0001f;
    }
    f32 omega = 2.0f / smoothTime;
    f32 x = omega * dt;
    f32 expTerm = 1.0f / (1.0f + x + (0.48f * x * x) + (0.235f * x * x * x));
    f32 change = current - target;
    f32 temp = (*vel + (omega * change)) * dt;
    *vel = (*vel - (omega * temp)) * expTerm;
    return target + ((change + temp) * expTerm);
}

// Antipode-safe spherical interpolation of a unit direction by fraction t. Rotating along the sphere
// (never through the centre) keeps the key from snapping when the dominant light swings to the far
// side — e.g. when a fairy's light switches off. The near-aligned fast path uses no trig, so the
// common per-frame case is cheap.
static void ToonSlerp(f32 from[3], f32 to[3], f32 t, f32 out[3]) {
    f32 dot = (from[0] * to[0]) + (from[1] * to[1]) + (from[2] * to[2]);
    f32 len;

    dot = (dot < -1.0f) ? -1.0f : ((dot > 1.0f) ? 1.0f : dot);

    if (dot > 0.9995f) {
        // Almost aligned: cheap linear step + renormalize (no trig).
        out[0] = from[0] + ((to[0] - from[0]) * t);
        out[1] = from[1] + ((to[1] - from[1]) * t);
        out[2] = from[2] + ((to[2] - from[2]) * t);
        len = sqrtf((out[0] * out[0]) + (out[1] * out[1]) + (out[2] * out[2]));
        if (len > 0.0001f) {
            out[0] /= len, out[1] /= len, out[2] /= len;
        }
        return;
    }

    if (dot < -0.9995f) {
        // Almost opposite: the great-circle path is ambiguous, so rotate around an arbitrary
        // perpendicular axis to reach the far side smoothly.
        f32 ref[3], perp[3], d, ang, c, s;

        if (fabsf(from[0]) < 0.9f) {
            ref[0] = 1.0f, ref[1] = 0.0f, ref[2] = 0.0f;
        } else {
            ref[0] = 0.0f, ref[1] = 1.0f, ref[2] = 0.0f;
        }
        d = (ref[0] * from[0]) + (ref[1] * from[1]) + (ref[2] * from[2]);
        perp[0] = ref[0] - (from[0] * d), perp[1] = ref[1] - (from[1] * d), perp[2] = ref[2] - (from[2] * d);
        len = sqrtf((perp[0] * perp[0]) + (perp[1] * perp[1]) + (perp[2] * perp[2]));
        if (len > 0.0001f) {
            perp[0] /= len, perp[1] /= len, perp[2] /= len;
        }
        ang = t * 3.14159265f;
        c = cosf(ang), s = sinf(ang);
        out[0] = (from[0] * c) + (perp[0] * s);
        out[1] = (from[1] * c) + (perp[1] * s);
        out[2] = (from[2] * c) + (perp[2] * s);
        return;
    }

    f32 theta = acosf(dot);
    f32 sinTheta = sinf(theta);
    f32 s0 = sinf((1.0f - t) * theta) / sinTheta;
    f32 s1 = sinf(t * theta) / sinTheta;
    out[0] = (s0 * from[0]) + (s1 * to[0]);
    out[1] = (s0 * from[1]) + (s1 * to[1]);
    out[2] = (s0 * from[2]) + (s1 * to[2]);
}



static void EnvironmentKey(PlayState* play, float dir[3], float col[3]) {
    auto* a = &play->envCtx.dirLight1.params.dir;
    auto* b = &play->envCtx.dirLight2.params.dir;
    auto* light = (b->color[0] + b->color[1] + b->color[2] > a->color[0] + a->color[1] + a->color[2]) ? b : a;
    float length = std::sqrt(float(light->x * light->x + light->y * light->y + light->z * light->z));
    if (length > .001f) {
        dir[0] = light->x / length; dir[1] = light->y / length; dir[2] = light->z / length;
        for (int i = 0; i < 3; ++i) col[i] = light->color[i] / 255.f;
    }
}

static bool PointKey(Actor* actor, PlayState* play, float dir[3], float col[3]) {
    float best = -1.f;
    if (!sPoints) return false;
    for (LightNode* node = play->lightCtx.listHead; node; node = node->next) {
        LightInfo* light = node->info;
        if (!light || light->type == LIGHT_DIRECTIONAL || light == sNaviGlow || light == sNaviNoGlow) continue;
        auto& p = light->params.point;
        float d[3] = { p.x - actor->world.pos.x, p.y - actor->world.pos.y, p.z - actor->world.pos.z };
        float distance = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];
        float radius = p.radius * sRange;
        if (radius <= 0 || distance < .0001f || distance >= radius * radius || (best >= 0 && distance >= best)) continue;
        best = distance;
        for (int i = 0; i < 3; ++i) { dir[i] = d[i] / std::sqrt(distance); col[i] = p.color[i] / 255.f; }
    }
    return best >= 0;
}

static bool Eligible(Actor* actor) {
    // OoT scene geometry, doors, weather and effects keep their own lighting.
    if (actor->category == ACTORCAT_BG || actor->category == ACTORCAT_DOOR || actor->category == ACTORCAT_SWITCH ||
        actor->category == ACTORCAT_EXPLOSIVE) return false;
    if (!sItems && (actor->category == ACTORCAT_ITEMACTION || actor->category == ACTORCAT_CHEST)) return false;
    return true;
}

static void BeginActor(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (!*drawVanilla) return;
    if (!sEnabled || !actor || !play || !Eligible(actor)) return;
    float dir[3] = {0,1,0}, col[3] = {1,1,1};
    if (!PointKey(actor, play, dir, col)) EnvironmentKey(play, dir, col);
    auto [it, fresh] = sKeys.try_emplace(actor);
    KeyState& key = it->second;
    if (fresh) {
        std::copy(dir,dir+3,key.dir); std::copy(col,col+3,key.col); key.frame = sFrame;
    } else if (key.frame != sFrame) {
        float next[3];
        float dt = (R_UPDATE_RATE > 0 ? R_UPDATE_RATE : 3) / 60.f;
        float alpha = sTransition > .0001f ? 1.f - std::exp(-4.6f * dt / sTransition) : 1.f;
        ToonSlerp(key.dir,dir,alpha,next); std::copy(next,next+3,key.dir);
        for (int i=0;i<3;++i) key.col[i]=ToonSmoothDamp(key.col[i],col[i],&key.velocity[i],sTransition,dt);
        key.frame = sFrame;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPToon(POLY_OPA_DISP++, true); gSPToon(POLY_XLU_DISP++, true);
    gSPToonKey(POLY_OPA_DISP++, (s8)(key.dir[0]*127), (s8)(key.dir[1]*127), (s8)(key.dir[2]*127),
              (u8)(std::clamp(key.col[0],0.f,1.f)*255), (u8)(std::clamp(key.col[1],0.f,1.f)*255), (u8)(std::clamp(key.col[2],0.f,1.f)*255));
    gSPToonKey(POLY_XLU_DISP++, (s8)(key.dir[0]*127), (s8)(key.dir[1]*127), (s8)(key.dir[2]*127),
              (u8)(std::clamp(key.col[0],0.f,1.f)*255), (u8)(std::clamp(key.col[1],0.f,1.f)*255), (u8)(std::clamp(key.col[2],0.f,1.f)*255));
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EndActor(Actor*, PlayState* play) {
    if (!sEnabled || !play) return;
    OPEN_DISPS(play->state.gfxCtx);
    gSPToon(POLY_OPA_DISP++, false); gSPToon(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}
static void ForgetActor(void* actor) { sKeys.erase(static_cast<Actor*>(actor)); }
static void ForgetScene(s16) { sKeys.clear(); }
static void BeginFrame() {
    sNaviGlow = sNaviNoGlow = nullptr;
    if (gPlayState && !CVarGetInteger(CVAR("UseNaviLight"),1)) {
        Player* player = GET_PLAYER(gPlayState);
        if (player && player->naviActor && player->naviActor->id == ACTOR_EN_ELF && player->naviActor->params == FAIRY_NAVI) {
            auto* navi = reinterpret_cast<EnElf*>(player->naviActor);
            sNaviGlow = &navi->lightInfoGlow; sNaviNoGlow = &navi->lightInfoNoGlow;
        }
    }
    sEnabled = CVarGetInteger(CVAR("Enabled"),1) != 0;
    sPoints = CVarGetInteger(CVAR("PointLights"),1) != 0;
    sItems = CVarGetInteger(CVAR("Items"),1) != 0;
    sDebug = CVarGetInteger(CVAR("DebugBands"),0) != 0;
    sRange = std::clamp(CVarGetFloat(CVAR("PointLightRange"),1.5f),0.f,5.f);
    sTransition = std::clamp(CVarGetFloat(CVAR("TransitionTime"),1.f),0.f,5.f);
    sFrame = gPlayState ? gPlayState->state.frames : 0;
    if (!sEnabled) { sKeys.clear(); return; }
    sApi->SetCelShadingRamp(CVarGetFloat(CVAR("RampCenter"),.5f), CVarGetFloat(CVAR("RampSoftness"),.02f),
                          CVarGetFloat(CVAR("HighlightIntensity"),.6f),CVarGetFloat(CVAR("ShadowIntensity"),.6f),sDebug);
}

static void Checkbox(const char* label,const char* cvar,int value) {
    SOHModMenuWidget w = {}; w.structSize=sizeof(w); w.section="Mods";w.sidebar="Cel Shading";
    w.type=SOH_MOD_MENU_CHECKBOX;w.label=label;w.cvar=cvar;w.defaultInt=value;sApi->RegisterMenuWidget(&w);
}
static void Slider(const char* label,const char* cvar,float value,float min,float max) {
    SOHModMenuWidget w = {};w.structSize=sizeof(w);w.section="Mods";w.sidebar="Cel Shading";w.column=1;
    w.type=SOH_MOD_MENU_FLOAT_SLIDER;w.label=label;w.cvar=cvar;w.defaultFloat=value;w.minFloat=min;w.maxFloat=max;
    sApi->RegisterMenuWidget(&w);
}
extern "C" SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) { sApi=api; }
extern "C" SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements() { return &sRequirements; }
extern "C" SOH_MOD_EXPORT void ModInit() {
    if (!SOH_MOD_API_HAS(sApi,SetCelShadingRamp) || !sApi->SetCelShadingRamp) return;
    sApi->RegisterMenuSidebar("Mods","Cel Shading","gMods.Menu.Page",2);
    Checkbox("Enable Cel Shading",CVAR("Enabled"),1);
    Checkbox("Use Point Lights",CVAR("PointLights"),1);
    Checkbox("Use Navi's Light",CVAR("UseNaviLight"),1);
    Checkbox("Shade Items and Chests",CVAR("Items"),1);
    Slider("Light / Shadow Boundary",CVAR("RampCenter"),.5f,0,1);
    Slider("Band Softness",CVAR("RampSoftness"),.02f,.001f,.5f);
    Slider("Highlight Intensity",CVAR("HighlightIntensity"),.6f,0,2);
    Slider("Shadow Intensity",CVAR("ShadowIntensity"),.6f,0,1);
    Slider("Point Light Range",CVAR("PointLightRange"),1.5f,0,5);
    Slider("Light Transition Time",CVAR("TransitionTime"),1.f,0,5);
    Checkbox("Show Light / Shadow Bands",CVAR("DebugBands"),0);
    SOH_REGISTER_HOOK(sApi,OnPlayDrawBegin,BeginFrame);
    SOH_REGISTER_HOOK(sApi,OnActorDraw,BeginActor);
    SOH_REGISTER_HOOK(sApi,OnActorDrawEnd,EndActor);
    SOH_REGISTER_HOOK(sApi,OnActorDestroy,ForgetActor);
    SOH_REGISTER_HOOK(sApi,OnSceneInit,ForgetScene);
}
