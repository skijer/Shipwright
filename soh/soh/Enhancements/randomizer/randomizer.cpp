#include "randomizer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <variables.h>
#include <macros.h>
#include <functions.h>
#include "3drando/menu.hpp"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SohGui/SohGui.hpp"
#include <imgui.h>
#include "../../../src/overlays/actors/ovl_En_GirlA/z_en_girla.h"
#include "randomizer_check_objects.h"
#include <sstream>
#include <tuple>
#include "soh/OTRGlobals.h"
#include <ship/window/FileDropMgr.h>
#include "static_data.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "settings.h"
#include "soh/util.h"
#include "randomizerTypes.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/Enhancements/randomizer/RCToRandInf.h"
#include "dungeon.h"

// Extended Inventory for Custom Items (Page 2)
extern "C" {
#include "mods/extended_inventory.h"
#include "mods/extended_equipment.h"
#include "mods/items/logic/weapon_upgrades.h"
#include "mods/items/custom_items.h"
#include "mods/items/custom_bottles.h" // Bottle_GiveBottle: rando bottles go to the 8-slot wheel
#include "src/overlays/actors/ovl_Obj_Bean/z_obj_bean.h"
#include "mods/nei_save.h"                          // Nei_Save() + FC_COMBO_OBTAINED_FC_SIZE (fcId store)
#include "soh/FleetShipCombo/FleetComboItemsGlue.h" // FcCombo_ItemForNative (native RG -> FcComboItemId)
#include "soh/FleetShipCombo/FleetComboItems.h"     // FCI_NO_ITEM sentinel
#include "soh/FleetShipCombo/FleetComboIds.h"       // FC_MM_SKULLS_* registry counters (MM GS tokens)

extern void func_80B8FE00(ObjBean*); // trigger planting
// MM trade/quest grant APIs (Skijer's NEI) — same calls the debug/give-all menu uses (SohMenuNEI.cpp):
// trade_items.c (adult-trade wheel bitmask), picto_box.c (pictoboxOwned), power_keg.c (kegOwned+count).
extern void TradeAdult_GiveItem(unsigned char item); // sets Nei_Save()->tradeAdultOwned bit
extern void Picto_SetOwned(unsigned char on);        // sets Nei_Save()->pictoboxOwned
extern void PowerKeg_SetOwned(unsigned char on);     // sets Nei_Save()->powerKegOwned
extern unsigned char PowerKeg_GetCount(void);        // Nei_Save()->powerKegCount
extern void PowerKeg_SetCount(unsigned char n);      // clamps to PowerKeg_MaxCount()
// Bottle Randomizer ownership (custom_bottles.cpp): once set, mm_bottle_items.cpp projects the
// item into SLOT_BOTTLE_3/4 (+ any C-button) every frame — the give only needs the flag.
extern void Bottle_SetNetOwned(unsigned char owned);        // Nei_Save()->netEquipped
extern void Bottle_SetBottomlessOwned(unsigned char owned); // Nei_Save()->bottomlessBottleMode
// FleetSync: while ApplyFcRegistryToNatives() is granting the FC deficit it calls Randomizer_Item_Give,
// which would re-enter the record hook below and double-count. This flag lets the hook skip recording
// during that apply pass (see FleetSync.cpp ApplyFcRegistryToNatives).
int FleetSync_IsApplyingFc(void);
extern "C" void FleetShared_OnNativeObtained(int nativeId); // FleetShipCombo/FleetSharedItems.h
extern PlayState* gPlayState;
}

static ObjectExtension::Register<CheckIdentity> RegisterIdentity;

extern std::map<RandomizerCheckArea, std::string> rcAreaNames;

using json = nlohmann::json;
using namespace std::literals::string_literals;

std::unordered_map<std::string, RandomizerCheckArea> SpoilerfileAreaNameToEnum;
std::unordered_map<std::string, HintType> SpoilerfileHintTypeNameToEnum;
std::set<RandomizerCheck> excludedLocations;
std::set<RandomizerCheck> spoilerExcludedLocations;

bool generated;

// ============================================================================
// CUSTOM ITEMS RANDOMIZER MESSAGES
// ============================================================================
// Helper structure for custom item messages (defined inline to avoid linker issues)
// Dual Cane skill state (mods/items/logic/item_cane_of_somaria.c) — used by the per-skill
// obtainability and give arms below.
extern "C" u8 Cane_GiveSkill(u8 skill);
extern "C" u8 Cane_HasSkill(u8 skill);
// Clawshot ownership (mods/items/logic/twilight_upgrade.c) — the Clawshot shares the hookshot cell.
extern "C" u8 TwilightUpgrade_HasClawshot(void);
extern "C" void TwilightUpgrade_SetClawshot(u8 on);

struct CustomItemMessageEntry {
    s16 rgId;
    ItemID itemId;
    const char* english;
    const char* german;
    const char* french;
};

// Array of all 26 custom item messages
/* Custom Item Messages
 * Descriptions, Lore, and Translations provided by Gemini 3.0
 */
