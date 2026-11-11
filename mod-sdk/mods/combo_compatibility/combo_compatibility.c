#include "z64items.h"
#include "z64itemdraw.h"
#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"

typedef struct {
    const char* key;
    const char* label;
    const char* icon;
    const char* name;
    const char* pause;
    const char* text;
} ComboItem;

static const ALIGN_ASSET(2) char sIcon0[] = "__OTR__textures/nei_combo_oot/land_deed";
static const ALIGN_ASSET(2) char sName0[] = "__OTR__textures/nei_combo_oot/land_deed_name";
static const ALIGN_ASSET(2) char sIcon1[] = "__OTR__textures/nei_combo_oot/swamp_deed";
static const ALIGN_ASSET(2) char sName1[] = "__OTR__textures/nei_combo_oot/swamp_deed_name";
static const ALIGN_ASSET(2) char sIcon2[] = "__OTR__textures/nei_combo_oot/mountain_deed";
static const ALIGN_ASSET(2) char sName2[] = "__OTR__textures/nei_combo_oot/mountain_deed_name";
static const ALIGN_ASSET(2) char sIcon3[] = "__OTR__textures/nei_combo_oot/ocean_deed";
static const ALIGN_ASSET(2) char sName3[] = "__OTR__textures/nei_combo_oot/ocean_deed_name";
static const ALIGN_ASSET(2) char sIcon4[] = "__OTR__textures/nei_combo_oot/blue_frog";
static const ALIGN_ASSET(2) char sName4[] = "__OTR__textures/nei_combo_oot/blue_frog_name";
static const ALIGN_ASSET(2) char sIcon5[] = "__OTR__textures/nei_combo_oot/cyan_frog";
static const ALIGN_ASSET(2) char sName5[] = "__OTR__textures/nei_combo_oot/cyan_frog_name";
static const ALIGN_ASSET(2) char sIcon6[] = "__OTR__textures/nei_combo_oot/pink_frog";
static const ALIGN_ASSET(2) char sName6[] = "__OTR__textures/nei_combo_oot/pink_frog_name";
static const ALIGN_ASSET(2) char sIcon7[] = "__OTR__textures/nei_combo_oot/white_frog";
static const ALIGN_ASSET(2) char sName7[] = "__OTR__textures/nei_combo_oot/white_frog_name";
static const ALIGN_ASSET(2) char sIcon8[] = "__OTR__textures/nei_combo_oot/moons_tear";
static const ALIGN_ASSET(2) char sName8[] = "__OTR__textures/nei_combo_oot/moons_tear_name";
static const ALIGN_ASSET(2) char sIcon9[] = "__OTR__textures/nei_combo_oot/room_key";
static const ALIGN_ASSET(2) char sName9[] = "__OTR__textures/nei_combo_oot/room_key_name";
static const ALIGN_ASSET(2) char sIcon10[] = "__OTR__textures/nei_combo_oot/letter_to_kafei";
static const ALIGN_ASSET(2) char sName10[] = "__OTR__textures/nei_combo_oot/letter_to_kafei_name";
static const ALIGN_ASSET(2) char sIcon11[] = "__OTR__textures/nei_combo_oot/express_mail";
static const ALIGN_ASSET(2) char sName11[] = "__OTR__textures/nei_combo_oot/express_mail_name";
static const ComboItem sItems[] = {
    { "nei.combo.land_deed", "Land Title Deed", sIcon0, sName0, "%rLand Title Deed&%wBelongs to Majora's Mask.", "You got %rLand Title Deed%w!&Belongs to Majora's Mask. Trade this Clock Town deed to the Southern Swamp Business Scrub." },
    { "nei.combo.swamp_deed", "Swamp Title Deed", sIcon1, sName1, "%rSwamp Title Deed&%wBelongs to Majora's Mask.", "You got %rSwamp Title Deed%w!&Belongs to Majora's Mask. Trade this swamp deed to the Goron Village Business Scrub." },
    { "nei.combo.mountain_deed", "Mountain Title Deed", sIcon2, sName2, "%rMountain Title Deed&%wBelongs to Majora's Mask.", "You got %rMountain Title Deed%w!&Belongs to Majora's Mask. Trade this mountain deed to the Zora Hall Business Scrub." },
    { "nei.combo.ocean_deed", "Ocean Title Deed", sIcon3, sName3, "%rOcean Title Deed&%wBelongs to Majora's Mask.", "You got %rOcean Title Deed%w!&Belongs to Majora's Mask. Trade this ocean deed to the Ikana Canyon Business Scrub." },
    { "nei.combo.blue_frog", "Blue Frog", sIcon4, sName4, "%rBlue Frog&%wBelongs to Majora's Mask.", "You got %rBlue Frog%w!&Belongs to Majora's Mask. Reunite this blue singer with the Mountain Village frog choir." },
    { "nei.combo.cyan_frog", "Cyan Frog", sIcon5, sName5, "%rCyan Frog&%wBelongs to Majora's Mask.", "You got %rCyan Frog%w!&Belongs to Majora's Mask. Reunite this cyan singer with the Mountain Village frog choir." },
    { "nei.combo.pink_frog", "Pink Frog", sIcon6, sName6, "%rPink Frog&%wBelongs to Majora's Mask.", "You got %rPink Frog%w!&Belongs to Majora's Mask. Reunite this pink singer with the Mountain Village frog choir." },
    { "nei.combo.white_frog", "White Frog", sIcon7, sName7, "%rWhite Frog&%wBelongs to Majora's Mask.", "You got %rWhite Frog%w!&Belongs to Majora's Mask. Reunite this white singer with the Mountain Village frog choir." },
    { "nei.combo.moons_tear", "Moon's Tear", sIcon8, sName8, "%rMoon's Tear&%wBelongs to Majora's Mask.", "You got %rMoon's Tear%w!&Belongs to Majora's Mask. Trade this fallen moon stone for the Clock Town land deed." },
    { "nei.combo.room_key", "Room Key", sIcon9, sName9, "%rRoom Key&%wBelongs to Majora's Mask.", "You got %rRoom Key%w!&Belongs to Majora's Mask. Open your room at the Stock Pot Inn, even after the inn closes." },
    { "nei.combo.letter_to_kafei", "Letter to Kafei", sIcon10, sName10, "%rLetter to Kafei&%wBelongs to Majora's Mask.", "You got %rLetter to Kafei%w!&Belongs to Majora's Mask. Mail Anju's letter to arrange a meeting with Kafei." },
    { "nei.combo.express_mail", "Express Mail to Mama", sIcon11, sName11, "%rExpress Mail to Mama&%wBelongs to Majora's Mask.", "You got %rExpress Mail to Mama%w!&Belongs to Majora's Mask. Deliver Kafei's urgent letter to his mother, Madame Aroma." },
};
static const SOHModApi* sApi;
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };

