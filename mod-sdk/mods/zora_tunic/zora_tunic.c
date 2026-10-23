#include <stdbool.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "z64items.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
#ifdef _MSC_VER
#pragma warning(error: 4013)
#endif

static const SOHModApi* sApi;
static const char* const sHooks[]={"OnPlayerUpdate"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,ARRAY_COUNT(sHooks)};
static bool TunicSwimEnabled(void) {
    const char* custom=sApi->GetWornEquipment(SOH_EQUIP_SLOT_TUNIC);
    return CVarGetInteger("gMods.ZoraTunic.Swim",1) &&
        CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)==EQUIP_VALUE_TUNIC_ZORA &&
        (!custom || !custom[0]);
}
static void Tick(void) {
    if (!gPlayState) return;
    typedef void (*SwimFunc)(PlayState*,Player*,bool);
    SwimFunc swim=(SwimFunc)sApi->FindService("nei.zora.human_gyorg_swim");
    if (swim) swim(gPlayState,GET_PLAYER(gPlayState),false);
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi,PublishService) || !SOH_MOD_API_HAS(sApi,FindService)) return;
    sApi->PublishService("nei.zora_tunic.swim_enabled",(void*)TunicSwimEnabled);
    if (SOH_MOD_API_HAS(sApi,RegisterMenuSidebar) && SOH_MOD_API_HAS(sApi,RegisterMenuWidget)) {
        sApi->RegisterMenuSidebar("Enhancements","Zora Tunic","gMods.Menu",1);
        SOHModMenuWidget w={0};w.structSize=sizeof(w);w.section="Enhancements";w.sidebar="Zora Tunic";
        w.type=SOH_MOD_MENU_CHECKBOX;w.label="Zora swim";w.cvar="gMods.ZoraTunic.Swim";w.defaultInt=1;
        w.tooltip="Wear the vanilla Zora Tunic and press A in deep water for Zora fast swim and dolphin jumps. Keeps Link's human model. Iron Boots and other forms take priority; shuffled swim is still required. Requires the Zora mod.";
        sApi->RegisterMenuWidget(&w);
    }
    SOH_REGISTER_HOOK(sApi,OnPlayerUpdate,Tick);
}