static const CustomItemMessageEntry customItemMessages[] = {
    // Movement Items
    // Skijer's progressive Roc's Feather (extended inventory page 2)
    { RG_PROGRESSIVE_ROCS, static_cast<ItemID>(ITEM_ROCS_FEATHER_SKIJER),
      "You got %rRoc's Feather%w!&This magical feather lets you&jump higher than normal.^Assign it to %y\xA1%w and "
      "press&to perform a high jump.&It even works in water!",
      "Du hast %rRocs Feder%w erhalten!&Diese magische Feder lässt&dich höher springen.^Weise sie %y\xA1%w zu und "
      "drücke&um hoch zu springen.&Funktioniert auch im Wasser!",
      "Vous obtenez la %rPlume de Roc%w!&Cette plume magique vous&permet de sauter plus haut.^Assignez-la à %y\xA1%w "
      "et "
      "appuyez&pour faire un grand saut.&Fonctionne même dans l'eau!" },

    // Vanilla rando Roc's Feather (shares the Nayru's Love slot, RSK_ROCS_FEATHER)
    { RG_ROCS_FEATHER, static_cast<ItemID>(ITEM_ROCS_FEATHER_SKIJER),
      "You got %rRoc's Feather%w!&Assign it to %y\xA1%w and press it&while standing to leap into&the air. It shares "
      "its slot&with Nayru's Love.",
      "Du hast %rRocs Feder%w erhalten!&Weise sie %y\xA1%w zu und drücke,&um in die Luft zu springen.&Sie teilt sich "
      "den Platz mit&Nayrus Umarmung.",
      "Vous obtenez la %rPlume de Roc%w!&Assignez-la à %y\xA1%w et appuyez&pour bondir dans les airs.&Elle partage son "
      "emplacement&avec l'Amour de Nayru." },

    // Skijer's NEI: page-2 custom items + the 24 MM masks + Bottle with Magic Mushroom moved their
    // messages into the unified registry (sNeiItems[] in extended_player.c). GetCustomItemMessage
    // falls back to those rows via Nei_FindByRg. Only items NOT in that registry remain below.

    // ─────────────────────────────────────────────────────────────────────────
    // Extended Equipment (12 items, equipment page 2 - toggled via [L] in pause)
    // ─────────────────────────────────────────────────────────────────────────
    { RG_EXT_CANE_OF_BYRNA, static_cast<ItemID>(ITEM_EXT_SWORD_1),
      "You got the %cCane of Byrna%w!&A blue cane of legend.^Equip on the %ysword slot%w&(%y\xA2%w toggles equipment "
      "pages).^%y\xA0\xA0\xA0%w chains, %yforward + \xA0%w thrusts,&%y\xA4%w + %y\x9F%w jump-slashes, and %y\xA3%w "
      "sends&the %gKinsect%w - at full charge it&launches you instead.^Airborne: %y\x9F%w dashes, %y\xA0%w spins,&"
      "%y\xA3%w ground-pounds.",
      "Du hast den %cStab von Byrna%w!&Ein blauer Stab der Legenden.^Rüste ihn am %ySchwert-Platz%w aus&(%y\xA2%w "
      "wechselt Seiten).^%y\xA0\xA0\xA0%w als Kette, %yvorwärts + \xA0%w sticht,&%y\xA4%w + %y\x9F%w springt, %y\xA3%w "
      "schickt&den %gKinsect%w - voll geladen&schleudert er dich selbst.^In der Luft: %y\x9F%w Sprint, %y\xA0%w "
      "Wirbel,&%y\xA3%w Stampfer.",
      "Vous obtenez la %cCanne de Byrna%w!&Une canne bleue de légende.^Équipez-la dans l'%yemplacement "
      "épée%w&(%y\xA2%w change de page).^%y\xA0\xA0\xA0%w enchaîne, %yavant + \xA0%w transperce,&%y\xA4%w + %y\x9F%w "
      "saute, %y\xA3%w lance&le %gKinsect%w - à pleine charge&c'est vous qu'il propulse.^En l'air: %y\x9F%w foncez, "
      "%y\xA0%w tournoyez,&%y\xA3%w écrasez." },

    { RG_EXT_FOUR_SWORD, static_cast<ItemID>(ITEM_EXT_SWORD_2),
      "You got the %gFour Sword%w!&A blade that splits its wielder&into four heroes.^Equip on the %ysword slot%w "
      "(%y\xA2%w toggles).^Hold %y\xA3%w + %y\xA0%w for 15 frames ->&%g3 colored clones%w (Red/Blue/Purple)&spawn "
      "around you in a triangle.^Each clone costs %g12 Magic%w.&Clones %gmirror your swings%w and copy&your "
      "%garrows%w, %gbombs%w and %gboomerang%w.^Enemy hits kill them.",
      "Du hast das %gVier-Schwert%w!&Eine Klinge die ihren Träger&in vier Helden teilt.^Rüste es am %ySchwert-Platz%w "
      "aus.^Halte %y\xA3%w + %y\xA0%w 15 Frames ->&%g3 farbige Klone%w (Rot/Blau/Violett)&erscheinen im Dreieck.^Jeder "
      "Klon kostet %g12 Magie%w.&Klone %gspiegeln deine Schwerthiebe%w und&kopieren %gPfeile%w, %gBomben%w und "
      "%gBumerang%w.^Feindtreffer töten sie.",
      "Vous obtenez l'%gÉpée de Quatre%w!&Une lame qui divise son porteur&en quatre héros.^Équipez-la dans "
      "l'%yemplacement épée%w.^Maintenez %y\xA3%w + %y\xA0%w 15 frames ->&%g3 clones colorés%w "
      "(Rouge/Bleu/Violet)&apparaissent en triangle.^Chaque clone coûte %g12 Magie%w.&Les clones %gimitent vos coups%w "
      "et copient&%gflèches%w, %gbombes%w et %gboomerang%w.^Les ennemis les tuent au contact." },

    { RG_PROGRESSIVE_HAMMER, static_cast<ItemID>(ITEM_HAMMER),
      "You got a %rProgressive Hammer%w!&First the %rMegaton Hammer%w, then the&%rIron Knuckle's Axe%w - %gdouble "
      "damage%w,&%gdouble reach%w, and a tomahawk&%rthrow%w that boomerangs back.^%y\xA5%w aims it, the hammer's own "
      "%y\xA1%w&button lets it fly.",
      "Du hast das %rHammer-Upgrade%w!&Dein %rStahlhammer%w wird zur&%rEisenknöchel-Axt%w - dem massiven&Tomahawk der "
      "Ritter Ganons.^Schwerer chunky Schwung:&%gdoppelter Schaden%w, %gdoppelte Reichweite%w,&langsameres "
      "Gehen.^%y\xA5%w zielt, die %y\xA1%w-Taste des&Hammers %rwirft%w die Axt - sie kommt&wie ein Bumerang zurück.",
      "Vous obtenez l'%rAmélioration de Masse%w!&Votre %rMasse des Titans%w devient la&%rHache d'Iron Knuckle%w - le "
      "tomahawk&massif des chevaliers de Ganon.^Coups lourds:&%gdouble dégâts%w, %gdouble portée%w,&marche plus "
      "lente.^%y\xA5%w vise, la touche %y\xA1%w de la masse&%rlance%w la hache - elle revient&en boomerang." },

    { RG_PROGRESSIVE_KOKIRI_SWORD, static_cast<ItemID>(ITEM_SWORD_KOKIRI),
      "You got a %gKokiri Sword Upgrade%w!&Sharpens your %gKokiri Sword%w&into the %gRazor Sword%w, then the&%gGilded "
      "Sword%w.",
      "Du hast ein %gKokiri-Schwert-Upgrade%w!&Schärft dein %gKokiri-Schwert%w&zum %gElfenschwert%w, dann "
      "zur&%gSchmirgelklinge%w.",
      "Vous obtenez une %gAmélioration d'Épée Kokiri%w!&Aiguise votre %gÉpée Kokiri%w&en %gLame Rasoir%w, puis "
      "en&%gExcalibur%w." },

    { RG_PROGRESSIVE_MASTER_SWORD, static_cast<ItemID>(ITEM_SWORD_MASTER),
      "You got a %cProgressive Master Sword%w!&First the %cMaster Sword%w, then the&%cReal Master Sword%w - at full "
      "health&a swing fires a thunder beam.",
      "Du hast das %cWahre Master-Schwert%w!&Dein %cMaster-Schwert%w erwacht zu&seiner wahren Kraft.",
      "Vous obtenez la %cVéritable Épée de Légende%w!&Votre %cÉpée de Légende%w révèle&son vrai pouvoir." },

    { RG_PROGRESSIVE_BGS, static_cast<ItemID>(ITEM_SWORD_BGS),
      "You got a %pProgressive Biggoron's Sword%w!&First the %yBiggoron Sword%w, then the&%pGreat Fairy's Sword%w - "
      "long reach&that restores HP and Magic on hit.",
      "Du hast das %pSchwert der Großen Fee%w!&Dein %yBiggoron-Schwert%w wird zur&legendären Klinge der Großen "
      "Fee&umgeschmiedet.",
      "Vous obtenez l'%pÉpée de la Grande Fée%w!&Votre %yÉpée de Biggoron%w est reforgée&en lame légendaire bénie "
      "par&la Grande Fée." },

    // Per-level chain identities: the progressive resolution (item.cpp) lands on these, so each
    // give reads as the level actually received. Skijer's NEI
    { RG_RAZOR_SWORD, static_cast<ItemID>(ITEM_SWORD_KOKIRI),
      "You got the %gRazor Sword%w!&Your Kokiri Sword has been&sharpened into a keener blade -&%gdouble damage%w on "
      "every slash.",
      "Du hast das %gElfenschwert%w!&Dein Kokiri-Schwert wurde zu&einer schärferen Klinge geschliffen -&%gdoppelter "
      "Schaden%w.",
      "Vous obtenez la %gLame Rasoir%w!&Votre Épée Kokiri a été aiguisée -&%gdégâts doublés%w à chaque coup." },

    { RG_GILDED_SWORD, static_cast<ItemID>(ITEM_SWORD_KOKIRI),
      "You got the %yGilded Sword%w!&Reforged with gold dust, the&final form of your Kokiri blade -&%ydouble damage%w "
      "and it never dulls.",
      "Du hast die %ySchmirgelklinge%w!&Mit Goldstaub neu geschmiedet -&die finale Form deiner Kokiri-Klinge.",
      "Vous obtenez %yExcalibur%w!&Reforgée avec de la poudre d'or -&la forme finale de votre lame Kokiri." },

    { RG_TRUE_MASTER_SWORD, static_cast<ItemID>(ITEM_SWORD_MASTER),
      "The %cMaster Sword%w has awakened as&the %cTrue Master Sword%w!&At full health, a swing fires a&%cthunder "
      "beam%w at your foes.",
      "Das %cMaster-Schwert%w ist als&%cWahres Master-Schwert%w erwacht!&Bei voller Energie feuert jeder&Schwung einen "
      "%cDonnerstrahl%w.",
      "L'%cÉpée de Légende%w s'éveille en&%cVéritable Épée de Légende%w!&Pleine vie: chaque coup tire&un %crayon de "
      "tonnerre%w." },

    { RG_GREAT_FAIRY_SWORD, static_cast<ItemID>(ITEM_SWORD_BGS),
      "You got the %pGreat Fairy's Sword%w!&Your Biggoron Sword reforged into&the fairy blade - hits %prestore&HP and "
      "Magic%w.",
      "Du hast das %pSchwert der Großen Fee%w!&Dein Biggoron-Schwert, neu geschmiedet -&Treffer %pstellen Herzen und "
      "Magie&wieder her%w.",
      "Vous obtenez l'%pÉpée de la Grande Fée%w!&Votre Épée de Biggoron reforgée -&les coups %prestaurent vie et "
      "magie%w." },

    { RG_IRON_KNUCKLE_AXE, static_cast<ItemID>(ITEM_HAMMER),
      "You got the %rIron Knuckle's Axe%w!&The massive tomahawk of Ganon's&knights - %gdouble damage%w, %gdouble "
      "reach%w.^%y\xA5%w aims the %rthrow%w, the hammer's&own %y\xA1%w button lets it fly.",
      "Du hast die %rEisenknöchel-Axt%w!&Der massive Tomahawk der Ritter&Ganons - %gdoppelter Schaden%w,&%gdoppelte "
      "Reichweite%w.^%y\xA5%w zielt, die %y\xA1%w-Taste des&Hammers %rwirft%w sie.",
      "Vous obtenez la %rHache d'Iron Knuckle%w!&Le tomahawk massif des chevaliers&de Ganon - %gdouble "
      "dégâts%w,&%gdouble portée%w.^%y\xA5%w vise, la touche %y\xA1%w de la&masse la %rlance%w." },

    { RG_ULTRASHOT, static_cast<ItemID>(ITEM_LONGSHOT),
      "You got the %yUltrashot%w!&Your Longshot surges with light -&%y4x reach%w and %y2x speed%w.&Nothing is out of "
      "range now.",
      "Du hast den %yUltraschot%w!&Dein Enterhaken pulsiert vor Licht -&%y4-fache Reichweite%w, "
      "%y2-fache&Geschwindigkeit%w.",
      "Vous obtenez l'%yUltra-Grappin%w!&Votre grappin déborde de lumière -&%yportée x4%w et %yvitesse x2%w." },

    { RG_QUARTZ_OF_MOTION, static_cast<ItemID>(ITEM_STONE_OF_AGONY),
      "Your Stone of Agony crystallized&into the %pQuartz of Motion%w!&Press %yA%w on its pause slot&to attune to "
      "hidden movement.",
      "Dein Stein der Qualen wurde zum&%pBewegungsquarz%w!&Drücke %yA%w auf seinem Menüplatz&um verborgene Bewegung zu "
      "spüren.",
      "Votre Pierre de Souffrance devient&le %pQuartz du Mouvement%w!&Appuyez sur %yA%w dans le menu&pour sentir les "
      "mouvements cachés." },

    { RG_ROCS_CAPE, static_cast<ItemID>(ITEM_ROCS_CAPE),
      "Your feather grew into the&%rRoc's Cape%w!&Jump, then hold the button to&%rglide%w gently to the ground.",
      "Deine Feder wurde zum&%rRocs Umhang%w!&Springe und halte die Taste&um sanft zu %rgleiten%w.",
      "Votre plume devient la&%rCape de Roc%w!&Sautez puis maintenez pour&%rplaner%w doucement." },

    // Dual Cane per-skill textboxes (order: Statue keeps RG_CANE_OF_SOMARIA's own message).
    { RG_CANE_PACCI_FLIP, static_cast<ItemID>(ITEM_CANE_OF_SOMARIA),
      "You got the %yCane of Pacci%w!&Its charge %yflips objects%w and&%ylaunches you%w from holes.&It shares the "
      "cane's slot.",
      "Du hast den %yStab von Pacci%w!&Seine Ladung %ydreht Objekte um%w&und %ykatapultiert dich%w aus Löchern.",
      "Vous obtenez la %yCanne de Pacci%w!&Sa charge %yretourne les objets%w&et vous %ypropulse%w des trous." },

    { RG_CANE_SOMARIA_BLOCK, static_cast<ItemID>(ITEM_CANE_OF_SOMARIA),
      "Your Cane of Somaria learned&%rBlock%w!&Conjure a %rsolid block%w to push,&weigh switches, or climb on.",
      "Dein Stab von Somaria lernte&%rBlock%w!&Beschwöre einen %rfesten Block%w&für Schalter und Kletterei.",
      "Votre Canne de Somaria apprend&%rBloc%w!&Créez un %rbloc solide%w à pousser&ou pour grimper." },

    { RG_CANE_PACCI_STONE, static_cast<ItemID>(ITEM_CANE_OF_SOMARIA),
      "Your Cane of Pacci learned&%yStone%w!&%yPetrify enemies%w and use them&as stepping stones.",
      "Dein Stab von Pacci lernte&%yStein%w!&%yVersteinere Gegner%w und nutze&sie als Trittsteine.",
      "Votre Canne de Pacci apprend&%yPierre%w!&%yPétrifiez les ennemis%w et&servez-vous-en de marches." },

    { RG_CANE_SOMARIA_PLATFORM, static_cast<ItemID>(ITEM_CANE_OF_SOMARIA),
      "Your Cane of Somaria learned&%rPlatform%w!&Conjure a %rfloating platform%w&that carries you across gaps.",
      "Dein Stab von Somaria lernte&%rPlattform%w!&Beschwöre eine %rschwebende&Plattform%w über Abgründe.",
      "Votre Canne de Somaria apprend&%rPlateforme%w!&Créez une %rplateforme flottante%w&pour franchir les vides." },

    { RG_CANE_PACCI_ULTRAHAND, static_cast<ItemID>(ITEM_CANE_OF_SOMARIA),
      "Your Cane of Pacci learned&%yUltrahand%w!&%yGrab, move and attach%w distant&objects with the glowing hand.",
      "Dein Stab von Pacci lernte&%yUltrahand%w!&%yGreife und bewege%w ferne Objekte&mit der leuchtenden Hand.",
      "Votre Canne de Pacci apprend&%yUltrahand%w!&%ySaisissez et déplacez%w des objets&avec la main lumineuse." },

    { RG_EXT_DIVINE_SHIELD, static_cast<ItemID>(ITEM_EXT_SHIELD_1),
      "You got the %yDivine Shield%w!&A blessed wooden shield said to&repel even the wrath of fire.^Equip on the "
      "%yshield slot%w (%y\xA2%w toggles).^Light wooden shield BUT %rfireproof%w -&fire breath, Dodongo flames "
      "and&torches will not burn it.^%cPerfect Parry%w (%y\xA3%w + block within&10 frames of an attack):&%cfreezes ALL "
      "enemies%w on screen!",
      "Du hast den %yGötterschild%w!&Ein gesegneter Holzschild der selbst&dem Zorn des Feuers widersteht.^Rüste ihn am "
      "%ySchild-Platz%w aus.^Leichter Holzschild ABER %rfeuerfest%w -&Feueratem, Dodongo-Flammen und&Fackeln "
      "verbrennen ihn nicht.^%cPerfekte Parade%w (%y\xA3%w + block in&den ersten 10 Frames eines Angriffs):&%cfriert "
      "ALLE Feinde%w auf dem Schirm ein!",
      "Vous obtenez le %yBouclier Divin%w!&Un bouclier en bois béni qui&résiste à la colère du feu.^Équipez-le dans "
      "l'%yemplacement bouclier%w.^Bouclier en bois MAIS %rignifuge%w -&souffle de feu, flammes de Dodongo&et torches "
      "ne le brûlent pas.^%cParade Parfaite%w (%y\xA3%w + bloquer dans&les 10 premières frames d'une attaque):&%cgèle "
      "TOUS les ennemis%w à l'écran!" },

    { RG_EXT_SHEIKAH_SHIELD, static_cast<ItemID>(ITEM_EXT_SHIELD_2),
      "You got the %cKite Shield%w!&A rider's shield, made to be&stood on.^Equip on the %yshield slot%w "
      "(%y\xA2%w toggles).^Press %y\xA3%w %rin mid-air%w to drop it&under your feet and %gsurf%w.&Downhill builds "
      "speed with no cap.^%y\x9F%w hops, %y\xA0%w spins,&%y\xA0%w + %y\xA3%w gets off.",
      "Du hast den %cNormannenschild%w!&Ein Reiterschild, gemacht&zum Draufstehen.^Rüste ihn am %ySchild-Platz%w "
      "aus.^%y\xA3%w %rin der Luft%w legt ihn unter&deine Füße und du %gsurfst%w.&Bergab wird es immer "
      "schneller.^%y\x9F%w hüpft, %y\xA0%w dreht,&%y\xA0%w + %y\xA3%w steigt ab.",
      "Vous obtenez le %cBouclier Normand%w!&Un bouclier de cavalier, fait&pour qu'on monte dessus.^Équipez-le dans "
      "l'%yemplacement bouclier%w.^%y\xA3%w %ren l'air%w le glisse sous vos&pieds et vous %gsurfez%w.&En descente, "
      "aucune limite de vitesse.^%y\x9F%w saute, %y\xA0%w pivote,&%y\xA0%w + %y\xA3%w descend." },

    { RG_EXT_SHIELD_OF_IKANA, static_cast<ItemID>(ITEM_EXT_SHIELD_3),
      "You got the %pShield of Ikana%w!&A cursed mirror shield from the&fallen kingdom of Ikana.^Equip on the %yshield "
      "slot%w (%y\xA2%w toggles).^%cSoul Drain%w (%y\xA3%w + block within&12 frames of an attack):&drains the "
      "attacker's %rHP%w and&heals you for half a heart.^%pDeath Save%w: when struck dead,&%previves once per scene%w "
      "with&3 hearts and a dark aura.",
      "Du hast den %pSchild von Ikana%w!&Ein verfluchter Spiegelschild aus&dem gefallenen Reich Ikana.^Rüste ihn am "
      "%ySchild-Platz%w aus.^%cSeelenraub%w (%y\xA3%w + block in&den ersten 12 Frames eines Angriffs):&saugt %rHP%w "
      "des Angreifers und&heilt dich um ein halbes Herz.^%pTodesrettung%w: bei tödlichem Treffer&%pwiederbelebt einmal "
      "pro Szene%w mit&3 Herzen und dunkler Aura.",
      "Vous obtenez le %pBouclier d'Ikana%w!&Un bouclier-miroir maudit du&royaume déchu d'Ikana.^Équipez-le dans "
      "l'%yemplacement bouclier%w.^%cVol d'Âme%w (%y\xA3%w + bloquer dans&les 12 premières frames d'une attaque):&vole "
      "les %rPV%w de l'attaquant et&vous soigne d'un demi-cœur.^%pSauvegarde de Mort%w: ressuscite&%pune fois par "
      "scène%w avec 3 cœurs&et une aura sombre." },

    { RG_EXT_TRIDENT, static_cast<ItemID>(ITEM_EXT_SWORD_3),
      "You got the %pTrident%w!&A gunlance in all but name.^Equip on the %ysword slot%w (%y\xA2%w toggles).^%y\xA0%w "
      "chains three slashes, %yforward + \xA0%w&lunges, and %rholding%w %y\xA0%w charges&three levels - the last one "
      "fires.^%y\xA3%w guards, %y\xA3%w + %y\xA0%w dashes with the&lance out front, and %rholding%w %y\xA3%w + "
      "%y\x9F%w "
      "%gflies%w.^In flight: stick moves, %y\xA3%w up, %y\xA2%w down,&%y\xA0%w fires, %y\x9F%w dives at the target.",
      "Du hast den %pDreizack%w!&Eine Gunlance, nur anders benannt.^Rüste ihn am %ySchwert-Platz%w aus.^%y\xA0%w "
      "verkettet drei Hiebe, %yvorwärts + \xA0%w&sticht, %rHalten%w von %y\xA0%w lädt drei&Stufen - die letzte "
      "feuert.^%y\xA3%w blockt, %y\xA3%w + %y\xA0%w stürmt mit der&Lanze voran, %rHalten%w von %y\xA3%w + %y\x9F%w "
      "%gfliegt%w.^Im Flug: Stick lenkt, %y\xA3%w hoch, %y\xA2%w runter,&%y\xA0%w feuert, %y\x9F%w stürzt aufs Ziel.",
      "Vous obtenez le %pTrident%w!&Une gunlance qui ne dit pas son nom.^Équipez-le dans l'%yemplacement "
      "épée%w.^%y\xA0%w enchaîne trois coups, %yavant + \xA0%w&fend, et %rmaintenir%w %y\xA0%w charge trois&niveaux - "
      "le dernier tire.^%y\xA3%w pare, %y\xA3%w + %y\xA0%w charge lance en&avant, %rmaintenir%w %y\xA3%w + %y\x9F%w "
      "fait %gvoler%w.^En vol: le stick dirige, %y\xA3%w monte,&%y\xA2%w descend, %y\xA0%w tire, %y\x9F%w plonge." },

    { RG_EXT_CLIMB_BOOTS, static_cast<ItemID>(ITEM_EXT_BOOTS_2),
      "You got the %yClimb Boots%w!&Equip on the %yboots slot%w (%y\xA2%w toggles).^They grip every floor: %cice%w "
      "stops&being slippery and %csteep slopes%w&stop sliding you away.",
      "Du hast die %yKletterstiefel%w!&Rüste sie am %yStiefel-Platz%w aus.^Sie greifen jeden Boden: %cEis%w ist&nicht "
      "mehr rutschig und %csteile Hänge%w&lassen dich nicht mehr abgleiten.",
      "Vous obtenez les %yBottes d'Escalade%w!&Équipez-les dans l'%yemplacement bottes%w.^Elles agrippent tout sol: "
      "la %cglace%w&ne glisse plus et les %cpentes raides%w&ne vous font plus déraper." },

    { RG_EXT_ROC_BOOTS, static_cast<ItemID>(ITEM_EXT_BOOTS_3),
      "You got %rRoc's Boots%w!&Equip on the %yboots slot%w (%y\xA2%w toggles).^%cWater and lava%w become solid "
      "ground&under your feet, and you fall at&%chalf gravity%w - so every jump&goes higher.",
      "Du hast %rRocs Stiefel%w!&Rüste sie am %yStiefel-Platz%w aus.^%cWasser und Lava%w werden zu festem&Boden, und "
      "du fällst mit %chalber&Schwerkraft%w - jeder Sprung geht höher.",
      "Vous obtenez les %rBottes de Roc%w!&Équipez-les dans l'%yemplacement bottes%w.^L'%ceau et la lave%w deviennent "
      "du sol&solide, et vous tombez en %cgravité&réduite de moitié%w - tous vos sauts&montent plus haut." },

    // (2026-08-07: texto legacy corregido — la capa ya NO ocupa el slot de túnica; es una pieza
    // propia de la columna de upgrades que se activa sola al poseerla.)
    { RG_EXT_MAGIC_CAPE, static_cast<ItemID>(ITEM_EXT_TUNIC_1),
      "You got the %pMagic Cape%w!&Ganondorf's enchanted cloak,&woven of pure dark mantle cloth.^It %phangs from your "
      "shoulders%w the&moment you own it - real %pcloth&physics%w sway with movement and wind.^All magic %ccosts are "
      "halved%w&(rounded down) while you own it -&cheap items become free.",
      "Du hast den %pZauberumhang%w!&Ganondorfs verzauberter Mantel,&gewebt aus dunklem Mantelstoff.^Er %phängt von "
      "deinen Schultern%w&sobald du ihn besitzt - echte&%pStoff-Physik%w schwingt mit Bewegung.^Alle Magie%ckosten "
      "sind halbiert%w&(abgerundet) solange du ihn hast.",
      "Vous obtenez la %pCape Magique%w!&Le manteau enchanté de Ganondorf,&tissé de pure étoffe sombre.^Elle %ppend de "
      "vos épaules%w dès que&vous la possédez - %pphysique de&tissu%w réelle au vent.^Tous les %ccoûts de magie sont "
      "réduits&de moitié%w (arrondi vers le bas)." },

    { RG_EXT_SPIRIT_BREASTPLATE, static_cast<ItemID>(ITEM_EXT_TUNIC_2),
      "You got the %ySpirit Tunic%w!&The golden ward of the Iron&Knuckle Nabooru.^Equip on the %ytunic slot%w "
      "(%y\xA2%w toggles).^Damage costs %gRupees%w instead&of hearts (1 HP = 1 Rupee), and&some of it spills out as "
      "pickups.^While you hold Rupees the %rheat%w and&%bunderwater%w timers never start.^If your wallet runs "
      "%rempty%w,&you take damage normally and&move at half speed.",
      "Du hast den %ySpirit-Brustpanzer%w!&Die goldene Rüstung der Eisenknöchel&Nabooru.^Rüste ihn am %yTunika-Platz%w "
      "aus.^Schaden kostet %gRupien%w statt&Herzen (1 HP = 1 Rupie).&%gPassiver Verbrauch%w: 1 Rupie alle&30 Frames im "
      "Tragen.^Wenn dein Beutel %rleer%w ist,&erleidest du Schaden normal und&bewegst dich halb so schnell.",
      "Vous obtenez le %yPlastron Spirituel%w!&L'armure dorée de l'Iron Knuckle&Nabooru.^Équipez-le dans "
      "l'%yemplacement tunique%w.^Les dégâts coûtent des %gRubis%w au&lieu de cœurs (1 PV = 1 Rubis).&%gDrain "
      "passif%w: 1 Rubis toutes&les 30 frames tant que porté.^Si votre bourse est %rvide%w,&vous prenez les dégâts "
      "normalement&et bougez à mi-vitesse." },

    { RG_EXT_CHAMPIONS_TUNIC, static_cast<ItemID>(ITEM_EXT_TUNIC_1),
      "You got the %cChampion's Tunic%w!&The blue garb of Hyrule's chosen,&blessed with battle aura.^Equip on the "
      "%ytunic slot%w (%y\xA2%w toggles).&Dyes your tunic %cchampion blue%w.^%gFlurry Rush%w: sidehop or backflip&past "
      "a nearby attack -> world slows&to 33% with iframes for ~2s or&until you land 7 hits.^%cBullet Time%w: aim while "
      "airborne&with bow/slingshot/hookshot/boomerang&-> time slows and you float while&the normal aim controls stay "
      "active.",
      "Du hast die %cRüstung des Helden%w!&Die blaue Tracht des Auserwählten&Hyrules, mit Kampfaura gesegnet.^Rüste "
      "sie am %yTunika-Platz%w aus.^Färbt deine Tunika %cheldenblau%w.^%gFlurry Rush%w: Weiche einem nahen&Angriff per "
      "Seitsprung oder Backflip aus&-> Welt auf 33% verlangsamt, mit&i-Frames für ~2s oder bis zu 7 Treffer.^%cBullet "
      "Time%w: Ziele in der Luft mit&Bogen/Schleuder/Greifhaken/Bumerang&-> Zeit verlangsamt, du schwebst und&die "
      "normale Zielsteuerung bleibt aktiv.",
      "Vous obtenez la %cTunique du Héros%w!&Le vêtement bleu de l'élu d'Hyrule,&béni d'une aura de combat.^Équipez-la "
      "dans l'%yemplacement tunique%w.^Teint votre tunique en %cbleu du héros%w.^%gFlurry Rush%w: esquivez une "
      "attaque&proche d'un saut latéral ou arrière&-> monde ralenti à 33%, invincible&~2 s ou jusqu'à 7 "
      "coups.^%cBullet Time%w: visez en l'air avec&arc/lance-pierre/grappin/boomerang&-> le temps ralentit, vous "
      "flottez et&la visée normale reste active." },

    { RG_EXT_PEGASUS_ANKLET, static_cast<ItemID>(ITEM_EXT_BOOTS_1),
      "You got the %rPegasus Anklet%w!&Winged anklets that grant the&speed of the legendary Pegasus.^Equip on the "
      "%yboots slot%w (%y\xA2%w toggles).^Hold %y\xA0%w after a sword swing&(intercepts the spin attack charge):&Link "
      "%glunges forward%w with sword&extended, dealing damage on contact.^A %gwind cone barrier%w forms in&front while "
      "you have Magic&(1 MP per 15 frames).&Walls cause a %rbonk%w recovery.",
      "Du hast den %rPegasus-Fußreif%w!&Geflügelte Fußreifen mit der&Geschwindigkeit des Pegasus.^Rüste sie am "
      "%yStiefel-Platz%w aus.^Halte %y\xA0%w nach einem Schwertschlag&(unterbricht den Aufladeangriff):&Link %gstürmt "
      "vor%w mit ausgestrecktem&Schwert, Schaden bei Kontakt.^Ein %gWindkegel%w bildet sich vor dir&solange du Magie "
      "hast (1 MP pro&15 Frames). Wände lösen einen&%rZusammenstoß%w aus.",
      "Vous obtenez le %rBracelet de Pégase%w!&Des bracelets ailés qui octroient&la vitesse du légendaire "
      "Pégase.^Équipez-le dans l'%yemplacement bottes%w.^Maintenez %y\xA0%w après un coup d'épée&(intercepte la charge "
      "tournoyante):&Link %ss'élance%w l'épée tendue,&infligeant des dégâts au contact.^Un %gcône de vent%w protecteur "
      "se forme&devant tant que vous avez de la Magie&(1 MP toutes les 15 frames).&Les murs causent un %rchoc%w." },

    { RG_EXT_PENDANT_OF_MEMORIES, static_cast<ItemID>(ITEM_EXT_BOOTS_2),
      "You got the %pPendant of Memories%w!&A pendant carrying the techniques&of heroes past.^Equip on the %yboots "
      "slot%w (%y\xA2%w toggles).^Three combat techniques unlock:^%c#1 Mortal Draw%w (TP): %y\xA0%w near an&enemy + "
      "sheathed + still + NOT&%y\xA4%w-targeting -> devastating draw&slash, often a one-hit kill.^%c#2 Ground Pound%w "
      "(Smash): %y\xA0%w in&air with sword -> fast fall ->&pogo bounce on hit, shockwave on landing.^%c#3 Parry Leap%w "
      "(WW): %y\xA4%w-target +&3 sidehops + %y\xA0%w -> parabolic arc&over the foe, land behind them.",
      "Du hast das %pAmulett der Erinnerungen%w!&Ein Anhänger mit Techniken vergangener&Helden.^Rüste es am "
      "%yStiefel-Platz%w aus.^Drei Kampftechniken werden frei:^%c#1 Mortal Draw%w (TP): %y\xA0%w bei einem&Feind + "
      "eingesteckt + still + NICHT&%y\xA4%w-fokussieren -> vernichtender Hieb,&oft One-Hit-Kill.^%c#2 Ground Pound%w "
      "(Smash): %y\xA0%w in&der Luft mit Schwert -> schneller Fall&-> Bounce bei Treffer, Schockwelle "
      "beim&Landen.^%c#3 Parry Leap%w (WW): %y\xA4%w-fokussieren&+ 3 Seitsprünge + %y\xA0%w -> parabolischer&Bogen "
      "über den Feind, hinter ihm landen.",
      "Vous obtenez le %pPendentif des Souvenirs%w!&Un pendentif portant les techniques&des héros passés.^Équipez-le "
      "dans l'%yemplacement bottes%w.^Trois techniques de combat:^%c#1 Mortal Draw%w (TP): %y\xA0%w près d'un&ennemi + "
      "rengainé + immobile + PAS&en %y\xA4%w-cible -> tranche dévastatrice,&souvent un one-shot.^%c#2 Ground Pound%w "
      "(Smash): %y\xA0%w en l'air&avec épée -> chute rapide -> rebond&sur impact, onde de choc à l'atterrissage.^%c#3 "
      "Parry Leap%w (WW): %y\xA4%w-cible +&3 esquives + %y\xA0%w -> arc parabolique&par-dessus l'ennemi, atterrir "
      "derrière." },

    { RG_EXT_WATER_DRAGON_SCALE, static_cast<ItemID>(ITEM_EXT_TUNIC_3),
      "You got the %wSage's Tunic%w!&Equip it on the %ytunic slot%w.^Its passive resistances follow your&owned "
      "medallions: %bice%w, %rfire%w,&%ythunder%w, %pstun%w, fall and wind.",
      "Du hast das %wOrni-Gewand%w!&Rüste es am %yTunika-Platz%w aus.^Seine Resistenzen folgen deinen&Medaillons: "
      "%bEis%w, %rFeuer%w, %yBlitz%w,&%pBetäubung%w, Sturz und Wind.",
      "Vous obtenez la %wTunique des Piafs%w!&Équipez-la dans l'%yemplacement tunique%w.^Ses résistances suivent vos "
      "médaillons:&%bglace%w, %rfeu%w, %yfoudre%w, %pétourdissement%w,&chute et vent." },

    // Sheikah Slate runes — one textbox per sibling pickup (wand idiom). The icon is the slate
    // composite with the rune's badge; the flame on the get-item model matches the color named here.
    { RG_SLATE_RUNE_BOMB, static_cast<ItemID>(EXT_ITEM_SHEIKAH_SLATE),
      "Your %cSheikah Slate%w learned the&%bRemote Bomb%w rune!&An ancient rune glows cyan on&the slate's face.^Select "
      "it by holding %y\xA2%w for the rune&wheel. %y\xA1%w draws the slate, then&casts the active rune.",
      "Dein %cSheikah-Stein%w hat das&%bFernzündbomben%w-Modul gelernt!&Eine uralte Rune leuchtet cyan&auf dem "
      "Stein.^Wähle sie mit gehaltenem %y\xA2%w im&Runen-Rad. %y\xA1%w zieht den Stein&und wirkt dann die aktive Rune.",
      "Votre %cTablette Sheikah%w apprend le&module %bBombe à Distance%w!&Une rune ancienne brille en cyan&sur la "
      "tablette.^Sélectionnez-la en maintenant %y\xA2%w:&la roue des runes. "
      "%y\xA1%w sort la&tablette, puis lance la rune active." },
    { RG_SLATE_RUNE_MASTER_CYCLE, static_cast<ItemID>(EXT_ITEM_SHEIKAH_SLATE),
      "Your %cSheikah Slate%w learned the&%gMaster Cycle%w rune!&An ancient rune glows teal on&the slate's "
      "face.^Select it by holding %y\xA2%w for the rune&wheel. %y\xA1%w draws the slate, then&casts the active rune.",
      "Dein %cSheikah-Stein%w hat das&%gMaster Cycle%w-Modul gelernt!&Eine uralte Rune leuchtet türkis&auf dem "
      "Stein.^Wähle sie mit gehaltenem %y\xA2%w im&Runen-Rad. %y\xA1%w zieht den Stein&und wirkt dann die aktive Rune.",
      "Votre %cTablette Sheikah%w apprend le&module %gMaster Cycle%w!&Une rune ancienne brille en turquoise&sur la "
      "tablette.^Sélectionnez-la en maintenant %y\xA2%w:&la roue des runes. "
      "%y\xA1%w sort la&tablette, puis lance la rune active." },
    { RG_SLATE_RUNE_STASIS, static_cast<ItemID>(EXT_ITEM_SHEIKAH_SLATE),
      "Your %cSheikah Slate%w learned the&%yStasis%w rune!&An ancient rune glows gold on&the slate's face.^Select it "
      "by holding %y\xA2%w for the rune&wheel. %y\xA1%w draws the slate, then&casts the active rune.",
      "Dein %cSheikah-Stein%w hat das&%yStasis%w-Modul gelernt!&Eine uralte Rune leuchtet golden&auf dem Stein.^Wähle "
      "sie mit gehaltenem %y\xA2%w im&Runen-Rad. %y\xA1%w zieht den Stein&und wirkt dann die aktive Rune.",
      "Votre %cTablette Sheikah%w apprend le&module %yCinetis%w!&Une rune ancienne brille en or&sur la "
      "tablette.^Sélectionnez-la en maintenant %y\xA2%w:&la roue des runes. "
      "%y\xA1%w sort la&tablette, puis lance la rune active." },
    { RG_SLATE_RUNE_CRYONIS, static_cast<ItemID>(EXT_ITEM_SHEIKAH_SLATE),
      "Your %cSheikah Slate%w learned the&%bCryonis%w rune!&An ancient rune glows ice-blue on&the slate's face.^Select "
      "it by holding %y\xA2%w for the rune&wheel. %y\xA1%w draws the slate, then&casts the active rune.",
      "Dein %cSheikah-Stein%w hat das&%bCryonis%w-Modul gelernt!&Eine uralte Rune leuchtet eisblau&auf dem "
      "Stein.^Wähle sie mit gehaltenem %y\xA2%w im&Runen-Rad. %y\xA1%w zieht den Stein&und wirkt dann die aktive Rune.",
      "Votre %cTablette Sheikah%w apprend le&module %bGlaciera%w!&Une rune ancienne brille en bleu&glacé sur la "
      "tablette.^Sélectionnez-la en maintenant %y\xA2%w:&la roue des runes. "
      "%y\xA1%w sort la&tablette, puis lance la rune active." },

    { RG_DESIRE_SENSOR, static_cast<ItemID>(EXT_ITEM_SHEIKAH_SLATE),
      "Your %cSheikah Slate%w learned the&%pSensor%w rune!&An ancient rune glows violet on&the slate's face.^Cast it "
      "to ask where one of your&%gdesired items%w hides. Each answer&costs a %rHeart Container%w, forever.",
      "Dein %cSheikah-Stein%w hat das&%pSensor%w-Modul gelernt!&Eine uralte Rune leuchtet violett&auf dem "
      "Stein.^Frage damit, wo eines deiner&%gWunsch-Items%w liegt. Jede Antwort&kostet ein %rHerzteil%w, f\xFCr immer.",
      "Votre %cTablette Sheikah%w apprend le&module %pCapteur%w!&Une rune ancienne brille en violet&sur la "
      "tablette.^Demandez o\xF9 se cache un de vos&%gobjets d\xE9sir\xE9s%w. Chaque r\xE9ponse&co\xFB"
      "te un %rC\x9C"
      "ur%w, pour toujours." },

    // Rod of Seasons — one textbox per sibling pickup. The flame on the get-item model matches the
    // colour named here, and hold %y\xA0%w on the rod's button opens the season wheel.
    { RG_SEASON_SPRING, static_cast<ItemID>(EXT_ITEM_ROD_OF_SEASONS),
      "Your %cRod of Seasons%w drew in&%pSpring%w!&Blossom drifts on the wind&wherever you carry it.^%rPress%w the "
      "rod's %y\xA1%w button again to&change season. %y\x9F%w confirms, %y\xA0%w cancels.",
      "Dein %cZepter der Jahreszeiten%w zog&den %pFrühling%w ein!&Blüten treiben im Wind,&wohin du es auch "
      "trägst.^%rDrücke%w die %y\xA1%w-Taste des Zepters&erneut, um die Jahreszeit zu wechseln.",
      "Votre %cSceptre des Saisons%w attire&le %pPrintemps%w!&Les pétales dérivent au vent&où que vous "
      "alliez.^%rAppuyez%w sur la touche %y\xA1%w du sceptre&pour changer de saison." },

    { RG_SEASON_SUMMER, static_cast<ItemID>(EXT_ITEM_ROD_OF_SEASONS),
      "Your %cRod of Seasons%w drew in&%ySummer%w!&The sky stays clear and the sun&stands high.^%rPress%w the rod's "
      "%y\xA1%w button again to&change season. %y\x9F%w confirms, %y\xA0%w cancels.",
      "Dein %cZepter der Jahreszeiten%w zog&den %ySommer%w ein!&Der Himmel bleibt klar und die&Sonne steht "
      "hoch.^%rDrücke%w die %y\xA1%w-Taste des Zepters&erneut, um die Jahreszeit zu wechseln.",
      "Votre %cSceptre des Saisons%w attire&l'%yÉté%w!&Le ciel reste clair et le soleil&est au zénith.^%rMaintenez%w "
      "la touche %y\xA1%w du sceptre&pour changer de saison." },

    { RG_SEASON_AUTUMN, static_cast<ItemID>(EXT_ITEM_ROD_OF_SEASONS),
      "Your %cRod of Seasons%w drew in&%rAutumn%w!&The sky greys over and the rain&never quite stops.^%rPress%w the "
      "rod's %y\xA1%w button again to&change season. %y\x9F%w confirms, %y\xA0%w cancels.",
      "Dein %cZepter der Jahreszeiten%w zog&den %rHerbst%w ein!&Der Himmel vergraut und der Regen&hört kaum "
      "auf.^%rDrücke%w die %y\xA1%w-Taste des Zepters&erneut, um die Jahreszeit zu wechseln.",
      "Votre %cSceptre des Saisons%w attire&l'%rAutomne%w!&Le ciel se voile et la pluie&ne cesse "
      "jamais.^%rAppuyez%w sur la touche %y\xA1%w du sceptre&pour changer de saison." },

    { RG_SEASON_WINTER, static_cast<ItemID>(EXT_ITEM_ROD_OF_SEASONS),
      "Your %cRod of Seasons%w drew in&%bWinter%w!&Snow falls under every open sky&you walk beneath.^%rPress%w the "
      "rod's %y\xA1%w button again to&change season. %y\x9F%w confirms, %y\xA0%w cancels.",
      "Dein %cZepter der Jahreszeiten%w zog&den %bWinter%w ein!&Unter jedem freien Himmel&fällt nun Schnee.^%rHalte%w "
      "die %y\xA1%w-Taste des Zepters&erneut, um die Jahreszeit zu wechseln.",
      "Votre %cSceptre des Saisons%w attire&l'%bHiver%w!&La neige tombe sous chaque ciel&ouvert.^%rMaintenez%w la "
      "touche %y\xA0%w du sceptre&pour changer de saison." },
};
static constexpr size_t customItemMessageCount = sizeof(customItemMessages) / sizeof(customItemMessages[0]);