static void DrawReward(PlayState* play, GetItemEntry* entry) {
    // The host overwrites drawItemId with ITEM_CUSTOM; tableId carries the runtime id instead.
    const SOHCustomItemDefinition* shown = sApi->FindCustomItemByRuntimeId(entry->tableId);
    if (shown == NULL || shown->iconPath == NULL) {
        return;
    }
    Z64Items_DrawIconBillboard(play, shown->iconPath);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    for (u16 i = 0; i < ARRAY_COUNT(sItems); i++) {
        const ComboItem* item = &sItems[i];
        SOHCustomItemDefinition definition = Z64Items_Define(item->key, item->icon, item->name);
        SOHCustomItemRandomizer randomizer = { 0 };
        randomizer.structSize = sizeof(randomizer);
        randomizer.type = SOH_CUSTOM_ITEM_TYPE_ITEM;
        // The owning game's pool supplies these rewards; presentation must not add duplicate copies.
        randomizer.poolCount = 0;
        randomizer.logicKey = item->key;
        randomizer.hintKey = item->key;
        definition.displayName = item->label;
        definition.pauseText = item->pause;
        definition.getItemText = item->text;
        definition.getItemEntry.drawFunc = DrawReward;
        definition.preferredPage = SOH_ITEM_PAGE_QUEST_VANILLA + 5 + i / 12;
        definition.preferredSlot = i % 12;
        definition.randomizer = &randomizer;
        if (!Z64Items_Register(sApi, &definition)) {
            return;
        }
    }
}
