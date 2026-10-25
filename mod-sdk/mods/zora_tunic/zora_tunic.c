#include <stdbool.h>
#include "z64items.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
#ifdef _MSC_VER
#pragma warning(error: 4013)
#endif

static const SOHModApi* sApi;
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),NULL,0};
static bool TunicSwimEnabled(void) {
    const char* custom=sApi->GetWornEquipment(SOH_EQUIP_SLOT_TUNIC);
    return CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)==EQUIP_VALUE_TUNIC_ZORA &&
        (!custom || !custom[0]);
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api){sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void){return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi,PublishService) || !SOH_MOD_API_HAS(sApi,FindService)) return;
    sApi->PublishService("nei.zora_tunic.swim_enabled",(void*)TunicSwimEnabled);
}