// Helper function to get custom item message by RG ID
const CustomItemMessageEntry* GetCustomItemMessage(s16 rgId) {
    for (size_t i = 0; i < customItemMessageCount; i++) {
        if (customItemMessages[i].rgId == rgId) {
            return &customItemMessages[i];
        }
    }
    // Skijer's NEI: fall back to the unified registry. Messages for registry-backed items now live
    // in sNeiItems[] (one row per item); reproject the row's name strings onto a CustomItemMessageEntry.
    const NeiItem* nei = Nei_FindByRg(rgId);
    if (nei != nullptr && nei->nameEn != nullptr) {
        static CustomItemMessageEntry neiMsg;
        neiMsg.rgId = rgId;
        neiMsg.itemId = static_cast<ItemID>(nei->item);
        neiMsg.english = nei->nameEn;
        // Rows may leave FR/DE as NULL — fall back to English so the CustomMessage
        // std::string ctor never receives a null char* (crash on textbox open).
        neiMsg.french = nei->nameFr != nullptr ? nei->nameFr : nei->nameEn;
        neiMsg.german = nei->nameDe != nullptr ? nei->nameDe : nei->nameEn;
        return &neiMsg;
    }
    return nullptr;
}

bool Rando_HandleSpoilerDrop(char* filePath) {
    if (SohUtils::IsStringEmpty(filePath)) {
        return false;
    }

    try {
        std::ifstream stream(filePath);
        if (!stream) {
            return false;
        }

        nlohmann::json json;
        stream >> json;

        if (json.contains("version") && json.contains("finalSeed")) {
            CVarSetString(CVAR_GENERAL("RandomizerDroppedFile"), filePath);
            CVarSetInteger(CVAR_GENERAL("RandomizerNewFileDropped"), 1);
            return true;
        }
    } catch ([[maybe_unused]] std::exception& e) {}
    return false;
}

Randomizer::Randomizer() {
    Rando::StaticData::InitItemTable();
    Rando::StaticData::InitLocationTable();

    for (auto area : rcAreaNames) {
        SpoilerfileAreaNameToEnum[area.second] = area.first;
    }
    SpoilerfileAreaNameToEnum["Inside Ganon's Castle"] = RCAREA_GANONS_CASTLE;
    SpoilerfileAreaNameToEnum["the Lost Woods"] = RCAREA_LOST_WOODS;
    SpoilerfileAreaNameToEnum["the Market"] = RCAREA_MARKET;
    SpoilerfileAreaNameToEnum["the Graveyard"] = RCAREA_GRAVEYARD;
    SpoilerfileAreaNameToEnum["Haunted Wasteland"] = RCAREA_WASTELAND;
    SpoilerfileAreaNameToEnum["outside Ganon's Castle"] = RCAREA_HYRULE_CASTLE;
    for (size_t c = 0; c < Rando::StaticData::hintTypeNames.size(); c++) {
        SpoilerfileHintTypeNameToEnum[Rando::StaticData::hintTypeNames[(HintType)c].GetEnglish(MF_CLEAN)] = (HintType)c;
    }

    Ship::Context::GetRawInstance()->GetFileDropMgr()->RegisterDropHandler(Rando_HandleSpoilerDrop);
}

Randomizer::~Randomizer() {
}

std::unordered_map<std::string, SceneID> spoilerFileDungeonToScene = {
    { "Deku Tree", SCENE_DEKU_TREE },
    { "Dodongo's Cavern", SCENE_DODONGOS_CAVERN },
    { "Jabu Jabu's Belly", SCENE_JABU_JABU },
    { "Forest Temple", SCENE_FOREST_TEMPLE },
    { "Fire Temple", SCENE_FIRE_TEMPLE },
    { "Water Temple", SCENE_WATER_TEMPLE },
    { "Spirit Temple", SCENE_SPIRIT_TEMPLE },
    { "Shadow Temple", SCENE_SHADOW_TEMPLE },
    { "Bottom of the Well", SCENE_BOTTOM_OF_THE_WELL },
    { "Ice Cavern", SCENE_ICE_CAVERN },
    { "Gerudo Training Ground", SCENE_GERUDO_TRAINING_GROUND },
    { "Ganon's Castle", SCENE_INSIDE_GANONS_CASTLE }
};

#ifdef _MSC_VER
#pragma optimize("", off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif
bool Randomizer::SpoilerFileExists(const char* spoilerFileName) {
    static std::unordered_map<std::string, bool> existsCache;
    static std::unordered_map<std::string, std::filesystem::file_time_type> lastModifiedCache;

    if (strcmp(spoilerFileName, "") == 0) {
        return false;
    }

    std::string sanitizedFileName = SohUtils::Sanitize(spoilerFileName);

    try {
        // Check if file exists and get last modified time
        std::filesystem::path filePath(sanitizedFileName);
        if (!std::filesystem::exists(filePath)) {
            // Cache and return false if file doesn't exist
            existsCache[sanitizedFileName] = false;
            lastModifiedCache.erase(sanitizedFileName);
            return false;
        }

        auto currentLastModified = std::filesystem::last_write_time(filePath);

        // Check cache first
        auto existsCacheIt = existsCache.find(sanitizedFileName);
        auto lastModifiedCacheIt = lastModifiedCache.find(sanitizedFileName);

        // If we have a valid cache entry and the file hasn't been modified
        if (existsCacheIt != existsCache.end() && lastModifiedCacheIt != lastModifiedCache.end() &&
            lastModifiedCacheIt->second == currentLastModified) {
            return existsCacheIt->second;
        }

        // Cache miss or file modified - need to check contents
        std::ifstream spoilerFileStream(sanitizedFileName);
        if (spoilerFileStream) {
            nlohmann::json contents;
            spoilerFileStream >> contents;
            spoilerFileStream.close();

            bool isValid = contents.contains("version") &&
                           strcmp(std::string(contents["version"]).c_str(), (char*)gBuildVersion) == 0;

            if (!isValid) {
                SohGui::RegisterPopup(
                    "Old Spoiler Version",
                    "The spoiler file located at\n" + std::string(spoilerFileName) +
                        "\nwas made by a version that doesn't match the currently running version.\n" +
                        "Loading for this file has been cancelled.");
                CVarClear(CVAR_GENERAL("SpoilerLog"));
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }

            // Update cache
            existsCache[sanitizedFileName] = isValid;
            lastModifiedCache[sanitizedFileName] = currentLastModified;
            return isValid;
        }

        // File couldn't be opened
        existsCache[sanitizedFileName] = false;
        lastModifiedCache.erase(sanitizedFileName);
        return false;

    } catch (const std::filesystem::filesystem_error&) {
        // Handle filesystem errors by invalidating cache
        existsCache[sanitizedFileName] = false;
        lastModifiedCache.erase(sanitizedFileName);
        return false;
    }
}
#ifdef _MSC_VER
#pragma optimize("", on)
#else
#pragma GCC pop_options
#endif

// Reference soh/src/overlays/actors/ovl_En_GirlA/z_en_girla.h
std::unordered_map<RandomizerGet, EnGirlAShopItem> randomizerGetToEnGirlShopItem = {
    { RG_BUY_DEKU_NUTS_5, SI_DEKU_NUTS_5 },
    { RG_BUY_ARROWS_30, SI_ARROWS_30 },
    { RG_BUY_ARROWS_50, SI_ARROWS_50 },
    { RG_BUY_BOMBS_525, SI_BOMBS_5_R25 },
    { RG_BUY_DEKU_NUTS_10, SI_DEKU_NUTS_10 },
    { RG_BUY_DEKU_STICK_1, SI_DEKU_STICK },
    { RG_BUY_BOMBS_10, SI_BOMBS_10 },
    { RG_BUY_FISH, SI_FISH },
    { RG_BUY_RED_POTION_30, SI_RED_POTION_R30 },
    { RG_BUY_GREEN_POTION, SI_GREEN_POTION },
    { RG_BUY_BLUE_POTION, SI_BLUE_POTION },
    { RG_BUY_HYLIAN_SHIELD, SI_HYLIAN_SHIELD },
    { RG_BUY_DEKU_SHIELD, SI_DEKU_SHIELD },
    { RG_BUY_GORON_TUNIC, SI_GORON_TUNIC },
    { RG_BUY_ZORA_TUNIC, SI_ZORA_TUNIC },
    { RG_BUY_HEART, SI_RECOVERY_HEART },
    { RG_BUY_BOMBCHUS_10, SI_BOMBCHU_10_1 },
    { RG_BUY_BOMBCHUS_20, SI_BOMBCHU_20_1 },
    { RG_BUY_DEKU_SEEDS_30, SI_DEKU_SEEDS_30 },
    { RG_BUY_BLUE_FIRE, SI_BLUE_FIRE },
    { RG_BUY_BOTTLE_BUG, SI_BUGS },
    { RG_BUY_POE, SI_POE },
    { RG_BUY_FAIRYS_SPIRIT, SI_FAIRY },
    { RG_BUY_ARROWS_10, SI_ARROWS_10 },
    { RG_BUY_BOMBS_20, SI_BOMBS_20 },
    { RG_BUY_BOMBS_30, SI_BOMBS_30 },
    { RG_BUY_BOMBS_535, SI_BOMBS_5_R35 },
    { RG_BUY_RED_POTION_40, SI_RED_POTION_R40 },
    { RG_BUY_RED_POTION_50, SI_RED_POTION_R50 },
};

std::map<s32, TrialKey> trialFlagToTrialKey = {
    { EVENTCHKINF_COMPLETED_LIGHT_TRIAL, TK_LIGHT_TRIAL },   { EVENTCHKINF_COMPLETED_FOREST_TRIAL, TK_FOREST_TRIAL },
    { EVENTCHKINF_COMPLETED_FIRE_TRIAL, TK_FIRE_TRIAL },     { EVENTCHKINF_COMPLETED_WATER_TRIAL, TK_WATER_TRIAL },
    { EVENTCHKINF_COMPLETED_SPIRIT_TRIAL, TK_SPIRIT_TRIAL }, { EVENTCHKINF_COMPLETED_SHADOW_TRIAL, TK_SHADOW_TRIAL },
};

bool Randomizer::IsTrialRequired(s32 trialFlag) {
    return Rando::Context::GetInstance()->GetTrial(trialFlagToTrialKey[trialFlag])->IsRequired();
}

GetItemEntry Randomizer::GetItemFromActor(s16 actorId, s16 sceneNum, s16 actorParams, GetItemID ogItemId,
                                          bool checkObtainability) {
    return Rando::Context::GetInstance()->GetFinalGIEntry(GetCheckFromActor(actorId, sceneNum, actorParams),
                                                          checkObtainability, ogItemId);
}

ItemObtainability Randomizer::GetItemObtainabilityFromRandomizerCheck(RandomizerCheck randomizerCheck) {
    return GetItemObtainabilityFromRandomizerGet(
        Rando::Context::GetInstance()->GetItemLocation(randomizerCheck)->GetPlacedRandomizerGet());
}

ItemObtainability Randomizer::GetItemObtainabilityFromRandomizerGet(RandomizerGet randoGet) {
    // progressive open chest has a second copy that unlocks large chests
    if (randoGet == RG_OPEN_CHEST && GetRandoSettingValue(RSK_SHUFFLE_OPEN_CHEST) == RO_OPEN_CHEST_PROGRESSIVE) {
        return Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_LARGE_CHEST) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
    }

    if (Rando::StaticData::RandoGetToRandInf.find(randoGet) != Rando::StaticData::RandoGetToRandInf.end()) {
        return Flags_GetRandomizerInf((RandomizerInf)Rando::StaticData::RandoGetToRandInf.find(randoGet)->second)
                   ? CANT_OBTAIN_ALREADY_HAVE
                   : CAN_OBTAIN;
    }

    // This is needed since Plentiful item pool also adds a third progressive wallet
    // but we should not get Tycoon's Wallet from it if it is off.
    bool tycoonWallet = GetRandoSettingValue(RSK_INCLUDE_TYCOON_WALLET);

    // Same thing with the infinite upgrades, if we're not shuffling them
    // and we're using the Plentiful item pool, we should prevent the infinite
    // upgrades from being gotten
    u8 infiniteUpgrades = GetRandoSettingValue(RSK_INFINITE_UPGRADES);

    u8 numWallets = 2 + (u8)tycoonWallet + (infiniteUpgrades != RO_INF_UPGRADES_OFF ? 1 : 0);

    switch (randoGet) {
        case RG_NONE:
        case RG_HINT:
        case RG_MAX:
        case RG_SOLD_OUT:
            return CANT_OBTAIN_MISC;

        // Equipment
        case RG_KOKIRI_SWORD:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_MASTER_SWORD:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_BIGGORON_SWORD:
            return !gSaveContext.bgsFlag ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_DEKU_SHIELD:
        case RG_BUY_DEKU_SHIELD:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_HYLIAN_SHIELD:
        case RG_BUY_HYLIAN_SHIELD:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN) ? CAN_OBTAIN
                                                                                  : CANT_OBTAIN_ALREADY_HAVE;
        case RG_MIRROR_SHIELD:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR) ? CAN_OBTAIN
                                                                                  : CANT_OBTAIN_ALREADY_HAVE;
        case RG_GORON_TUNIC:
        case RG_BUY_GORON_TUNIC:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_TUNIC, EQUIP_INV_TUNIC_GORON) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_ZORA_TUNIC:
        case RG_BUY_ZORA_TUNIC:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_TUNIC, EQUIP_INV_TUNIC_ZORA) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_IRON_BOOTS:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_BOOTS, EQUIP_INV_BOOTS_IRON) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_HOVER_BOOTS:
            return !CHECK_OWNED_EQUIP(EQUIP_TYPE_BOOTS, EQUIP_INV_BOOTS_HOVER) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;

        // Inventory Items
        case RG_PROGRESSIVE_STICK_UPGRADE:
            return infiniteUpgrades != RO_INF_UPGRADES_OFF
                       ? (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_STICK_UPGRADE) ? CANT_OBTAIN_ALREADY_HAVE
                                                                                      : CAN_OBTAIN)
                       : (CUR_UPG_VALUE(UPG_STICKS) < 3 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE);
        case RG_DEKU_STICK_1:
        case RG_BUY_DEKU_STICK_1:
            return CUR_UPG_VALUE(UPG_STICKS) ||
                           !OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_DEKU_STICK_BAG).Get()
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_NEED_UPGRADE;
        case RG_PROGRESSIVE_NUT_UPGRADE:
            return infiniteUpgrades != RO_INF_UPGRADES_OFF
                       ? (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_NUT_UPGRADE) ? CANT_OBTAIN_ALREADY_HAVE
                                                                                    : CAN_OBTAIN)
                       : (CUR_UPG_VALUE(UPG_NUTS) < 3 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE);
        case RG_DEKU_NUTS_5:
        case RG_DEKU_NUTS_10:
        case RG_BUY_DEKU_NUTS_5:
        case RG_BUY_DEKU_NUTS_10:
            return CUR_UPG_VALUE(UPG_NUTS) ||
                           !OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_DEKU_NUT_BAG).Get()
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_NEED_UPGRADE;
        case RG_PROGRESSIVE_BOMB_BAG:
            return infiniteUpgrades != RO_INF_UPGRADES_OFF
                       ? (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BOMB_BAG) ? CANT_OBTAIN_ALREADY_HAVE
                                                                                 : CAN_OBTAIN)
                       : (CUR_UPG_VALUE(UPG_BOMB_BAG) < 3 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE);
        case RG_BOMBS_5:
        case RG_BOMBS_10:
        case RG_BOMBS_20:
        case RG_BUY_BOMBS_525:
        case RG_BUY_BOMBS_535:
        case RG_BUY_BOMBS_10:
        case RG_BUY_BOMBS_20:
        case RG_BUY_BOMBS_30:
            return CUR_UPG_VALUE(UPG_BOMB_BAG) ? CAN_OBTAIN : CANT_OBTAIN_NEED_UPGRADE;
        case RG_PROGRESSIVE_BOW:
            return infiniteUpgrades != RO_INF_UPGRADES_OFF
                       ? (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_QUIVER) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN)
                       : (CUR_UPG_VALUE(UPG_QUIVER) < 3 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE);
        case RG_ARROWS_5:
        case RG_ARROWS_10:
        case RG_ARROWS_30:
        case RG_BUY_ARROWS_10:
        case RG_BUY_ARROWS_30:
        case RG_BUY_ARROWS_50:
            return CUR_UPG_VALUE(UPG_QUIVER) ? CAN_OBTAIN : CANT_OBTAIN_NEED_UPGRADE;
        case RG_PROGRESSIVE_SLINGSHOT:
            return infiniteUpgrades != RO_INF_UPGRADES_OFF
                       ? (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BULLET_BAG) ? CANT_OBTAIN_ALREADY_HAVE
                                                                                   : CAN_OBTAIN)
                       : (CUR_UPG_VALUE(UPG_BULLET_BAG) < 3 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE);
        case RG_DEKU_SEEDS_30:
        case RG_BUY_DEKU_SEEDS_30:
            return CUR_UPG_VALUE(UPG_BULLET_BAG) ? CAN_OBTAIN : CANT_OBTAIN_NEED_UPGRADE;
        case RG_PROGRESSIVE_OCARINA:
            switch (INV_CONTENT(ITEM_OCARINA_FAIRY)) {
                case ITEM_NONE:
                case ITEM_OCARINA_FAIRY:
                    return CAN_OBTAIN;
                case ITEM_OCARINA_TIME:
                default:
                    return CANT_OBTAIN_ALREADY_HAVE;
            }
        case RG_BOMBCHU_5:
        case RG_BOMBCHU_10:
        case RG_BOMBCHU_20:
        case RG_BUY_BOMBCHUS_10:
        case RG_BUY_BOMBCHUS_20:
            return OTRGlobals::Instance->gRandoContext->GetOption(RSK_BOMBCHU_BAG).Is(RO_BOMBCHU_BAG_NONE)
                       ? CAN_OBTAIN
                       : (INV_CONTENT(ITEM_BOMBCHU) != ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_NEED_UPGRADE);
        case RG_PROGRESSIVE_BOMBCHU_BAG: // RANDOTODO Do we want bombchu refills to exist separately from bombchu bags?
                                         // If so, this needs changing.
            switch (OTRGlobals::Instance->gRandoContext->GetOption(RSK_BOMBCHU_BAG).Get()) {
                case RO_BOMBCHU_BAG_NONE:
                    return CANT_OBTAIN_MISC;
                case RO_BOMBCHU_BAG_SINGLE:
                    return CAN_OBTAIN;
                case RO_BOMBCHU_BAG_PROGRESSIVE:
                    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_BOMBCHUS)) {
                        return CANT_OBTAIN_ALREADY_HAVE;
                    } else {
                        switch (gSaveContext.ship.quest.data.randomizer.bombchuUpgradeLevel) {
                            case 0:
                            case 1:
                                return CAN_OBTAIN;
                            case 2:
                                return infiniteUpgrades == RO_INF_UPGRADES_CONDENSED_PROGRESSIVE
                                           ? CANT_OBTAIN_ALREADY_HAVE
                                           : CAN_OBTAIN;
                            case 3:
                                return infiniteUpgrades == RO_INF_UPGRADES_PROGRESSIVE ? CAN_OBTAIN
                                                                                       : CANT_OBTAIN_ALREADY_HAVE;
                        }
                    }
            }
            assert(false);
            return CAN_OBTAIN;
        case RG_PROGRESSIVE_HOOKSHOT:
            switch (INV_CONTENT(ITEM_HOOKSHOT)) {
                case ITEM_NONE:
                case ITEM_HOOKSHOT:
                    return CAN_OBTAIN;
                case ITEM_LONGSHOT:
                    // NEI chain level 3: the Longshot still upgrades into the Ultrashot.
                    return Nei_UltrashotOwned() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
                default:
                    return CANT_OBTAIN_ALREADY_HAVE;
            }
        case RG_BOOMERANG:
            return INV_CONTENT(ITEM_BOOMERANG) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_LENS_OF_TRUTH:
            return INV_CONTENT(ITEM_LENS) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_MAGIC_BEAN:
        case RG_MAGIC_BEAN_PACK:
            return AMMO(ITEM_BEAN) < 10 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_MEGATON_HAMMER:
            return INV_CONTENT(ITEM_HAMMER) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        // NEI progressive weapons — obtainable until the top upgrade level is reached.
        case RG_PROGRESSIVE_HAMMER:
            return WeaponUpgrade_HasHammerAxe() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_PROGRESSIVE_KOKIRI_SWORD:
            return WeaponUpgrade_HasGilded() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_PROGRESSIVE_MASTER_SWORD:
            return WeaponUpgrade_HasTrueMaster() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_PROGRESSIVE_BGS:
            return WeaponUpgrade_HasGreatFairy() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        // Per-level chain identities (explicit gives / give_all dedup).
        case RG_RAZOR_SWORD:
            return WeaponUpgrade_HasRazor() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_GILDED_SWORD:
            return WeaponUpgrade_HasGilded() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_TRUE_MASTER_SWORD:
            return WeaponUpgrade_HasTrueMaster() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_GREAT_FAIRY_SWORD:
            return WeaponUpgrade_HasGreatFairy() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_IRON_KNUCKLE_AXE:
            return WeaponUpgrade_HasHammerAxe() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_ULTRASHOT:
            return Nei_UltrashotOwned() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_QUARTZ_OF_MOTION:
            return Nei_Save()->quartzOwned ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_CLAWSHOT:
            return TwilightUpgrade_HasClawshot() ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_CANE_OF_SOMARIA: {
            // Obtainable until all 6 skills are lit (the walk queues 6 copies).
            for (u8 s = 0; s < 6; s++) {
                if (!Cane_HasSkill(s)) {
                    return CAN_OBTAIN;
                }
            }
            return CANT_OBTAIN_ALREADY_HAVE;
        }
        case RG_CANE_PACCI_FLIP:
            return Cane_HasSkill(3) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_CANE_SOMARIA_BLOCK:
            return Cane_HasSkill(1) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_CANE_PACCI_STONE:
            return Cane_HasSkill(4) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_CANE_SOMARIA_PLATFORM:
            return Cane_HasSkill(2) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_CANE_PACCI_ULTRAHAND:
            return Cane_HasSkill(5) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
        case RG_FIRE_ARROWS:
            return INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_ICE_ARROWS:
            return INV_CONTENT(ITEM_ARROW_ICE) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_LIGHT_ARROWS:
            return INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_DINS_FIRE:
            return INV_CONTENT(ITEM_DINS_FIRE) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FARORES_WIND:
            return INV_CONTENT(ITEM_FARORES_WIND) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_NAYRUS_LOVE:
            if (!GetRandoSettingValue(RSK_ROCS_FEATHER)) {
                return INV_CONTENT(ITEM_NAYRUS_LOVE) == ITEM_NONE ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
            } else {
                return Flags_GetRandomizerInf(RAND_INF_OBTAINED_NAYRUS_LOVE) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;
            }
        case RG_ROCS_FEATHER:
            return Flags_GetRandomizerInf(RAND_INF_OBTAINED_ROCS_FEATHER) ? CANT_OBTAIN_ALREADY_HAVE : CAN_OBTAIN;

        // Bottles
        case RG_EMPTY_BOTTLE:
        case RG_BOTTLE_WITH_MILK:
        case RG_BOTTLE_WITH_RED_POTION:
        case RG_BOTTLE_WITH_GREEN_POTION:
        case RG_BOTTLE_WITH_BLUE_POTION:
        case RG_BOTTLE_WITH_FAIRY:
        case RG_BOTTLE_WITH_FISH:
        case RG_BOTTLE_WITH_BLUE_FIRE:
        case RG_BOTTLE_WITH_BUGS:
        case RG_BOTTLE_WITH_POE:
        case RG_RUTOS_LETTER:
        case RG_BOTTLE_WITH_BIG_POE:
        case RG_BOTTLE_WITH_MAGIC_MUSHROOM:
        case RG_MM_BOTTLE_GOLD_DUST: // final cross items — fills a bottle slot like the row above
            return Inventory_HasEmptyBottleSlot() ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;

        // Bottle Refills
        case RG_MILK:
        case RG_FISH:
        case RG_RED_POTION_REFILL:
        case RG_GREEN_POTION_REFILL:
        case RG_BLUE_POTION_REFILL:
        case RG_BUY_FISH:
        case RG_BUY_RED_POTION_30:
        case RG_BUY_GREEN_POTION:
        case RG_BUY_BLUE_POTION:
        case RG_BUY_BLUE_FIRE:
        case RG_BUY_BOTTLE_BUG:
        case RG_BUY_POE:
        case RG_BUY_FAIRYS_SPIRIT:
        case RG_BUY_RED_POTION_40:
        case RG_BUY_RED_POTION_50:
            return Inventory_HasEmptyBottle() ? CAN_OBTAIN : CANT_OBTAIN_NEED_EMPTY_BOTTLE;

        // Trade Items
        // case RG_PROGRESSIVE_GORONSWORD:
        // case RG_GIANTS_KNIFE:

        // Misc Items
        case RG_POCKET_EGG:
            return Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_POCKET_EGG) ||
                           Flags_GetRandomizerInf(RAND_INF_ADULT_TRADES_HAS_POCKET_CUCCO)
                       ? CANT_OBTAIN_ALREADY_HAVE
                       : CAN_OBTAIN;
        case RG_STONE_OF_AGONY:
            // 2-level progressive: the stone, then the Quartz of Motion. Only
            // once both are in do further copies become dead weight.
            return !Nei_Save()->quartzOwned ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_GERUDO_MEMBERSHIP_CARD:
            return !CHECK_QUEST_ITEM(QUEST_GERUDO_CARD) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_DOUBLE_DEFENSE:
            return !gSaveContext.isDoubleDefenseAcquired ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_GOLD_SKULLTULA_TOKEN:
            return gSaveContext.inventory.gsTokens < 100 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_PROGRESSIVE_STRENGTH:
            return CUR_UPG_VALUE(UPG_STRENGTH) < 3 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_PROGRESSIVE_WALLET:
            return CUR_UPG_VALUE(UPG_WALLET) < numWallets ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_PROGRESSIVE_SCALE:
            return CUR_UPG_VALUE(UPG_SCALE) < 2 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_PROGRESSIVE_MAGIC_METER:
        case RG_MAGIC_SINGLE:
        case RG_MAGIC_DOUBLE:
            return infiniteUpgrades != RO_INF_UPGRADES_OFF
                       ? (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER) ? CANT_OBTAIN_ALREADY_HAVE
                                                                                    : CAN_OBTAIN)
                       : (gSaveContext.magicLevel < 2 ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE);
        case RG_FISHING_POLE:
            return !Flags_GetRandomizerInf(RAND_INF_FISHING_POLE_FOUND) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_PROGRESSIVE_ROCS:
            switch (ExtInv_GetSlotItem(SLOT_ROCS)) { // Skijer's NEI
                case ITEM_NONE:
                case ITEM_ROCS_FEATHER_SKIJER:
                    return CAN_OBTAIN;
                case ITEM_ROCS_CAPE:
                default:
                    return CANT_OBTAIN_ALREADY_HAVE;
            }

        // Songs
        case RG_ZELDAS_LULLABY:
            return !CHECK_QUEST_ITEM(QUEST_SONG_LULLABY) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_EPONAS_SONG:
            return !CHECK_QUEST_ITEM(QUEST_SONG_EPONA) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SARIAS_SONG:
            return !CHECK_QUEST_ITEM(QUEST_SONG_SARIA) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SUNS_SONG:
            return !CHECK_QUEST_ITEM(QUEST_SONG_SUN) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SONG_OF_TIME:
            return !CHECK_QUEST_ITEM(QUEST_SONG_TIME) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SONG_OF_STORMS:
            return !CHECK_QUEST_ITEM(QUEST_SONG_STORMS) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_MINUET_OF_FOREST:
            return !CHECK_QUEST_ITEM(QUEST_SONG_MINUET) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_BOLERO_OF_FIRE:
            return !CHECK_QUEST_ITEM(QUEST_SONG_BOLERO) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SERENADE_OF_WATER:
            return !CHECK_QUEST_ITEM(QUEST_SONG_SERENADE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_REQUIEM_OF_SPIRIT:
            return !CHECK_QUEST_ITEM(QUEST_SONG_REQUIEM) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_NOCTURNE_OF_SHADOW:
            return !CHECK_QUEST_ITEM(QUEST_SONG_NOCTURNE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_PRELUDE_OF_LIGHT:
            return !CHECK_QUEST_ITEM(QUEST_SONG_PRELUDE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;

        // Dungeon Items
        case RG_DEKU_TREE_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_DEKU_TREE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_DODONGOS_CAVERN_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_DODONGOS_CAVERN) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_JABU_JABUS_BELLY_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_JABU_JABU) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FOREST_TEMPLE_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_FOREST_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FIRE_TEMPLE_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_FIRE_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_WATER_TEMPLE_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_WATER_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SPIRIT_TEMPLE_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_SPIRIT_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SHADOW_TEMPLE_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_SHADOW_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_BOTTOM_OF_THE_WELL_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_BOTTOM_OF_THE_WELL) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_ICE_CAVERN_MAP:
            return !CHECK_DUNGEON_ITEM(DUNGEON_MAP, SCENE_ICE_CAVERN) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_DEKU_TREE_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_DEKU_TREE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_DODONGOS_CAVERN_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_DODONGOS_CAVERN) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_JABU_JABUS_BELLY_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_JABU_JABU) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FOREST_TEMPLE_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_FOREST_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FIRE_TEMPLE_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_FIRE_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_WATER_TEMPLE_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_WATER_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SPIRIT_TEMPLE_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_SPIRIT_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SHADOW_TEMPLE_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_SHADOW_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_BOTTOM_OF_THE_WELL_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_BOTTOM_OF_THE_WELL) ? CAN_OBTAIN
                                                                                  : CANT_OBTAIN_ALREADY_HAVE;
        case RG_ICE_CAVERN_COMPASS:
            return !CHECK_DUNGEON_ITEM(DUNGEON_COMPASS, SCENE_ICE_CAVERN) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FOREST_TEMPLE_BOSS_KEY:
            return !CHECK_DUNGEON_ITEM(DUNGEON_KEY_BOSS, SCENE_FOREST_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FIRE_TEMPLE_BOSS_KEY:
            return !CHECK_DUNGEON_ITEM(DUNGEON_KEY_BOSS, SCENE_FIRE_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_WATER_TEMPLE_BOSS_KEY:
            return !CHECK_DUNGEON_ITEM(DUNGEON_KEY_BOSS, SCENE_WATER_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SPIRIT_TEMPLE_BOSS_KEY:
            return !CHECK_DUNGEON_ITEM(DUNGEON_KEY_BOSS, SCENE_SPIRIT_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SHADOW_TEMPLE_BOSS_KEY:
            return !CHECK_DUNGEON_ITEM(DUNGEON_KEY_BOSS, SCENE_SHADOW_TEMPLE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_GANONS_CASTLE_BOSS_KEY:
            return !CHECK_DUNGEON_ITEM(DUNGEON_KEY_BOSS, SCENE_GANONS_TOWER) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FOREST_TEMPLE_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::FOREST_TEMPLE)
                               ->GetTotalSmallKeys(&gSaveContext) < FOREST_TEMPLE_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FIRE_TEMPLE_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::FIRE_TEMPLE)
                               ->GetTotalSmallKeys(&gSaveContext) < FIRE_TEMPLE_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_WATER_TEMPLE_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::WATER_TEMPLE)
                               ->GetTotalSmallKeys(&gSaveContext) < WATER_TEMPLE_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SPIRIT_TEMPLE_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::SPIRIT_TEMPLE)
                               ->GetTotalSmallKeys(&gSaveContext) < SPIRIT_TEMPLE_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SHADOW_TEMPLE_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::SHADOW_TEMPLE)
                               ->GetTotalSmallKeys(&gSaveContext) < SHADOW_TEMPLE_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_BOTTOM_OF_THE_WELL_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::BOTTOM_OF_THE_WELL)
                               ->GetTotalSmallKeys(&gSaveContext) < BOTTOM_OF_THE_WELL_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_GERUDO_TRAINING_GROUND_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::GERUDO_TRAINING_GROUND)
                               ->GetTotalSmallKeys(&gSaveContext) < GERUDO_TRAINING_GROUND_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_GERUDO_FORTRESS_SMALL_KEY: {
            std::vector<uint8_t> DoorFlags = THIEVES_HIDEOUT_DOOR_FLAGS;
            return Rando::FindTotalSmallKeys(&gSaveContext, SCENE_THIEVES_HIDEOUT, &DoorFlags) <
                           GERUDO_FORTRESS_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        }
        case RG_GANONS_CASTLE_SMALL_KEY:
            return OTRGlobals::Instance->gRandoContext->GetDungeon(Rando::GANONS_CASTLE)
                               ->GetTotalSmallKeys(&gSaveContext) < GANONS_CASTLE_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;
        case RG_TREASURE_GAME_SMALL_KEY:
            // I assume this cannot be easily manipulated?
            return gSaveContext.inventory.dungeonKeys[SCENE_TREASURE_BOX_SHOP] < TREASURE_GAME_SMALL_KEY_MAX
                       ? CAN_OBTAIN
                       : CANT_OBTAIN_ALREADY_HAVE;

        // Dungeon Rewards
        case RG_KOKIRI_EMERALD:
            return !CHECK_QUEST_ITEM(QUEST_KOKIRI_EMERALD) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_GORON_RUBY:
            return !CHECK_QUEST_ITEM(QUEST_GORON_RUBY) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_ZORA_SAPPHIRE:
            return !CHECK_QUEST_ITEM(QUEST_ZORA_SAPPHIRE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FOREST_MEDALLION:
            return !CHECK_QUEST_ITEM(QUEST_MEDALLION_FOREST) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_FIRE_MEDALLION:
            return !CHECK_QUEST_ITEM(QUEST_MEDALLION_FIRE) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_WATER_MEDALLION:
            return !CHECK_QUEST_ITEM(QUEST_MEDALLION_WATER) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SPIRIT_MEDALLION:
            return !CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_SHADOW_MEDALLION:
            return !CHECK_QUEST_ITEM(QUEST_MEDALLION_SHADOW) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;
        case RG_LIGHT_MEDALLION:
            return !CHECK_QUEST_ITEM(QUEST_MEDALLION_LIGHT) ? CAN_OBTAIN : CANT_OBTAIN_ALREADY_HAVE;

        case RG_RECOVERY_HEART:
        case RG_GREEN_RUPEE:
        case RG_GREG_RUPEE:
        case RG_BLUE_RUPEE:
        case RG_RED_RUPEE:
        case RG_PURPLE_RUPEE:
        case RG_HUGE_RUPEE:
        case RG_PIECE_OF_HEART:
        case RG_HEART_CONTAINER:
        case RG_ICE_TRAP:
        case RG_TREASURE_GAME_HEART:
        case RG_TREASURE_GAME_GREEN_RUPEE:
        case RG_BUY_HEART:
        case RG_TRIFORCE_PIECE:
        case RG_TRIFORCE:
        default:
            return CAN_OBTAIN;
    }
}

Rando::Location* Randomizer::GetCheckObjectFromActor(s16 actorId, s16 sceneNum, s32 actorParams = 0x00) {
    RandomizerCheck specialRc = RC_UNKNOWN_CHECK;
    // TODO: Migrate these special cases into table, or at least document why they are special
    switch (sceneNum) {
        case SCENE_TREASURE_BOX_SHOP: {
            if ((actorId == ACTOR_EN_BOX && actorParams == 20170) ||
                (actorId == ACTOR_ITEM_ETCETERA && actorParams == 2572)) {
                specialRc = RC_MARKET_TREASURE_CHEST_GAME_REWARD;
            }

            // todo: handle the itemetc part of this so drawing works when we implement shuffle
            if (actorId == ACTOR_EN_BOX) {
                bool isAKey = (actorParams & 0x60) == 0x20;
                if ((actorParams & 0xF) < 2) {
                    specialRc = isAKey ? RC_MARKET_TREASURE_CHEST_GAME_KEY_1 : RC_MARKET_TREASURE_CHEST_GAME_ITEM_1;
                } else if ((actorParams & 0xF) < 4) {
                    specialRc = isAKey ? RC_MARKET_TREASURE_CHEST_GAME_KEY_2 : RC_MARKET_TREASURE_CHEST_GAME_ITEM_2;
                } else if ((actorParams & 0xF) < 6) {
                    specialRc = isAKey ? RC_MARKET_TREASURE_CHEST_GAME_KEY_3 : RC_MARKET_TREASURE_CHEST_GAME_ITEM_3;
                } else if ((actorParams & 0xF) < 8) {
                    specialRc = isAKey ? RC_MARKET_TREASURE_CHEST_GAME_KEY_4 : RC_MARKET_TREASURE_CHEST_GAME_ITEM_4;
                } else if ((actorParams & 0xF) < 10) {
                    specialRc = isAKey ? RC_MARKET_TREASURE_CHEST_GAME_KEY_5 : RC_MARKET_TREASURE_CHEST_GAME_ITEM_5;
                }
            }
            break;
        }
        case SCENE_SACRED_FOREST_MEADOW:
            if (actorId == ACTOR_EN_SA) {
                specialRc = RC_SONG_FROM_SARIA;
            }
            break;
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS:
            switch (actorParams) {
                case 14342:
                    specialRc = RC_TOT_LEFTMOST_GOSSIP_STONE;
                    break;
                case 14599:
                    specialRc = RC_TOT_LEFT_CENTER_GOSSIP_STONE;
                    break;
                case 14862:
                    specialRc = RC_TOT_RIGHT_CENTER_GOSSIP_STONE;
                    break;
                case 15120:
                    specialRc = RC_TOT_RIGHTMOST_GOSSIP_STONE;
                    break;
            }
            break;
        case SCENE_HOUSE_OF_SKULLTULA:
            if (actorId == ACTOR_EN_SSH) {
                switch (actorParams) { // actor params are used to differentiate between textboxes
                    case 1:
                        specialRc = RC_KAK_10_GOLD_SKULLTULA_REWARD;
                        break;
                    case 2:
                        specialRc = RC_KAK_20_GOLD_SKULLTULA_REWARD;
                        break;
                    case 3:
                        specialRc = RC_KAK_30_GOLD_SKULLTULA_REWARD;
                        break;
                    case 4:
                        specialRc = RC_KAK_40_GOLD_SKULLTULA_REWARD;
                        break;
                    case 5:
                        specialRc = RC_KAK_50_GOLD_SKULLTULA_REWARD;
                        break;
                }
            }
            break;
        case SCENE_KAKARIKO_VILLAGE:
            switch (actorId) {
                case ACTOR_EN_NIW_LADY:
                    if (LINK_IS_ADULT) {
                        specialRc = RC_KAK_ANJU_AS_ADULT;
                    } else {
                        specialRc = RC_KAK_ANJU_AS_CHILD;
                    }
            }
            break;
        case SCENE_LAKE_HYLIA:
            switch (actorId) {
                case ACTOR_ITEM_ETCETERA:
                    if (LINK_IS_ADULT) {
                        specialRc = RC_LH_SUN;
                    } else {
                        specialRc = RC_LH_UNDERWATER_ITEM;
                    }
            }
            break;
        case SCENE_ZORAS_FOUNTAIN:
            switch (actorParams) {
                case 15362:
                case 14594:
                    specialRc = RC_ZF_JABU_GOSSIP_STONE;
                    break;
                case 14849:
                case 14337:
                    specialRc = RC_ZF_FAIRY_GOSSIP_STONE;
                    break;
            }
            break;
        case SCENE_GERUDOS_FORTRESS:
            // GF chest as child has different params and gives odd mushroom
            // set it to the GF chest check for both ages
            if (actorId == ACTOR_EN_BOX) {
                specialRc = RC_GF_CHEST;
            }
            break;
        case SCENE_DODONGOS_CAVERN:
            // special case for MQ DC Gossip Stone
            if (actorId == ACTOR_EN_GS && actorParams == 15892 && ResourceMgr_IsGameMasterQuest()) {
                specialRc = RC_DODONGOS_CAVERN_GOSSIP_STONE;
            }
            break;
        case SCENE_SHOOTING_GALLERY:
            // special case for shooting gallery sign
            if (actorId == ACTOR_EN_KANBAN) {
                if (LINK_IS_ADULT) {
                    specialRc = RC_KAK_SHOOTING_GALLERY_RECTANGLE_SIGN;
                } else {
                    specialRc = RC_MK_SHOOTING_GALLERY_RECTANGLE_SIGN;
                }
            }
            break;
    }

    if (specialRc != RC_UNKNOWN_CHECK) {
        return Rando::StaticData::GetLocation(specialRc);
    }

    auto range = Rando::StaticData::CheckFromActorMultimap.equal_range(std::make_tuple(actorId, sceneNum, actorParams));

    for (auto it = range.first; it != range.second; ++it) {
        if (Rando::StaticData::GetLocation(it->second)->GetQuest() == RCQUEST_BOTH ||
            (Rando::StaticData::GetLocation(it->second)->GetQuest() == RCQUEST_VANILLA &&
             !ResourceMgr_IsGameMasterQuest()) ||
            (Rando::StaticData::GetLocation(it->second)->GetQuest() == RCQUEST_MQ && ResourceMgr_IsGameMasterQuest())) {
            return Rando::StaticData::GetLocation(it->second);
        }
    }

    return Rando::StaticData::GetLocation(RC_UNKNOWN_CHECK);
}

// RANDOTODO: Move all Shopsanity stuff to a ShuffleShops.cpp
ShopItemIdentity Randomizer::IdentifyShopItem(s32 sceneNum, u8 slotIndex) {
    ShopItemIdentity shopItemIdentity;

    shopItemIdentity.identity.randomizerInf = RAND_INF_MAX;
    shopItemIdentity.identity.randomizerCheck = RC_UNKNOWN_CHECK;
    shopItemIdentity.ogItemId = GI_NONE;
    shopItemIdentity.itemPrice = -1;
    shopItemIdentity.enGirlAShopItem = 0x32;

    if (slotIndex == 0) {
        return shopItemIdentity;
    }

    Rando::Location* location = GetCheckObjectFromActor(
        ACTOR_EN_GIRLA,
        // Bazaar (SHOP1) scene is reused, so if entering from Kak use debug scene to identify
        (sceneNum == SCENE_BAZAAR && gSaveContext.entranceIndex == ENTR_BAZAAR_0) ? SCENE_TEST01 : sceneNum,
        slotIndex - 1);

    if (location->GetRandomizerCheck() != RC_UNKNOWN_CHECK) {
        shopItemIdentity.identity.randomizerInf = rcToRandomizerInf[location->GetRandomizerCheck()];
        shopItemIdentity.identity.randomizerCheck = location->GetRandomizerCheck();
        shopItemIdentity.ogItemId = (GetItemID)Rando::StaticData::RetrieveItem(location->GetVanillaItem()).GetItemID();

        RandomizerGet randoGet = Rando::Context::GetInstance()
                                     ->GetItemLocation(shopItemIdentity.identity.randomizerCheck)
                                     ->GetPlacedRandomizerGet();
        if (randomizerGetToEnGirlShopItem.find(randoGet) != randomizerGetToEnGirlShopItem.end()) {
            shopItemIdentity.enGirlAShopItem = randomizerGetToEnGirlShopItem[randoGet];
        }

        shopItemIdentity.itemPrice =
            OTRGlobals::Instance->gRandoContext->GetItemLocation(shopItemIdentity.identity.randomizerCheck)->GetPrice();
    }

    return shopItemIdentity;
}

u8 Randomizer::GetRandoSettingValue(RandomizerSettingKey randoSettingKey) {
    return Rando::Context::GetInstance()->GetOption(randoSettingKey).Get();
}

u8 Randomizer::GetTriforcePiecesRequired() {
    u8 required = 0;
    if (GetRandoSettingValue(RSK_RAINBOW_BRIDGE) == RO_BRIDGE_TRIFORCE_PIECES) {
        required = std::max(required, GetRandoSettingValue(RSK_RAINBOW_BRIDGE_TRIFORCE_COUNT));
    }
    if (GetRandoSettingValue(RSK_GANONS_BOSS_KEY) == RO_GANON_BOSS_KEY_TRIFORCE_PIECES) {
        required = std::max(required, GetRandoSettingValue(RSK_GBK_TRIFORCE_COUNT));
    }
    if (GetRandoSettingValue(RSK_GANONS_SOUL) == RO_GANONS_SOUL_TRIFORCE_PIECES) {
        required = std::max(required, GetRandoSettingValue(RSK_GANONS_SOUL_TRIFORCE_COUNT));
    }
    if (GetRandoSettingValue(RSK_WINCON) == RO_WINCON_TRIFORCE_PIECES) {
        required = std::max(required, GetRandoSettingValue(RSK_WINCON_TRIFORCE_COUNT));
    }
    return required;
}

GetItemEntry Randomizer::GetItemFromKnownCheck(RandomizerCheck randomizerCheck, GetItemID ogItemId,
                                               bool checkObtainability) {
    return Rando::Context::GetInstance()->GetFinalGIEntry(randomizerCheck, checkObtainability);
}

RandomizerCheck Randomizer::GetCheckFromActor(s16 actorId, s16 sceneNum, s16 actorParams) {
    return GetCheckObjectFromActor(actorId, sceneNum, actorParams)->GetRandomizerCheck();
}

RandomizerInf Randomizer::GetRandomizerInfFromCheck(RandomizerCheck rc) {
    auto rcIt = rcToRandomizerInf.find(rc);
    if (rcIt == rcToRandomizerInf.end())
        return RAND_INF_MAX;

    return rcIt->second;
}

RandomizerCheck Randomizer::GetCheckFromRandomizerInf(RandomizerInf randomizerInf) {
    for (auto const& [key, value] : rcToRandomizerInf) {
        if (value == randomizerInf)
            return key;
    }

    return RC_UNKNOWN_CHECK;
}

std::thread randoThread;

void GenerateRandomizerImgui(std::string seed = "") {
    CVarSetInteger(CVAR_GENERAL("RandoGenerating"), 1);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    auto ctx = Rando::Context::GetInstance();
    // RANDOTODO proper UI for selecting if a spoiler loaded should be used for settings
    Rando::Settings::GetInstance()->SetAllToContext();

    // todo: this efficiently when we build out cvar array support
    std::set<RandomizerCheck> excludedLocations;
    std::stringstream excludedLocationStringStream(CVarGetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), ""));
    std::string excludedLocationString;
    while (getline(excludedLocationStringStream, excludedLocationString, ',')) {
        excludedLocations.insert((RandomizerCheck)std::stoi(excludedLocationString));
    }

    // todo: better way to sort out linking tricks rather than name

    std::set<RandomizerTrick> enabledTricks;
    std::stringstream enabledTrickStringStream(CVarGetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), ""));
    std::string enabledTrickString;
    while (getline(enabledTrickStringStream, enabledTrickString, ',')) {
        if (Rando::StaticData::trickToEnum.contains(enabledTrickString)) {
            enabledTricks.insert(Rando::StaticData::trickToEnum[enabledTrickString]);
        }
    }

    // Update the visibilitiy before removing conflicting excludes (in case the locations tab wasn't viewed)
    RandomizerCheckObjects::UpdateImGuiVisibility();

    // Remove excludes for locations that are no longer allowed to be excluded
    for (auto& location : Rando::StaticData::GetLocationTable()) {
        auto elfound = excludedLocations.find(location.GetRandomizerCheck());
        if (!ctx->GetItemLocation(location.GetRandomizerCheck())->IsVisible() && elfound != excludedLocations.end()) {
            excludedLocations.erase(elfound);
        }
    }

    Rando::Context::GetInstance()->SetSeedGenerated(GenerateRandomizer(excludedLocations, enabledTricks, seed));
    CVarSetInteger(CVAR_GENERAL("RandoGenerating"), 0);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();

    generated = true;

    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnGenerationCompletion>();
}

bool GenerateRandomizer(std::string seed /*= ""*/) {
    if (generated) {
        generated = false;
        randoThread.join();
    }
    if (CVarGetInteger(CVAR_GENERAL("RandoGenerating"), 0) == 0) {
        randoThread = std::thread(&GenerateRandomizerImgui, seed);
        return true;
    }
    return false;
}

static bool locationsTabOpen = false;
static bool tricksTabOpen = false;

void JoinRandoGenerationThread() {
    if (generated) {
        generated = false;
        randoThread.join();
    }
}

class ExtendedVanillaTableInvalidItemIdException : public std::exception {
  private:
    s16 itemID;

  public:
    ExtendedVanillaTableInvalidItemIdException(s16 itemID) : itemID(itemID) {
    }
    std::string what() {
        return itemID + " is not a valid ItemID for the extendedVanillaGetItemTable. If you are adding a new"
                        "item, try adding it to randoGetItemTable instead.";
    }
};

static std::unordered_map<RandomizerGet, GameplayStatTimestamp> randomizerGetToStatsTimeStamp = {
    { RG_GOHMA_SOUL, TIMESTAMP_FOUND_GOHMA_SOUL },
    { RG_KING_DODONGO_SOUL, TIMESTAMP_FOUND_KING_DODONGO_SOUL },
    { RG_BARINADE_SOUL, TIMESTAMP_FOUND_BARINADE_SOUL },
    { RG_PHANTOM_GANON_SOUL, TIMESTAMP_FOUND_PHANTOM_GANON_SOUL },
    { RG_VOLVAGIA_SOUL, TIMESTAMP_FOUND_VOLVAGIA_SOUL },
    { RG_MORPHA_SOUL, TIMESTAMP_FOUND_MORPHA_SOUL },
    { RG_BONGO_BONGO_SOUL, TIMESTAMP_FOUND_BONGO_BONGO_SOUL },
    { RG_TWINROVA_SOUL, TIMESTAMP_FOUND_TWINROVA_SOUL },
    { RG_GANON_SOUL, TIMESTAMP_FOUND_GANON_SOUL },

    { RG_BRONZE_SCALE, TIMESTAMP_FOUND_BRONZE_SCALE },

    { RG_OCARINA_A_BUTTON, TIMESTAMP_FOUND_OCARINA_A_BUTTON },
    { RG_OCARINA_C_UP_BUTTON, TIMESTAMP_FOUND_OCARINA_C_UP_BUTTON },
    { RG_OCARINA_C_DOWN_BUTTON, TIMESTAMP_FOUND_OCARINA_C_DOWN_BUTTON },
    { RG_OCARINA_C_LEFT_BUTTON, TIMESTAMP_FOUND_OCARINA_C_LEFT_BUTTON },
    { RG_OCARINA_C_RIGHT_BUTTON, TIMESTAMP_FOUND_OCARINA_C_RIGHT_BUTTON },

    { RG_FISHING_POLE, TIMESTAMP_FOUND_FISHING_POLE },

    { RG_GUARD_HOUSE_KEY, TIMESTAMP_FOUND_GUARD_HOUSE_KEY },
    { RG_MARKET_BAZAAR_KEY, TIMESTAMP_FOUND_MARKET_BAZAAR_KEY },
    { RG_MARKET_POTION_SHOP_KEY, TIMESTAMP_FOUND_MARKET_POTION_SHOP_KEY },
    { RG_MASK_SHOP_KEY, TIMESTAMP_FOUND_MASK_SHOP_KEY },
    { RG_MARKET_SHOOTING_GALLERY_KEY, TIMESTAMP_FOUND_MARKET_SHOOTING_GALLERY_KEY },
    { RG_BOMBCHU_BOWLING_KEY, TIMESTAMP_FOUND_BOMBCHU_BOWLING_KEY },
    { RG_TREASURE_CHEST_GAME_BUILDING_KEY, TIMESTAMP_FOUND_TREASURE_CHEST_GAME_BUILDING_KEY },
    { RG_BOMBCHU_SHOP_KEY, TIMESTAMP_FOUND_BOMBCHU_SHOP_KEY },
    { RG_RICHARDS_HOUSE_KEY, TIMESTAMP_FOUND_RICHARDS_HOUSE_KEY },
    { RG_ALLEY_HOUSE_KEY, TIMESTAMP_FOUND_ALLEY_HOUSE_KEY },
    { RG_KAK_BAZAAR_KEY, TIMESTAMP_FOUND_KAK_BAZAAR_KEY },
    { RG_KAK_POTION_SHOP_KEY, TIMESTAMP_FOUND_KAK_POTION_SHOP_KEY },
    { RG_BOSS_HOUSE_KEY, TIMESTAMP_FOUND_BOSS_HOUSE_KEY },
    { RG_GRANNYS_POTION_SHOP_KEY, TIMESTAMP_FOUND_GRANNYS_POTION_SHOP_KEY },
    { RG_SKULLTULA_HOUSE_KEY, TIMESTAMP_FOUND_SKULLTULA_HOUSE_KEY },
    { RG_IMPAS_HOUSE_KEY, TIMESTAMP_FOUND_IMPAS_HOUSE_KEY },
    { RG_WINDMILL_KEY, TIMESTAMP_FOUND_WINDMILL_KEY },
    { RG_KAK_SHOOTING_GALLERY_KEY, TIMESTAMP_FOUND_KAK_SHOOTING_GALLERY_KEY },
    { RG_DAMPES_HUT_KEY, TIMESTAMP_FOUND_DAMPES_HUT_KEY },
    { RG_TALONS_HOUSE_KEY, TIMESTAMP_FOUND_TALONS_HOUSE_KEY },
    { RG_STABLES_KEY, TIMESTAMP_FOUND_STABLES_KEY },
    { RG_BACK_TOWER_KEY, TIMESTAMP_FOUND_BACK_TOWER_KEY },
    { RG_HYLIA_LAB_KEY, TIMESTAMP_FOUND_HYLIA_LAB_KEY },
    { RG_FISHING_HOLE_KEY, TIMESTAMP_FOUND_FISHING_HOLE_KEY },

    { RG_GREG_RUPEE, TIMESTAMP_FOUND_GREG },

    { RG_CHILD_WALLET, TIMESTAMP_FOUND_CHILD_WALLET },
    { RG_TYCOON_WALLET, TIMESTAMP_FOUND_TYCOON_WALLET },

    { RG_DEKU_STICK_BAG, TIMESTAMP_FOUND_DEKU_STICK_BAG },
    { RG_DEKU_NUT_BAG, TIMESTAMP_FOUND_DEKU_NUT_BAG },

    { RG_POWER_BRACELET, TIMESTAMP_FOUND_GRAB },
    { RG_CLIMB, TIMESTAMP_FOUND_CLIMB },
    { RG_CRAWL, TIMESTAMP_FOUND_CRAWL },
    { RG_OPEN_CHEST, TIMESTAMP_FOUND_OPEN_CHESTS },

    { RG_SPEAK_DEKU, TIMESTAMP_FOUND_SPEAK_DEKU },
    { RG_SPEAK_GERUDO, TIMESTAMP_FOUND_SPEAK_GERUDO },
    { RG_SPEAK_GORON, TIMESTAMP_FOUND_SPEAK_GORON },
    { RG_SPEAK_HYLIAN, TIMESTAMP_FOUND_SPEAK_HYLIAN },
    { RG_SPEAK_KOKIRI, TIMESTAMP_FOUND_SPEAK_KOKIRI },
    { RG_SPEAK_ZORA, TIMESTAMP_FOUND_SPEAK_ZORA },

    { RG_DEATH_MOUNTAIN_CRATER_BEAN_SOUL, TIMESTAMP_FOUND_DMC_BEAN_SOUL },
    { RG_DEATH_MOUNTAIN_TRAIL_BEAN_SOUL, TIMESTAMP_FOUND_DMT_BEAN_SOUL },
    { RG_DESERT_COLOSSUS_BEAN_SOUL, TIMESTAMP_FOUND_COLOSSUS_BEAN_SOUL },
    { RG_GERUDO_VALLEY_BEAN_SOUL, TIMESTAMP_FOUND_GV_BEAN_SOUL },
    { RG_GRAVEYARD_BEAN_SOUL, TIMESTAMP_FOUND_GY_BEAN_SOUL },
    { RG_KOKIRI_FOREST_BEAN_SOUL, TIMESTAMP_FOUND_KF_BEAN_SOUL },
    { RG_LAKE_HYLIA_BEAN_SOUL, TIMESTAMP_FOUND_LH_BEAN_SOUL },
    { RG_LOST_WOODS_BRIDGE_BEAN_SOUL, TIMESTAMP_FOUND_LW_BRIDGE_BEAN_SOUL },
    { RG_LOST_WOODS_BEAN_SOUL, TIMESTAMP_FOUND_LW_MEADOW_BEAN_SOUL },
    { RG_ZORAS_RIVER_BEAN_SOUL, TIMESTAMP_FOUND_ZR_BEAN_SOUL },

    { RG_SKELETON_KEY, TIMESTAMP_FOUND_SKELETON_KEY },

    { RG_ROCS_FEATHER, TIMESTAMP_FOUND_ROCS_FEATHER },
};

// Gameplay stat tracking: Update time the item was acquired
// (special cases for rando items)
void Randomizer_GameplayStats_SetTimestamp(uint16_t item) {
    u32 time = static_cast<u32>(GAMEPLAYSTAT_TOTAL_TIME);
    // Have items in Link's pocket shown as being obtained at 0.1 seconds
    if (time == 0) {
        time = 1;
    }

    int16_t timestampItem = -1;
    if (item == RG_GANONS_CASTLE_BOSS_KEY) {
        timestampItem = ITEM_KEY_BOSS;
    } else if (item == RG_MASTER_SWORD) {
        timestampItem = ITEM_SWORD_MASTER;
    } else if (item >= RG_EMPTY_BOTTLE && item <= RG_BOTTLE_WITH_BIG_POE) {
        timestampItem = ITEM_BOTTLE;
    } else if ((item >= RG_BOMBCHU_5 && item <= RG_BOMBCHU_20) || item == RG_PROGRESSIVE_BOMBCHU_BAG) {
        timestampItem = ITEM_BOMBCHU;
    } else if (item == RG_MAGIC_SINGLE) {
        timestampItem = ITEM_SINGLE_MAGIC;
    } else if (item == RG_DOUBLE_DEFENSE) {
        timestampItem = ITEM_DOUBLE_DEFENSE;
    } else if (item >= RG_KEATON_MASK && item <= RG_MASK_OF_TRUTH) {
        timestampItem = ITEM_MASK_KEATON + (item - RG_KEATON_MASK);
    } else if (item == RG_WEIRD_EGG) {
        timestampItem = ITEM_WEIRD_EGG;
    } else if (item == RG_ZELDAS_LETTER) {
        timestampItem = ITEM_LETTER_ZELDA;
    } else if (randomizerGetToStatsTimeStamp.contains((RandomizerGet)item)) {
        timestampItem = randomizerGetToStatsTimeStamp[(RandomizerGet)item];
    }

    if (timestampItem != -1 && gSaveContext.ship.stats.itemTimestamp[timestampItem] == 0) {
        gSaveContext.ship.stats.itemTimestamp[timestampItem] = time;
    }
}

extern "C" u8 Return_Item_Entry(GetItemEntry itemEntry, u8 returnItem);
// item_cane_of_somaria.c (Skijer's NEI Dual Cane) — lights one of the six skill bits and,
// on the first one obtained, also puts the cane into SLOT_CANE_OF_SOMARIA. Returns 1 when
// the skill was newly granted. CANE_SKILL_* order: 0 Statue, 1 Block, 2 Platform,
// 3 Flip, 4 Stone, 5 Ultrahand.
extern "C" u8 Cane_GiveSkill(u8 skill);

// The child trade slot can be displaced (e.g. chicken consumed waking Talon,
// letter shown to the guard), leaving an item there the player no longer owns.
static bool ChildTradeSlotOccupied() {
    u8 slotItem = INV_CONTENT(ITEM_TRADE_CHILD);
    if (slotItem < ITEM_WEIRD_EGG || slotItem > ITEM_MASK_TRUTH) {
        return false;
    }
    return Flags_GetRandomizerInf((RandomizerInf)(slotItem - ITEM_WEIRD_EGG + RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG));
}

extern "C" u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry) {
    if (giEntry.modIndex != MOD_RANDOMIZER) {
        LUSLOG_WARN(
            "Randomizer_Item_Give was called with a GetItemEntry with a mod index different from MOD_RANDOMIZER (%d)",
            giEntry.modIndex);
        assert(false);
        return -1;
    }

    RandomizerGet item = (RandomizerGet)giEntry.getItemId;

    // FleetShipCombo: record every FC cross item obtained in OoT into the fcId-indexed store so it
    // syncs to MM. Bump BOTH the synced count (comboObtainedFc) and the local applied count
    // (comboAppliedFc) together — this item is materializing here natively right now, so its
    // deficit stays 0 and ApplyFcRegistryToNatives will not re-grant it. This is the single
    // programmatic give choke; it is skipped while ApplyFcRegistryToNatives is itself granting the
    // FC deficit (its Randomizer_Item_Give calls re-enter here), which would otherwise double-count.
    if (!FleetSync_IsApplyingFc()) {
        // NOTE: progressive chains arrive here already resolved to their TIER (RG_MAGIC_SINGLE, wallets,
        // strength...) and deliberately do NOT fold back into the chain's FC row: those natives cross
        // through the shared-state sync (FleetSync ExtractShared/ApplyShared: inventory, upgrades,
        // magic flags), and counting them here as well would make the FC deficit grant a SECOND
        // tier on the other side. Skijer's NEI
        int fc = FcCombo_ItemForNative((int)item);
        if (fc != FCI_NO_ITEM && fc >= 0 && fc < FC_COMBO_OBTAINED_FC_SIZE) {
            NeiSaveData* nei = Nei_Save();
            nei->comboObtainedFc[fc]++;
            nei->comboAppliedFc[fc]++;
        }
        FleetShared_OnNativeObtained((int)item); // ComboShip: hand MM its half of a shared item
    }

    // Gameplay stats: Update the time the item was obtained
    Randomizer_GameplayStats_SetTimestamp(item);

    // open chest: not progressive gives both flags at once, progressive gives large only as the second copy
    if (item == RG_OPEN_CHEST &&
        (OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_SHUFFLE_OPEN_CHEST) != RO_OPEN_CHEST_PROGRESSIVE ||
         Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_CHEST))) {
        Flags_SetRandomizerInf(RAND_INF_CAN_OPEN_LARGE_CHEST);
    }

    // if it's an item that just sets a randomizerInf, set it
    if (Rando::StaticData::RandoGetToRandInf.find(item) != Rando::StaticData::RandoGetToRandInf.end()) {
        Flags_SetRandomizerInf((RandomizerInf)Rando::StaticData::RandoGetToRandInf.find(item)->second);
        if (item == RG_SKELETON_KEY) {
            Flags_SetRandomizerInf(RAND_INF_HAS_SKELETON_KEY);
            // This isn't technically necessary, because keys will no longer be consumed,
            // but for the player's sanity we display that they _have_ keys.
            gSaveContext.inventory.dungeonKeys[SCENE_FOREST_TEMPLE] = FOREST_TEMPLE_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_FIRE_TEMPLE] = FIRE_TEMPLE_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_WATER_TEMPLE] = WATER_TEMPLE_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_SPIRIT_TEMPLE] = SPIRIT_TEMPLE_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_SHADOW_TEMPLE] = SHADOW_TEMPLE_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_BOTTOM_OF_THE_WELL] = BOTTOM_OF_THE_WELL_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_GERUDO_TRAINING_GROUND] = GERUDO_TRAINING_GROUND_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_THIEVES_HIDEOUT] = GERUDO_FORTRESS_SMALL_KEY_MAX;
            gSaveContext.inventory.dungeonKeys[SCENE_INSIDE_GANONS_CASTLE] = GANONS_CASTLE_SMALL_KEY_MAX;
        } else if (item >= RG_KEATON_MASK && item <= RG_MASK_OF_TRUTH) {
            if (!ChildTradeSlotOccupied()) {
                INV_CONTENT(ITEM_TRADE_CHILD) = (int)ITEM_MASK_KEATON + (item - RG_KEATON_MASK);
            }
        } else if (item == RG_WEIRD_EGG) {
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG);
            if (!ChildTradeSlotOccupied()) {
                INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_WEIRD_EGG;
            }
        } else if (item == RG_ZELDAS_LETTER) {
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_LETTER_ZELDA);
            if (!ChildTradeSlotOccupied()) {
                INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_LETTER_ZELDA;
            }
        } else if (item == RG_CHILD_WALLET &&
                   OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_FULL_WALLETS)) {
            Rupees_ChangeBy(99);
        } else if (item == RG_GREG_RUPEE) {
            Rupees_ChangeBy(1);
        }

        return Return_Item_Entry(giEntry, RG_NONE);
    }

    // bottle items
    if (item >= RG_BOTTLE_WITH_RED_POTION && item <= RG_BOTTLE_WITH_BIG_POE) {
        ItemID bottleItem = ITEM_NONE;
        switch (item) {
            case RG_BOTTLE_WITH_RED_POTION:
                bottleItem = ITEM_POTION_RED;
                break;
            case RG_BOTTLE_WITH_GREEN_POTION:
                bottleItem = ITEM_POTION_GREEN;
                break;
            case RG_BOTTLE_WITH_BLUE_POTION:
                bottleItem = ITEM_POTION_BLUE;
                break;
            case RG_BOTTLE_WITH_FAIRY:
                bottleItem = ITEM_FAIRY;
                break;
            case RG_BOTTLE_WITH_FISH:
                bottleItem = ITEM_FISH;
                break;
            case RG_BOTTLE_WITH_BLUE_FIRE:
                bottleItem = ITEM_BLUE_FIRE;
                break;
            case RG_BOTTLE_WITH_BUGS:
                bottleItem = ITEM_BUG;
                break;
            case RG_BOTTLE_WITH_POE:
                bottleItem = ITEM_POE;
                break;
            case RG_BOTTLE_WITH_BIG_POE:
                bottleItem = ITEM_BIG_POE;
                break;
            default:
                break;
        }

        // Skijer's NEI — "Bottle with X" GRANTS a bottle, so it belongs in the 8-slot wheel. NEI owns
        // all four vanilla slots permanently, so the fallback loop below never matched under it and
        // every rando bottle was silently lost.
        if (Bottle_GiveBottle(bottleItem)) {
            return Return_Item_Entry(giEntry, RG_NONE);
        }

        for (u16 i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_NONE) {
                gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = bottleItem;
                return Return_Item_Entry(giEntry, RG_NONE);
            }
        }
    }

    // Magic Mushroom bottle (NEI custom - not part of the vanilla bottle
    // range, so handled separately).
    if (item == RG_BOTTLE_WITH_MAGIC_MUSHROOM) {
        if (Bottle_GiveBottle(ITEM_BOTTLE_WITH_MAGIC_MUSHROOM)) {
            return Return_Item_Entry(giEntry, RG_NONE);
        }
        for (u16 i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_NONE) {
                gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = ITEM_BOTTLE_WITH_MAGIC_MUSHROOM;
                return Return_Item_Entry(giEntry, RG_NONE);
            }
        }
    }

    // MM Bottle with Gold Dust (final cross items) — same custom-bottle grant as the Magic Mushroom
    // above: ITEM_GOLD_DUST (0xEC) into the first free bottle slot. mm_bottles_behavior.cpp maps
    // 0xEC -> MM_BOTTLE_GOLD_DUST, so the content behaves (and empties) like MM's gold dust.
    if (item == RG_MM_BOTTLE_GOLD_DUST) {
        if (Bottle_GiveBottle(ITEM_GOLD_DUST)) {
            return Return_Item_Entry(giEntry, RG_NONE);
        }
        for (u16 i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] == ITEM_NONE) {
                gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] = ITEM_GOLD_DUST;
                return Return_Item_Entry(giEntry, RG_NONE);
            }
        }
    }

    // dungeon items
    if ((item >= RG_FOREST_TEMPLE_SMALL_KEY && item <= RG_GANONS_CASTLE_SMALL_KEY) ||
        (item >= RG_FOREST_TEMPLE_KEY_RING && item <= RG_GANONS_CASTLE_KEY_RING) ||
        (item >= RG_FOREST_TEMPLE_BOSS_KEY && item <= RG_GANONS_CASTLE_BOSS_KEY) ||
        (item >= RG_DEKU_TREE_MAP && item <= RG_ICE_CAVERN_MAP) ||
        (item >= RG_DEKU_TREE_COMPASS && item <= RG_ICE_CAVERN_COMPASS)) {
        u16 mapIndex = gSaveContext.mapIndex;
        u8 numOfKeysOnKeyring = 0;
        switch (item) {
            case RG_DEKU_TREE_MAP:
            case RG_DEKU_TREE_COMPASS:
                mapIndex = SCENE_DEKU_TREE;
                break;
            case RG_DODONGOS_CAVERN_MAP:
            case RG_DODONGOS_CAVERN_COMPASS:
                mapIndex = SCENE_DODONGOS_CAVERN;
                break;
            case RG_JABU_JABUS_BELLY_MAP:
            case RG_JABU_JABUS_BELLY_COMPASS:
                mapIndex = SCENE_JABU_JABU;
                break;
            case RG_FOREST_TEMPLE_MAP:
            case RG_FOREST_TEMPLE_COMPASS:
            case RG_FOREST_TEMPLE_SMALL_KEY:
            case RG_FOREST_TEMPLE_KEY_RING:
            case RG_FOREST_TEMPLE_BOSS_KEY:
                mapIndex = SCENE_FOREST_TEMPLE;
                numOfKeysOnKeyring = FOREST_TEMPLE_SMALL_KEY_MAX;
                break;
            case RG_FIRE_TEMPLE_MAP:
            case RG_FIRE_TEMPLE_COMPASS:
            case RG_FIRE_TEMPLE_SMALL_KEY:
            case RG_FIRE_TEMPLE_KEY_RING:
            case RG_FIRE_TEMPLE_BOSS_KEY:
                mapIndex = SCENE_FIRE_TEMPLE;
                numOfKeysOnKeyring = FIRE_TEMPLE_SMALL_KEY_MAX;
                break;
            case RG_WATER_TEMPLE_MAP:
            case RG_WATER_TEMPLE_COMPASS:
            case RG_WATER_TEMPLE_SMALL_KEY:
            case RG_WATER_TEMPLE_KEY_RING:
            case RG_WATER_TEMPLE_BOSS_KEY:
                mapIndex = SCENE_WATER_TEMPLE;
                numOfKeysOnKeyring = WATER_TEMPLE_SMALL_KEY_MAX;
                break;
            case RG_SPIRIT_TEMPLE_MAP:
            case RG_SPIRIT_TEMPLE_COMPASS:
            case RG_SPIRIT_TEMPLE_SMALL_KEY:
            case RG_SPIRIT_TEMPLE_KEY_RING:
            case RG_SPIRIT_TEMPLE_BOSS_KEY:
                mapIndex = SCENE_SPIRIT_TEMPLE;
                numOfKeysOnKeyring = SPIRIT_TEMPLE_SMALL_KEY_MAX;
                break;
            case RG_SHADOW_TEMPLE_MAP:
            case RG_SHADOW_TEMPLE_COMPASS:
            case RG_SHADOW_TEMPLE_SMALL_KEY:
            case RG_SHADOW_TEMPLE_KEY_RING:
            case RG_SHADOW_TEMPLE_BOSS_KEY:
                mapIndex = SCENE_SHADOW_TEMPLE;
                numOfKeysOnKeyring = SHADOW_TEMPLE_SMALL_KEY_MAX;
                break;
            case RG_BOTTOM_OF_THE_WELL_MAP:
            case RG_BOTTOM_OF_THE_WELL_COMPASS:
            case RG_BOTTOM_OF_THE_WELL_SMALL_KEY:
            case RG_BOTTOM_OF_THE_WELL_KEY_RING:
                mapIndex = SCENE_BOTTOM_OF_THE_WELL;
                numOfKeysOnKeyring = BOTTOM_OF_THE_WELL_SMALL_KEY_MAX;
                break;
            case RG_ICE_CAVERN_MAP:
            case RG_ICE_CAVERN_COMPASS:
                mapIndex = SCENE_ICE_CAVERN;
                break;
            case RG_GANONS_CASTLE_BOSS_KEY:
                mapIndex = SCENE_GANONS_TOWER;
                break;
            case RG_GERUDO_TRAINING_GROUND_SMALL_KEY:
            case RG_GERUDO_TRAINING_GROUND_KEY_RING:
                mapIndex = SCENE_GERUDO_TRAINING_GROUND;
                numOfKeysOnKeyring = GERUDO_TRAINING_GROUND_SMALL_KEY_MAX;
                break;
            case RG_GERUDO_FORTRESS_SMALL_KEY:
            case RG_GERUDO_FORTRESS_KEY_RING:
                mapIndex = SCENE_THIEVES_HIDEOUT;
                numOfKeysOnKeyring = GERUDO_FORTRESS_SMALL_KEY_MAX;
                break;
            case RG_GANONS_CASTLE_SMALL_KEY:
            case RG_GANONS_CASTLE_KEY_RING:
                mapIndex = SCENE_INSIDE_GANONS_CASTLE;
                numOfKeysOnKeyring = GANONS_CASTLE_SMALL_KEY_MAX;
                break;
            default:
                break;
        }

        if ((item >= RG_FOREST_TEMPLE_SMALL_KEY) && (item <= RG_GANONS_CASTLE_SMALL_KEY)) {
            gSaveContext.ship.stats.dungeonKeys[mapIndex]++;
            if (gSaveContext.inventory.dungeonKeys[mapIndex] < 0) {
                gSaveContext.inventory.dungeonKeys[mapIndex] = 1;
            } else {
                gSaveContext.inventory.dungeonKeys[mapIndex]++;
            }
            return Return_Item_Entry(giEntry, RG_NONE);
        }

        if ((item >= RG_FOREST_TEMPLE_KEY_RING) && (item <= RG_GANONS_CASTLE_KEY_RING)) {
            gSaveContext.ship.stats.dungeonKeys[mapIndex] = numOfKeysOnKeyring;
            gSaveContext.inventory.dungeonKeys[mapIndex] = numOfKeysOnKeyring;
            return Return_Item_Entry(giEntry, RG_NONE);
        }

        u32 bitmask;
        if ((item >= RG_DEKU_TREE_MAP) && (item <= RG_ICE_CAVERN_MAP)) {
            bitmask = gBitFlags[2];
        } else if ((item >= RG_DEKU_TREE_COMPASS) && (item <= RG_ICE_CAVERN_COMPASS)) {
            bitmask = gBitFlags[1];
        } else {
            bitmask = gBitFlags[0];
        }

        gSaveContext.inventory.dungeonItems[mapIndex] |= bitmask;
        return Return_Item_Entry(giEntry, RG_NONE);
    }

    switch (item) {
        case RG_MAGIC_SINGLE:
            gSaveContext.isMagicAcquired = true;
            gSaveContext.magicFillTarget = MAGIC_NORMAL_METER;
            Magic_Fill(play);
            break;
        case RG_MAGIC_DOUBLE:
            if (!gSaveContext.isMagicAcquired) {
                gSaveContext.isMagicAcquired = true;
            }
            gSaveContext.isDoubleMagicAcquired = true;
            gSaveContext.magicFillTarget = MAGIC_DOUBLE_METER;
            gSaveContext.magicLevel = 0;
            Magic_Fill(play);
            break;
        case RG_MAGIC_BEAN_PACK:
            if (INV_CONTENT(ITEM_BEAN) == ITEM_NONE) {
                INV_CONTENT(ITEM_BEAN) = ITEM_BEAN;
                AMMO(ITEM_BEAN) = 10;
            }
            break;
        case RG_DOUBLE_DEFENSE:
            gSaveContext.isDoubleDefenseAcquired = true;
            gSaveContext.inventory.defenseHearts = 20;
            gSaveContext.healthAccumulator = MAX_HEALTH;
            break;
        case RG_TYCOON_WALLET:
            Inventory_ChangeUpgrade(UPG_WALLET, 3);
            if (OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(RSK_FULL_WALLETS)) {
                Rupees_ChangeBy(999);
            }
            break;
        case RG_TRIFORCE:
            GameInteractor_SetTriforceHuntCreditsWarpActive(true);
            break;
        case RG_TRIFORCE_PIECE:
            gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected++;
            GameInteractor_SetTriforceHuntPieceGiven(true);
            // Reward/win triggers (Ganon's Boss Key, Ganon's Soul, win condition) are evaluated by
            // CheckTriggers() on item receive, so Triforce Piece thresholds are handled there.
            break;
        case RG_PROGRESSIVE_BOMBCHU_BAG:
            OTRGlobals::Instance->gRandoContext->HandleGetBombchuBag();
            break;
        case RG_MASTER_SWORD:
            if (!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER)) {
                gSaveContext.inventory.equipment |= gBitFlags[1] << gEquipShifts[EQUIP_TYPE_SWORD];
            }
            break;
        case RG_DEKU_STICK_BAG:
            Inventory_ChangeUpgrade(UPG_STICKS, 1);
            INV_CONTENT(ITEM_STICK) = ITEM_STICK;
            AMMO(ITEM_STICK) = static_cast<int8_t>(CUR_CAPACITY(UPG_STICKS));
            break;
        case RG_DEKU_NUT_BAG:
            Inventory_ChangeUpgrade(UPG_NUTS, 1);
            INV_CONTENT(ITEM_NUT) = ITEM_NUT;
            AMMO(ITEM_NUT) = static_cast<int8_t>(CUR_CAPACITY(UPG_NUTS));
            break;
        // Custom Items (Second Inventory Page)
        // IMPORTANT: Use ExtInv_SetItemById() instead of INV_CONTENT() for custom items
        // to avoid buffer overflow on gItemSlots[] array (which only has 54 elements)
        case RG_ROCS_FEATHER:
            // Vanilla rando Roc's Feather: lives in the Nayru's Love slot and cycles with it
            // (see RocsFeatherCycle.c). Skijer's feather is RG_PROGRESSIVE_ROCS instead.
            Flags_SetRandomizerInf(RAND_INF_OBTAINED_ROCS_FEATHER);
            if (INV_CONTENT(ITEM_NAYRUS_LOVE) == ITEM_NONE) {
                INV_CONTENT(ITEM_NAYRUS_LOVE) = ITEM_ROCS_FEATHER;
            }
            break;
        case RG_PROGRESSIVE_ROCS:
            // Progressive Roc's: Give Feather first, then Cape as upgrade
            switch (ExtInv_GetSlotItem(SLOT_ROCS)) { // Skijer's NEI
                case ITEM_NONE:
                    ExtInv_SetItemById(ITEM_ROCS_FEATHER_SKIJER);
                    break;
                case ITEM_ROCS_FEATHER_SKIJER:
                default:
                    ExtInv_SetItemById(ITEM_ROCS_CAPE);
                    break;
            }
            break;
        // Skijer's NEI: the uniform "ExtInv_SetItemById(ITEM_x)" custom-item + MM-mask arms are
        // folded into the registry-driven default below (Nei_FindByRg(item)->item). RG_ROCS_CAPE,
        // the 24 page-2 items, and the cosmetic MM masks all flow through it. Arms doing extra work
        // (Roc progressive above; the 5 trade masks below that also set OOT trade flags) stay explicit.
        // Dual Cane (Somaria / Pacci) — six skills sharing ONE inventory slot, so the
        // registry-driven default ("put ITEM_CANE_OF_SOMARIA in its slot") is not enough:
        // each copy of this check has to light the NEXT skill bit. Cane_GiveSkill also
        // drops the cane into the slot on the first one, so the slot still fills itself.
        // Order alternates the two canes so the yellow one shows up early:
        // Statue -> Flip -> Block -> Stone -> Platform -> Ultrahand.
        case RG_CANE_OF_SOMARIA: {
            static const uint8_t kCaneOrder[6] = { 0, 3, 1, 4, 2, 5 };
            for (int i = 0; i < 6; i++) {
                if (Cane_GiveSkill(kCaneOrder[i])) {
                    break;
                }
            }
            break;
        }
        // Extended Equipment (ownership bits in upper 16 of inventory.equipment)
        case RG_EXT_CANE_OF_BYRNA:
            ExtEquip_GiveItem(EQUIP_TYPE_SWORD, 1);
            break;
        case RG_EXT_FOUR_SWORD:
            ExtEquip_GiveItem(EQUIP_TYPE_SWORD, 2);
            break;
        // NEI Weapon Upgrades — progressive. Level 1 grants the vanilla weapon (normal
        // SaveContext state, owned-bit only — same convention as RG_MASTER_SWORD above, no
        // auto-equip); subsequent copies set a Nei_Save()->weaponUpgrades bit.
        case RG_PROGRESSIVE_HAMMER:
            if (INV_CONTENT(ITEM_HAMMER) == ITEM_NONE) {
                INV_CONTENT(ITEM_HAMMER) = ITEM_HAMMER;
            } else {
                WeaponUpgrade_SetHammerAxe(1);
            }
            break;
        // Stone of Agony, 2 levels. Level 1 is the vanilla quest item (keeps its
        // grotto rumble); level 2 is the Quartz of Motion, used from the kaleido
        // (A on the stone's slot) — see mods/quartz_of_motion/quartz_kaleido.cpp.
        case RG_STONE_OF_AGONY:
            if (!CHECK_QUEST_ITEM(QUEST_STONE_OF_AGONY)) {
                gSaveContext.inventory.questItems |= gBitFlags[QUEST_STONE_OF_AGONY];
            } else {
                Nei_Save()->quartzOwned = 1;
            }
            break;
        case RG_PROGRESSIVE_KOKIRI_SWORD:
            if (!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI)) {
                gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
            } else {
                WeaponUpgrade_GiveProgressiveKokiri(); // Razor, then Gilded
            }
            break;
        case RG_PROGRESSIVE_MASTER_SWORD:
            if (!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER)) {
                gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
            } else {
                WeaponUpgrade_SetTrueMaster(1);
            }
            break;
        case RG_PROGRESSIVE_BGS:
            if (!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON)) {
                gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
                gSaveContext.bgsFlag = 1; // HasItem(RG_BIGGORON_SWORD) requires bgsFlag
            } else {
                WeaponUpgrade_SetGreatFairy(1);
            }
            break;
        // Per-level chain identities — the resolved form of the RG_PROGRESSIVE_* entries above
        // (item.cpp GetGIEntry). Each also heals the levels below it so an explicit console give
        // can't strand the chain. Skijer's NEI
        case RG_RAZOR_SWORD:
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
            WeaponUpgrade_SetRazor(1);
            break;
        case RG_GILDED_SWORD:
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
            WeaponUpgrade_SetRazor(1);
            WeaponUpgrade_SetGilded(1);
            break;
        case RG_TRUE_MASTER_SWORD:
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
            WeaponUpgrade_SetTrueMaster(1);
            break;
        case RG_GREAT_FAIRY_SWORD:
            if (!CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON)) {
                gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
                gSaveContext.bgsFlag = 1;
            }
            WeaponUpgrade_SetGreatFairy(1);
            break;
        case RG_IRON_KNUCKLE_AXE:
            if (INV_CONTENT(ITEM_HAMMER) == ITEM_NONE) {
                INV_CONTENT(ITEM_HAMMER) = ITEM_HAMMER;
            }
            WeaponUpgrade_SetHammerAxe(1);
            break;
        case RG_ULTRASHOT:
            INV_CONTENT(ITEM_HOOKSHOT) = ITEM_LONGSHOT; // the Ultrashot rides the Longshot
            Nei_Save()->ultrashotOwned = 1;
            break;
        case RG_QUARTZ_OF_MOTION:
            gSaveContext.inventory.questItems |= gBitFlags[QUEST_STONE_OF_AGONY]; // chain heal
            Nei_Save()->quartzOwned = 1;
            break;
        // Clawshot: a real owned item in OoT too, not just a cross-collection trophy. It has no slot
        // of its own — it rides the hookshot/longshot cell, and A there opens the vanilla<->clawshot
        // flip (Clawshot_HandleKaleidoSelector), exactly the Lens/Pictograph Box arrangement. All the
        // machinery already existed; the give was simply never lighting the ownership bit.
        // Skijer's NEI
        case RG_CLAWSHOT:
            TwilightUpgrade_SetClawshot(1);
            break;
        // Dual Cane per-skill identities (resolution targets of RG_CANE_OF_SOMARIA). Skill ids:
        // 0 Statue, 1 Block, 2 Platform (Somaria) / 3 Flip, 4 Stone, 5 Ultrahand (Pacci).
        case RG_CANE_PACCI_FLIP:
            Cane_GiveSkill(3);
            break;
        case RG_CANE_SOMARIA_BLOCK:
            Cane_GiveSkill(1);
            break;
        case RG_CANE_PACCI_STONE:
            Cane_GiveSkill(4);
            break;
        case RG_CANE_SOMARIA_PLATFORM:
            Cane_GiveSkill(2);
            break;
        case RG_CANE_PACCI_ULTRAHAND:
            Cane_GiveSkill(5);
            break;
        case RG_EXT_DIVINE_SHIELD:
            ExtEquip_GiveItem(EQUIP_TYPE_SHIELD, 1);
            break;
        case RG_EXT_SHEIKAH_SHIELD:
            ExtEquip_GiveItem(EQUIP_TYPE_SHIELD, 2);
            break;
        case RG_EXT_SHIELD_OF_IKANA:
            ExtEquip_GiveItem(EQUIP_TYPE_SHIELD, 3);
            break;
        // Tunic slots remapped 2026-07-16: 1=Champion, 2=Spirit, 3=Sage's. The Magic Cape is no
        // longer a grid slot — it grants via its dedicated ownership flag.
        case RG_EXT_MAGIC_CAPE:
            ExtEquip_GiveCape();
            break;
        case RG_EXT_SPIRIT_BREASTPLATE:
            ExtEquip_GiveItem(EQUIP_TYPE_TUNIC, 2);
            break;
        case RG_EXT_CHAMPIONS_TUNIC:
            ExtEquip_GiveItem(EQUIP_TYPE_TUNIC, 1);
            break;
        case RG_EXT_PEGASUS_ANKLET:
            ExtEquip_GiveItem(EQUIP_TYPE_BOOTS, 1);
            break;
        // The last three grid cells. Playable in both games but with no randomizer identity, so the
        // save editor was the only way to own them — and nothing for FleetSync to carry. Skijer's NEI
        case RG_EXT_TRIDENT:
            ExtEquip_GiveItem(EQUIP_TYPE_SWORD, 3); // bit 18
            break;
        case RG_EXT_CLIMB_BOOTS:
            ExtEquip_GiveItem(EQUIP_TYPE_BOOTS, 2); // bit 26
            break;
        case RG_EXT_ROC_BOOTS:
            ExtEquip_GiveItem(EQUIP_TYPE_BOOTS, 3); // bit 27
            break;
        // The four 2026-08-06 page-2 additions — EXT (u16) inventory ids into the widened page-2
        // store. Behaviorless-for-now real items (cell + icon + get-item model). Skijer's NEI
        case RG_SHEIKAH_SLATE:
            ExtInv_GiveItem(SLOT_SHEIKAH_SLATE, EXT_ITEM_SHEIKAH_SLATE);
            break;
        // Sheikah Slate runes — sibling items over the slate cell (wand idiom). Each lights its
        // slateRunesOwned bit; the first one also hands over the slate itself (Slate_GrantRune).
        case RG_SLATE_RUNE_BOMB:
            Slate_GrantRune(SLATE_RUNE_BOMB);
            break;
        case RG_SLATE_RUNE_MASTER_CYCLE:
            Slate_GrantRune(SLATE_RUNE_MASTER_CYCLE);
            break;
        case RG_SLATE_RUNE_STASIS:
            Slate_GrantRune(SLATE_RUNE_STASIS);
            break;
        case RG_SLATE_RUNE_CRYONIS:
            Slate_GrantRune(SLATE_RUNE_CRYONIS);
            break;
        // The Desire Sensor's pool item, rehoused: it grants the Sensor rune, not an item of its own.
        case RG_DESIRE_SENSOR:
            Slate_GrantRune(SLATE_RUNE_SENSOR);
            break;
        case RG_PHANTOM_HOURGLASS:
            ExtInv_GiveItem(SLOT_PHANTOM_HOURGLASS, EXT_ITEM_PHANTOM_HOURGLASS);
            break;
        case RG_SHADOW_CRYSTAL:
            ExtInv_GiveItem(SLOT_SHADOW_CRYSTAL, EXT_ITEM_SHADOW_CRYSTAL);
            break;
        case RG_ROD_OF_SEASONS:
            ExtInv_GiveItem(SLOT_ROD_OF_SEASONS, EXT_ITEM_ROD_OF_SEASONS);
            break;
        // Crossover Items. Both registry rows carry NEI_NO_SLOT, so the generic default arm
        // below (ExtInv_SetItemById) silently drops them — the item would be consumed by the
        // check and lost. Ownership is a flag read by BrokenItems_FormUnlocked.
        case RG_POKEBALL:
            Nei_Save()->pokeballOwned = 1;
            break;
        case RG_MARIO_MASK:
            Flags_SetRandomizerInf(RAND_INF_OBTAINED_MARIO_MASK);
            break;
        // Rod of Seasons — sibling items over the rod's cell (slate idiom). Each lights its own
        // season, and the first one obtained hands over the rod itself.
        case RG_SEASON_SPRING:
            Seasons_GrantSeason(SEASON_SPRING);
            break;
        case RG_SEASON_SUMMER:
            Seasons_GrantSeason(SEASON_SUMMER);
            break;
        case RG_SEASON_AUTUMN:
            Seasons_GrantSeason(SEASON_AUTUMN);
            break;
        case RG_SEASON_WINTER:
            Seasons_GrantSeason(SEASON_WINTER);
            break;
        case RG_EXT_PENDANT_OF_MEMORIES:
            // ONE grant: the adult trade wheel. The old dual-grant also lit the ExtEquip BOOTS-2 bit
            // as a "moveset" flag — that slot is the CLIMB BOOTS since 2026-07-29, so granting it
            // would hand out a pair of boots. equip_pendant.c keys off ExtEquip_PendantActive(), which
            // reads the trade bit. Idempotent with RG_MM_PENDANT_OF_MEMORIES. Skijer's NEI
            TradeAdult_GiveItem(ITEM_EXT_BOOTS_2);
            break;
        case RG_EXT_WATER_DRAGON_SCALE:
            ExtEquip_GiveItem(EQUIP_TYPE_TUNIC, 3);
            break;
        // MM Masks (Third Inventory Page) — only the masks that ALSO set an OOT trade flag stay
        // explicit. The 19 cosmetic-only masks fold into the registry default below. Skijer's NEI
        case RG_MM_MASK_KEATON:
            ExtInv_SetItemById(ITEM_MM_MASK_KEATON);
            // Also give OOT Keaton Mask so trade quest interactions work (gate guard, etc.)
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_KEATON);
            if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
                INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_KEATON;
            }
            break;
        case RG_MM_MASK_BUNNY:
            ExtInv_SetItemById(ITEM_MM_MASK_BUNNY);
            // Also give OOT Bunny Hood so vanilla equip effect works
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_BUNNY);
            if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
                INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_BUNNY;
            }
            break;
        case RG_MM_MASK_GORON:
            ExtInv_SetItemById(ITEM_MM_MASK_GORON);
            // Also give OOT Goron Mask so trade quest interactions work
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_GORON);
            if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
                INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_GORON;
            }
            break;
        case RG_MM_MASK_TRUTH:
            ExtInv_SetItemById(ITEM_MM_MASK_TRUTH);
            // Also give OOT Mask of Truth so vanilla equip effect works (Gossip Stones, etc.)
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH);
            if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
                INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_TRUTH;
            }
            break;
        case RG_MM_MASK_ZORA:
            ExtInv_SetItemById(ITEM_MM_MASK_ZORA);
            // Also give OOT Zora Mask so trade quest interactions work
            Flags_SetRandomizerInf(RAND_INF_CHILD_TRADES_HAS_MASK_ZORA);
            if (INV_CONTENT(ITEM_TRADE_CHILD) == ITEM_NONE) {
                INV_CONTENT(ITEM_TRADE_CHILD) = ITEM_MASK_ZORA;
            }
            break;
        // MM collectibles ported to OoT rando (Stray Fairy + 4 Boss Remains). They have no OoT
        // inventory presence — collecting the check (model + message) is the whole effect, so the
        // give itself is a no-op. Explicit cases keep them off the registry-default assert below.
        case RG_MM_STRAY_FAIRY:
        case RG_MM_STRAY_FAIRY_WOODFALL:
        case RG_MM_STRAY_FAIRY_SNOWHEAD:
        case RG_MM_STRAY_FAIRY_GREAT_BAY:
        case RG_MM_STRAY_FAIRY_STONE_TOWER:
            break;
        // MM boss remains: no OoT inventory slot, but ownership is recorded in the parallel MM
        // quest store (Nei_Save()->mmQuestItems, FC_MMQ bits) so OoT's mirrored MM quest page
        // lights up and FleetSync carries the bit to MM's native questItems.
        case RG_MM_REMAINS_ODOLWA:
            Nei_Save()->mmQuestItems |= FC_MMQ_REMAINS_ODOLWA;
            break;
        case RG_MM_REMAINS_GOHT:
            Nei_Save()->mmQuestItems |= FC_MMQ_REMAINS_GOHT;
            break;
        case RG_MM_REMAINS_GYORG:
            Nei_Save()->mmQuestItems |= FC_MMQ_REMAINS_GYORG;
            break;
        case RG_MM_REMAINS_TWINMOLD:
            Nei_Save()->mmQuestItems |= FC_MMQ_REMAINS_TWINMOLD;
            break;
        // MM per-dungeon items (small key / boss key / map / compass) — model + message only, no OoT
        // inventory slot; give is a no-op.
        case RG_MM_SMALL_KEY_WOODFALL:
        case RG_MM_SMALL_KEY_SNOWHEAD:
        case RG_MM_SMALL_KEY_GREAT_BAY:
        case RG_MM_SMALL_KEY_STONE_TOWER:
        case RG_MM_BOSS_KEY_WOODFALL:
        case RG_MM_BOSS_KEY_SNOWHEAD:
        case RG_MM_BOSS_KEY_GREAT_BAY:
        case RG_MM_BOSS_KEY_STONE_TOWER:
        case RG_MM_MAP_WOODFALL:
        case RG_MM_MAP_SNOWHEAD:
        case RG_MM_MAP_GREAT_BAY:
        case RG_MM_MAP_STONE_TOWER:
        case RG_MM_COMPASS_WOODFALL:
        case RG_MM_COMPASS_SNOWHEAD:
        case RG_MM_COMPASS_GREAT_BAY:
        case RG_MM_COMPASS_STONE_TOWER:
        case RG_MM_SOUL_GOHT:
        case RG_MM_SOUL_GYORG:
        case RG_MM_SOUL_MAJORA:
        case RG_MM_SOUL_ODOLWA:
        case RG_MM_SOUL_TWINMOLD:
        case RG_MM_SOUL_ALIEN:
        case RG_MM_SOUL_ARMOS:
        case RG_MM_SOUL_BAD_BAT:
        case RG_MM_SOUL_BEAMOS:
        case RG_MM_SOUL_BOE:
        case RG_MM_SOUL_BUBBLE:
        case RG_MM_SOUL_CAPTAIN_KEETA:
        case RG_MM_SOUL_CHUCHU:
        case RG_MM_SOUL_DEATH_ARMOS:
        case RG_MM_SOUL_DEEP_PYTHON:
        case RG_MM_SOUL_DEKU_BABA:
        case RG_MM_SOUL_DEXIHAND:
        case RG_MM_SOUL_DINOLFOS:
        case RG_MM_SOUL_DODONGO:
        case RG_MM_SOUL_DRAGONFLY:
        case RG_MM_SOUL_EENO:
        case RG_MM_SOUL_EYEGORE:
        case RG_MM_SOUL_FREEZARD:
        case RG_MM_SOUL_GARO:
        case RG_MM_SOUL_GEKKO:
        case RG_MM_SOUL_GIANT_BEE:
        case RG_MM_SOUL_GOMESS:
        case RG_MM_SOUL_GUAY:
        case RG_MM_SOUL_HIPLOOP:
        case RG_MM_SOUL_IGOS_DU_IKANA:
        case RG_MM_SOUL_IRON_KNUCKLE:
        case RG_MM_SOUL_KEESE:
        case RG_MM_SOUL_LEEVER:
        case RG_MM_SOUL_LIKE_LIKE:
        case RG_MM_SOUL_MAD_SCRUB:
        case RG_MM_SOUL_NEJIRON:
        case RG_MM_SOUL_OCTOROK:
        case RG_MM_SOUL_PEAHAT:
        case RG_MM_SOUL_PIRATE:
        case RG_MM_SOUL_POE:
        case RG_MM_SOUL_REDEAD:
        case RG_MM_SOUL_SHELLBLADE:
        case RG_MM_SOUL_SKULLFISH:
        case RG_MM_SOUL_SKULLTULA:
        case RG_MM_SOUL_SNAPPER:
        case RG_MM_SOUL_STALCHILD:
        case RG_MM_SOUL_TAKKURI:
        case RG_MM_SOUL_TEKTITE:
        case RG_MM_SOUL_WALLMASTER:
        case RG_MM_SOUL_WART:
        case RG_MM_SOUL_WIZROBE:
        case RG_MM_SOUL_WOLFOS:
        // MM trade / quest-chain items — grant the REAL NEI inventory item, same APIs the give-all
        // debug menu path uses: adult-trade wheel (pause kaleido slots 4/5, trade_items.c),
        // pictobox (picto_box.c), powder keg (power_keg.c), Bombers' Notebook (mmQuestItems bit).
        case RG_MM_MOONS_TEAR:
            TradeAdult_GiveItem(ITEM_MM_MOONS_TEAR);
            break;
        case RG_MM_DEED_LAND:
            TradeAdult_GiveItem(ITEM_MM_DEED_LAND);
            break;
        case RG_MM_DEED_SWAMP:
            TradeAdult_GiveItem(ITEM_MM_DEED_SWAMP);
            break;
        case RG_MM_DEED_MOUNTAIN:
            TradeAdult_GiveItem(ITEM_MM_DEED_MOUNTAIN);
            break;
        case RG_MM_DEED_OCEAN:
            TradeAdult_GiveItem(ITEM_MM_DEED_OCEAN);
            break;
        case RG_MM_ROOM_KEY:
            TradeAdult_GiveItem(ITEM_MM_ROOM_KEY);
            break;
        case RG_MM_LETTER_TO_KAFEI:
            TradeAdult_GiveItem(ITEM_MM_LETTER_KAFEI);
            break;
        case RG_MM_LETTER_TO_MAMA:
            TradeAdult_GiveItem(ITEM_MM_SPECIAL_DELIVERY);
            break;
        case RG_MM_PENDANT_OF_MEMORIES:
            // Trade index 19 is the pendant's only ownership flag (see RG_EXT_PENDANT_OF_MEMORIES).
            TradeAdult_GiveItem(ITEM_EXT_BOOTS_2);
            break;
        case RG_MM_PICTOGRAPH_BOX:
            Picto_SetOwned(1);
            break;
        case RG_MM_POWDER_KEG:
            PowerKeg_SetOwned(1);
            if (PowerKeg_GetCount() < 1) {
                PowerKeg_SetCount(1); // arrives loaded, like buying one in MM
            }
            break;
        case RG_MM_BOMBERS_NOTEBOOK:
            Nei_Save()->mmQuestItems |= FC_MMQ_BOMBERS_NOTEBOOK;
            break;
        // MM ocarina songs, owl-statue warps, and Tingle maps — model + message only, no OoT
        // inventory slot; give is a no-op.
        // MM songs: record ownership in the parallel MM quest store (Nei_Save()->mmQuestItems,
        // FC_MMQ bits — the bits OoT's mirrored MM quest page reads) so the icon lights up and
        // FleetSync carries it to MM's native questItems. Shared-identity songs (Saria / Sun /
        // Time / Epona / Storms) instead set OoT's NATIVE questItems bit — the mirror page reads
        // those rows natively and the song is the same item in both games.
        case RG_MM_SONG_SONATA:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_SONATA;
            break;
        case RG_MM_SONG_LULLABY:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_GORON_LULLABY;
            break;
        // Progressive Goron Lullaby, MM's default shape for this song: copy 1 is the Intro (which has
        // no quest-page icon of its own, exactly like RG_MM_SONG_LULLABY_INTRO), copy 2 completes it.
        // Escalating on the quest bit keeps it idempotent if the copies arrive out of order.
        case RG_MM_SONG_LULLABY_PROGRESSIVE:
            // Copy 1 is the Intro, which has no quest-page icon of its own (same as
            // RG_MM_SONG_LULLABY_INTRO above); copy 2 completes the song and lights the icon. The
            // level comes from the FC registry, which the record hook at the top of this function has
            // ALREADY bumped for this pickup — so >= 2 means "this is the second copy". There is no
            // separate intro bit to read, and adding one would touch the save layout for nothing.
            if (Nei_Save()->comboObtainedFc[FCI_MM_SONG_LULLABY_PROGRESSIVE] >= 2) {
                Nei_Save()->mmQuestItems |= FC_MMQ_SONG_GORON_LULLABY;
            }
            break;
        case RG_MM_SONG_NOVA:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_NEW_WAVE;
            break;
        // The 3 NEI custom songs. Each one OWNS the MM quest-page row of the song it replaces, so it
        // sets that row's bit — Command Melody takes Song of Time's, Fugue of Home takes Epona's,
        // Ballad of the Hero takes Song of Storms' (sMmPageSongs, z_kaleido_collect.c). They exist
        // precisely so those three rows are not duplicates of songs OoT already has.
        case RG_NEI_SONG_FUGUE_OF_HOME:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_EPONA;
            break;
        case RG_NEI_SONG_COMMAND_MELODY:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_TIME;
            break;
        case RG_NEI_SONG_BALLAD_OF_HERO:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_STORMS;
            break;
        case RG_MM_SONG_ELEGY:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_ELEGY;
            break;
        case RG_MM_SONG_OATH:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_OATH;
            break;
        case RG_MM_SONG_HEALING:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_HEALING;
            break;
        case RG_MM_SONG_SOARING:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_SOARING;
            break;
        case RG_MM_SONG_SARIA:
            gSaveContext.inventory.questItems |= (1 << QUEST_SONG_SARIA);
            break;
        case RG_MM_SONG_SUN:
            gSaveContext.inventory.questItems |= (1 << QUEST_SONG_SUN);
            break;
        case RG_MM_SONG_TIME:
            gSaveContext.inventory.questItems |= (1 << QUEST_SONG_TIME);
            break;
        case RG_MM_SONG_EPONA:
            gSaveContext.inventory.questItems |= (1 << QUEST_SONG_EPONA);
            break;
        case RG_MM_SONG_STORMS:
            gSaveContext.inventory.questItems |= (1 << QUEST_SONG_STORMS);
            break;
        case RG_MM_SONG_DOUBLE_TIME:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_TIME_DOUBLE;
            break;
        case RG_MM_SONG_INVERTED_TIME:
            Nei_Save()->mmQuestItems |= FC_MMQ_SONG_TIME_INVERTED;
            break;
        // Lullaby Intro has no quest-page icon of its own (MM tracks it separately); no-op.
        case RG_MM_SONG_LULLABY_INTRO:
        case RG_MM_OWL_CLOCK_TOWN_SOUTH:
        case RG_MM_OWL_GREAT_BAY_COAST:
        case RG_MM_OWL_IKANA_CANYON:
        case RG_MM_OWL_MILK_ROAD:
        case RG_MM_OWL_MOUNTAIN_VILLAGE:
        case RG_MM_OWL_SNOWHEAD:
        case RG_MM_OWL_SOUTHERN_SWAMP:
        case RG_MM_OWL_STONE_TOWER:
        case RG_MM_OWL_WOODFALL:
        case RG_MM_OWL_ZORA_CAPE:
        case RG_MM_TINGLE_MAP_CLOCK_TOWN:
        case RG_MM_TINGLE_MAP_WOODFALL:
        case RG_MM_TINGLE_MAP_SNOWHEAD:
        case RG_MM_TINGLE_MAP_ROMANI_RANCH:
        case RG_MM_TINGLE_MAP_GREAT_BAY:
        case RG_MM_TINGLE_MAP_STONE_TOWER:
        // Final MM cross items with no OoT store: healed frogs (Don Gero's choir), Great Spin
        // (WEEKEVENTREG is MM-side) and the 6 clock-shuffle halves — model + message only; the FC
        // record hook above already counted the pickup for the cross-game registry.
        case RG_MM_FROG_BLUE:
        case RG_MM_FROG_CYAN:
        case RG_MM_FROG_PINK:
        case RG_MM_FROG_WHITE:
        case RG_MM_GREAT_SPIN_ATTACK:
        case RG_MM_TIME_DAY_1:
        case RG_MM_TIME_DAY_2:
        case RG_MM_TIME_DAY_3:
        case RG_MM_TIME_NIGHT_1:
        case RG_MM_TIME_NIGHT_2:
        case RG_MM_TIME_NIGHT_3:
        // Progressive clock halves: identical no-op here. Which half each copy stands for is MM's
        // call (ClockItems), and the FC record hook above already counted the pickup for the cross
        // registry, which is the whole mechanism by which it reaches Termina. Skijer's NEI
        case RG_MM_TIME_PROGRESSIVE:
        // Gold Dust normally grants in the bottle-slot block ABOVE the switch; the obtainability
        // gate keeps it from firing with full bottles. This case only stops the default-assert if
        // it ever falls through anyway (content lost, matching the mushroom's failure mode).
        case RG_MM_BOTTLE_GOLD_DUST:
            break;
        // MM Swamp/Ocean GS tokens — SoH DOES have a store: the FC registry's raw MM world-progress
        // counters (FleetComboIds.h FC_MM_SKULLS_*; the array FleetSync max-merges with MM's
        // comboObtained wholesale). Incrementing the cell here is the canonical obtain; MM's
        // Inventory_IncrementSkullTokenCount picks it up on sync.
        case RG_MM_GS_TOKEN_SWAMP: {
            NeiSaveData* nei = Nei_Save();
            if (nei->comboObtained[FC_MM_SKULLS_SWAMP] < 255) {
                nei->comboObtained[FC_MM_SKULLS_SWAMP]++;
            }
            break;
        }
        case RG_MM_GS_TOKEN_OCEAN: {
            NeiSaveData* nei = Nei_Save();
            if (nei->comboObtained[FC_MM_SKULLS_OCEAN] < 255) {
                nei->comboObtained[FC_MM_SKULLS_OCEAN]++;
            }
            break;
        }
        // Bottle Randomizer extra items (Skijer's NEI, custom_bottles.cpp): REAL grants. Setting the
        // ownership flag is the whole give — mm_bottle_items.cpp's per-frame enforcement projects the
        // item into SLOT_BOTTLE_3 (Net) / SLOT_BOTTLE_4 (Bottomless, shows as empty bottle until
        // filled) and refreshes any C-button. Same store the debug save-editor toggles.
        case RG_NET:
            Bottle_SetNetOwned(1);
            break;
        case RG_BOTTOMLESS_BOTTLE:
            Bottle_SetBottomlessOwned(1);
            break;
        // Skijer's NEI — Bomb Arrows owns no inventory cell any more (it is the bow's element flag),
        // so the generic ExtInv_SetItemById arm below would silently no-op. Set the save flag.
        case RG_BOMB_ARROWS:
            Nei_Save()->bombArrowsOwned = 1;
            break;
        // Elemental Wand: whichever rod lands grants that mode AND the slot. In "Single item" mode
        // one pickup lights all six; in "Elemental shuffle" each rod is its own check. Wand_GrantMode
        // handles both, so the six arms are identical by design.
        case RG_ELEMENTAL_WAND:
        case RG_WAND_SAND_ROD:
            Wand_GrantMode(WAND_MODE_SAND);
            break;
        case RG_WAND_TORNADO_ROD:
            Wand_GrantMode(WAND_MODE_TORNADO);
            break;
        case RG_WAND_WATER_ROD:
            Wand_GrantMode(WAND_MODE_WATER);
            break;
        case RG_WAND_METEOR_ROD:
            Wand_GrantMode(WAND_MODE_METEOR);
            break;
        case RG_WAND_STORM_ROD:
            Wand_GrantMode(WAND_MODE_STORM);
            break;
        case RG_WAND_SHADOW_SCEPTER:
            Wand_GrantMode(WAND_MODE_SCEPTER);
            break;
        default: {
            // Skijer's NEI: generic give for uniform custom-item + MM-mask arms. The registry row
            // (keyed by RG) names the page-2/3 inventory item; identical to the old per-RG
            // ExtInv_SetItemById(ITEM_x). Rows without an inventory slot fall through to the warning.
            const NeiItem* neiGive = Nei_FindByRg((int16_t)item);
            // Masks carry slot=NEI_NO_SLOT; ExtInv_SetItemById resolves their page-3 slot. Skijer's NEI
            if (neiGive != NULL && neiGive->item != NEI_NO_ITEM) {
                ExtInv_SetItemById((uint16_t)neiGive->item); // u8 would truncate the EXT ids (0x220+)
                break;
            }
            // The check is already marked collected, so a missing arm eats the item in silence.
            LUSLOG_ERROR("Randomizer_Item_Give didn't have behaviour specified for getItemId=%d", item);
            assert(false);
            return -1;
        }
    }

    return Return_Item_Entry(giEntry, RG_NONE);
}
