/**
 * extended_player.c - Extended player item action system
 *
 * Maps custom ITEM_xxx values to PLAYER_IA_xxx actions, and each custom
 * PLAYER_IA_xxx to its model group / update func / init func.
 *
 * MM-PORT BOUNDARY: every custom item is one row in sNeiItems[] below. To port
 * an item to MM (2ship), copy its logic module + its single descriptor row.
 * The four ExtPlayer_* getters are thin lookups over that table with a vanilla
 * fallback, so there is exactly one place that describes an item's engine glue.
 *
 * Items whose action is a *vanilla* PLAYER_IA (bow combos, swords, medallions ->
 * spells, Chateau Romani -> blue potion) or is chosen dynamically (SW97 arrows ->
 * bow/slingshot by age) are NOT table rows: they alias vanilla behavior and are
 * resolved in ExtPlayer_GetItemAction before the table lookup.
 */

#include "extended_player.h"
#include "extended_inventory.h" // SLOT_*, AGE_REQ_*, NeiItem (Skijer's NEI)
#include "extended_equipment.h" // ITEM_EXT_SWORD_* (ext swords ride B as themselves)
#include "z64.h"
#include "mods/items/custom_items.h"
#include "assets/soh_assets.h"                           // icon textures (Skijer's NEI)
#include "soh/Enhancements/randomizer/randomizerTypes.h" // RG_* (Skijer's NEI)
#include "soh/Enhancements/randomizer/draw.h"            // Randomizer_Draw* (Skijer's NEI)
#include <stddef.h>                                      // NULL

// External reference to vanilla arrays
extern int8_t sItemActions[];
extern uint8_t sActionModelGroups[];
extern s32 (*sItemActionUpdateFuncs[])(Player* this, PlayState* play);
extern void (*sItemActionInitFuncs[])(PlayState* play, Player* this);

// External vanilla functions used by custom items
extern s32 func_8083485C(Player* this, PlayState* play);
extern s32 Player_UpperAction_Sword(Player* this, PlayState* play);
extern void Player_InitDefaultIA(PlayState* play, Player* this);

// External custom item upper action functions
extern s32 Player_UpperAction_Beetle(Player* this, PlayState* play);
extern s32 Player_UpperAction_BombArrows(Player* this, PlayState* play);
extern s32 Player_UpperAction_CaneOfSomaria(Player* this, PlayState* play);
extern s32 Player_UpperAction_DekuLeaf(Player* this, PlayState* play);
extern s32 Player_UpperAction_Shovel(Player* this, PlayState* play);
extern s32 Player_UpperAction_SwitchHook(Player* this, PlayState* play);

// External custom item init functions (not declared in custom_items.h)
extern void Player_InitHyliasGraceIA(PlayState* play, Player* this);
extern void Player_InitZonaiPermafrostIA(PlayState* play, Player* this);
extern void Player_InitSwitchHookIA(PlayState* play, Player* this);
extern void Player_InitMogmaMittsIA(PlayState* play, Player* this);
extern void Player_InitWhipIA(PlayState* play, Player* this);
extern void Player_InitDominionRodIA(PlayState* play, Player* this);
extern void Player_InitTimeGateIA(PlayState* play, Player* this);
extern void Player_InitMinishCapIA(PlayState* play, Player* this);
extern void Player_InitLanternIA(PlayState* play, Player* this);
extern void Player_InitPokeballIA(PlayState* play, Player* this);

// ── Shared item action, dispatched by held ITEM (Skijer's NEI) ───────────────────────────────────
// SoH's PlayerItemAction space (0x00-0x7F, and heldItemAction is s8) is COMPLETELY full, so the
// Elemental Wand has no action of its own — it shares PLAYER_IA_UNUSED_5B with the Mario Mask.
// ExtPlayer_FindByIA returns the FIRST row with a matching ia, so two rows sharing one action would
// otherwise mean whichever is listed first silently steals the other's behavior.
//
// The fix is the same shape as Nei_ArmsHookVariant: keep ONE action, and branch inside on the held
// ITEM. Both rows point at these trampolines, so which row the search finds stops mattering — and
// each item gets its own behavior back. This is what makes the six rods implementable without
// widening heldItemAction to s16.
static s32 Nei_SharedIA_UpperAction(Player* this, PlayState* play) {
    if (this->heldItemId == ITEM_ELEMENTAL_WAND) {
        extern s32 Player_UpperAction_ElementalWand(Player * player, PlayState * play);
        return Player_UpperAction_ElementalWand(this, play);
    }
    // Mario Mask (and anything else that lands on this action): the generic aim/hold handler.
    return func_8083485C(this, play);
}

static void Nei_SharedIA_Init(PlayState* play, Player* this) {
    if (this->heldItemId == ITEM_ELEMENTAL_WAND) {
        extern void Player_InitElementalWandIA(PlayState * play, Player * player);
        Player_InitElementalWandIA(play, this);
        return;
    }
    Player_InitDefaultIA(play, this);
}

// (Sw97_PreferBow removed — Skijer's NEI. It existed to decide whether an SW97 arrow ITEM should
// fire from the bow or the slingshot, back when the element WAS the item on the button. The button
// now holds the real weapon, so the weapon is no longer ambiguous and the bow/slingshot elements are
// separate flags anyway.)

// Skijer's NEI: which hookshot-actor variant is firing, resolved from the HELD ITEM:
//   0 Hookshot   1 Longshot   2 Ultrashot (Longshot + ultrashotOwned)
//   3 Clawshot (Twilight clawshot MODE toggled on the hookshot/longshot)
//   4 Switch Hook (own slot; swaps positions, no damage)
// z_arms_hook.c calls this — it can't see the NEI item ids.
u8 Nei_ArmsHookVariant(Player* player) {
    extern u8 TwilightUpgrade_IsClawshotActive(void);

    if (player != NULL) {
        switch (player->heldItemId) {
            case ITEM_SWITCH_HOOK:
                return 4; // NEI_HOOK_VARIANT_SWITCHHOOK
            case ITEM_HOOKSHOT:
                return TwilightUpgrade_IsClawshotActive() ? 3 : 0;
            case ITEM_LONGSHOT:
                if (TwilightUpgrade_IsClawshotActive()) {
                    return 3; // the mode toggle wins over the Ultrashot unlock
                }
                return Nei_Save()->ultrashotOwned ? 2 : 1;
            default:
                break;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Custom item descriptor table — single source of truth for engine glue.
//
//   item       ITEM_xxx, or NEI_NO_ITEM for IA-only rows (no inventory item).
//   ia         PLAYER_IA_xxx (unique per row).
//   modelGroup PLAYER_MODELGROUP_xxx.
//   slot       page-2 inventory slot (SLOT_*), or NEI_NO_SLOT.
//   ageReq     AGE_REQ_* (AGE_REQ_NONE for slotless rows).
//   icon       page-2 icon texture (NULL = dynamic, getter handles it).
//   updateFn   upper-action update (func_8083485C = generic "no special update").
//   initFn     action init (Player_InitDefaultIA = generic).
//
// Only items whose IA is a *custom* action live here; vanilla-IA aliases are
// resolved separately in ExtPlayer_GetItemAction. Skijer's NEI
// ---------------------------------------------------------------------------
// Skijer's NEI — extra columns: drawFunc (rando GI 3D model), rg (RandomizerGet,
// NEI_NO_RG if non-uniform / none), and name strings (relocated from
// customItemMessages[] so each item lives in one row). Roc's Feather Skijer keeps
// rg=NEI_NO_RG: ITEM_ROCS_FEATHER_SKIJER maps to two RGs (progressive + vanilla),
// so its give/draw/name stay on the old per-RG path.
static const NeiItem sNeiItems[] = {
    // item                          ia                              modelGroup                  slot ageReq icon update
    // init                       drawFunc                          rg                       nameEn / nameFr / nameDe
    { ITEM_ROCS_FEATHER_SKIJER, PLAYER_IA_ROCS_FEATHER_SKIJER, PLAYER_MODELGROUP_DEFAULT, SLOT_ROCS, AGE_REQ_NONE,
      (void*)gItemIconRocsFeatherTex, func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG, NULL, NULL, NULL },
    { ITEM_ROCS_CAPE, PLAYER_IA_ROCS_CAPE, PLAYER_MODELGROUP_DEFAULT, SLOT_ROCS, AGE_REQ_NONE,
      (void*)gItemIconRocsCapeTex, func_8083485C, Player_InitDefaultIA, NULL, RG_ROCS_CAPE,
      "You got %rRoc's Cape%w!&This magical cape enhances&your jumping ability.^Now you can perform a&%gdouble jump%w "
      "in midair.&Press %y\xA1%w again while&jumping to go higher!",
      "Vous obtenez la %rCape de Roc%w!&Cette cape magique améliore&vos capacités de saut.^Vous pouvez maintenant "
      "effectuer&un %gdouble saut%w en l'air.&Appuyez sur %y\xA1%w en sautant&pour aller plus haut!",
      "Du hast %rRocs Umhang%w erhalten!&Dieser magische Umhang&verbessert deine Sprungkraft.^Du kannst nun "
      "einen&%gDoppelsprung%w in der Luft&ausführen. Drücke %y\xA1%w&erneut während du springst!" },
    // RETIRED: the Desire Sensor became the Sheikah Slate's Sensor rune, so RG_DESIRE_SENSOR now
    // grants a rune and its model/textbox live with the other runes. Row kept for icon lookups on
    // old saves; NEI_NO_SLOT = unreachable, NEI_NO_RG = off the give/draw/name path.
    { ITEM_DESIRE_SENSOR, PLAYER_IA_DESIRE_SENSOR, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      (void*)gItemIconDesireSensorTex, func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG, NULL, NULL, NULL },
    // RETIRED 2026-08-06: no inventory cell (41 belongs to the Phantom Hourglass); noclip moves
    // to the Soul spell (TODO). Row kept for icon/textbox lookups; NEI_NO_SLOT = unreachable.
    { ITEM_HYLIAS_GRACE, PLAYER_IA_HYLIAS_GRACE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      (void*)gItemIconHyliaGraceTex, func_8083485C, Player_InitHyliasGraceIA, Randomizer_DrawHyliaGrace,
      RG_HYLIAS_GRACE,
      "You got %pHylia's Grace%w!&A divine blessing that transforms&you into a %cfairy%w for 10 seconds.^Press "
      "%y\xA1%w "
      "to activate&(requires a %rFairy in a Bottle%w).^%yA%w = Ascend  %yB%w = Descend&%yL%w = Sprint&1 minute "
      "cooldown after use.",
      "Vous obtenez la %pGrâce d'Hylia%w!&Une bénédiction divine qui vous&transforme en %cfée%w pendant 10 "
      "secondes.^Appuyez sur %y\xA1%w pour activer&(nécessite une %rFée en Bouteille%w).^%yA%w = Monter  %yB%w = "
      "Descendre&%yL%w = Sprint&1 minute de recharge après utilisation.",
      "Du hast %pHylias Gnade%w erhalten!&Ein göttlicher Segen der dich&für 10 Sekunden in eine %cFee%w "
      "verwandelt.^Drücke %y\xA1%w zum Aktivieren&(benötigt eine %rFee in einer Flasche%w).^%yA%w = Aufsteigen  %yB%w "
      "= "
      "Absteigen&%yL%w = Sprinten&1 Minute Abklingzeit nach Nutzung." },
    { ITEM_ZONAI_PERMAFROST, PLAYER_IA_ZONAI_PERMAFROST, PLAYER_MODELGROUP_DEFAULT, SLOT_ZONAI_PERMAFROST, AGE_REQ_NONE,
      (void*)gItemIconZonaiPermafrostTex, func_8083485C, Player_InitZonaiPermafrostIA, Randomizer_DrawZonaiPermafrost,
      RG_ZONAI_PERMAFROST,
      "You got the %cZonai Timer%w!&Ancient Zonai technology that&freezes the flow of time itself.^Press %y\xA1%w to "
      "%gtoggle%w it.&%rAll enemies%w, %ypuzzle elements%w,&and even the %cday/night cycle%w&stop until you press "
      "again.^%g4 Magic%w to start, then %g1 Magic%w&every 10 frames. An empty meter&ends it. Move freely meanwhile.",
      "Vous obtenez le %cMinuteur Soneau%w!&Technologie ancienne des Soneau&qui gèle le flux du temps.^Appuyez sur "
      "%y\xA1%w pour l'%gactiver%w&ou l'arrêter. %rTous les ennemis%w,&%yéléments de puzzle%w et le %ccycle "
      "jour/nuit%w&s'arrêtent jusqu'au prochain appui.^%g4 Magie%w au départ, puis %g1 Magie%w&toutes les 10 frames. "
      "La jauge vide&y met fin. Bougez librement.",
      "Du hast den %cSonau-Zeitmesser%w!&Uralte Sonau-Technologie die&den Fluss der Zeit einfriert.^Drücke %y\xA1%w "
      "zum "
      "%gUmschalten%w.&%rAlle Feinde%w, %yRätsel-Elemente%w&und der %cTag/Nacht-Zyklus%w stehen&bis zum nächsten "
      "Druck still.^%g4 Magie%w zum Start, dann %g1 Magie%w&alle 10 Frames. Eine leere Leiste&beendet es." },
    { ITEM_DEMISE_DESTRUCTION, PLAYER_IA_DEMISE_DESTRUCTION, PLAYER_MODELGROUP_DEFAULT, SLOT_DEMISE_DESTRUCTION,
      AGE_REQ_NONE, (void*)gItemIconDemiseDestructionTex, func_8083485C, Player_InitDemiseDestructionIA,
      Randomizer_DrawDemiseDestruction, RG_DEMISE_DESTRUCTION,
      "You got %rDemise Destruction%w!&The dark power of the Demon King&Demise, sealed in this artifact.^Press "
      "%y\xA1%w "
      "to unleash a&devastating %rlightning explosion%w&that damages all enemies in&a %glarge radius%w around "
      "you.^%rHigh Magic cost%w.&Best saved for emergencies!&The ground itself trembles...",
      "Vous obtenez %rDestruction de l'Avatar%w!&Le pouvoir sombre du Roi Démon&Avatar, scellé dans cet "
      "artefact.^Appuyez sur %y\xA1%w pour déchaîner&une %rexplosion de foudre%w&dévastatrice qui blesse tous "
      "les&ennemis dans un %glarge rayon%w.^%rCoût élevé en Magie%w.&À garder pour les urgences!&La terre elle-même "
      "tremble...",
      "Du hast %rTodbringer Zerstörung%w!&Die dunkle Macht des Dämonenkönigs&Todbringer, versiegelt in "
      "diesem&Artefakt.^Drücke %y\xA1%w um eine verheerende&%rBlitz-Explosion%w zu entfesseln&die alle Feinde in "
      "einem&%ggroßen Radius%w um dich trifft.^%rHohe Magiekosten%w.&Am besten für Notfälle aufheben!&Der Boden selbst "
      "bebt..." },
    { ITEM_DEKU_LEAF, PLAYER_IA_DEKU_LEAF, PLAYER_MODELGROUP_DEFAULT, SLOT_DEKU_LEAF, AGE_REQ_CHILD,
      (void*)gItemIconDekuLeafTex, Player_UpperAction_DekuLeaf, Player_InitDefaultIA, Randomizer_DrawDekuLeaf,
      RG_DEKU_LEAF,
      "You got the %gDeku Leaf%w!&A giant leaf with powers&of the wind.^%yIn the air%w: Use it to glide&slowly and "
      "cover great&distances. Consumes magic.^%yOn the ground%w: Creates a gust&of wind that pushes objects&and "
      "enemies forward.",
      "Vous obtenez la %gFeuille Mojo%w!&Une feuille géante dotée&des pouvoirs du vent.^%yDans les airs%w: "
      "Planez&lentement sur de grandes&distances. Consomme de la magie.^%yAu sol%w: Crée une rafale&qui pousse les "
      "objets&et ennemis vers l'avant.",
      "Du hast das %gDeku-Blatt%w erhalten!&Ein Riesenblatt mit der&Kraft des Windes.^%yIn der Luft%w: Gleite "
      "langsam&und überbrücke große&Distanzen. Verbraucht Magie.^%yAm Boden%w: Erzeugt einen&Windstoß der Objekte "
      "und&Feinde nach vorne schiebt." },
    { ITEM_SWITCH_HOOK, PLAYER_IA_SWITCH_HOOK, PLAYER_MODELGROUP_HOOKSHOT, SLOT_SWITCH_HOOK, AGE_REQ_CHILD,
      (void*)gItemIconSwitchHookTex, Player_UpperAction_SwitchHook, Player_InitSwitchHookIA, Randomizer_DrawSwitchHook,
      RG_SWITCH_HOOK,
      "You got the %cSwitch Hook%w!&A magical hook that swaps&your position with targets.^Hold %y\xA1%w to "
      "aim,&release "
      "to fire.&%c\xA5%w = First-person mode^Swap places with pots, crates,&and certain enemies!&Non-swappable targets "
      "take damage.",
      "Vous obtenez le %cCrochet Échange%w!&Un crochet magique qui échange&votre position avec les cibles.^Maintenez "
      "%y\xA1%w pour viser,&relâchez pour tirer.&%c\xA5%w = Première personne^Échangez avec des pots, caisses,&et "
      "certains ennemis!&Les cibles non-échangeables subissent des dégâts.",
      "Du hast den %cWechselhaken%w!&Ein magischer Haken der deine&Position mit Zielen tauscht.^Halte %y\xA1%w zum "
      "Zielen,&lass los zum Feuern.&%c\xA5%w = Erste-Person^Tausche Plätze mit Töpfen, Kisten&und bestimmten "
      "Feinden!&Nicht-tauschbare Ziele nehmen Schaden." },
    { ITEM_MOGMA_MITTS, PLAYER_IA_MOGMA_MITTS, PLAYER_MODELGROUP_DEFAULT, SLOT_MOGMA_MITTS, AGE_REQ_NONE,
      (void*)gItemIconMogmaMittsTex, func_8083485C, Player_InitMogmaMittsIA, Randomizer_DrawMogmaMitts, RG_MOGMA_MITTS,
      "You got the %yMogma Mitts%w!&Claws of the underground.&Climb any wall! Uses %gMagic%w.",
      "Vous obtenez les %yGants Mogma%w!&Griffes souterraines.&Grimpez partout! Utilise de la %gMagie%w.",
      "Du hast die %yMogma-Klauen%w erhalten!&Klauen aus dem Untergrund.&Klettere überall! Verbraucht %gMagie%w." },
    { ITEM_GUST_JAR, PLAYER_IA_GUST_JAR, PLAYER_MODELGROUP_DEFAULT, SLOT_GUST_JAR, AGE_REQ_CHILD,
      (void*)gItemIconGustJarTex, func_8083485C, Player_InitGustJarIA, Randomizer_DrawGustJar, RG_GUST_JAR,
      "You got the %gGust Jar%w!&A vessel containing&ancient winds.^%ySuction mode%w: Hold %y\xA1%w&to absorb objects, "
      "enemies&and environmental elements.^%yCapture mode%w: Absorb fire,&ice or electricity to store&special "
      "ammunition.^%yShoot mode%w: Release %y\xA1%w to&fire what you captured.&%c\xA5%w = First-person mode",
      "Vous obtenez le %gPot Magique%w!&Un récipient contenant&des vents anciens.^%yMode aspiration%w: Maintenez "
      "%y\xA1%w&pour absorber objets, ennemis&et éléments environnementaux.^%yMode capture%w: Absorbez feu,&glace ou "
      "électricité comme&munition spéciale.^%yMode tir%w: Relâchez %y\xA1%w pour&tirer ce que vous avez "
      "capturé.&%c\xA5%w = Première personne",
      "Du hast den %gMagischen Krug%w!&Ein Gefäß mit uralten&Winden.^%yAnsaugmodus%w: Halte %y\xA1%w&um Objekte, "
      "Feinde "
      "und&Umgebungselemente anzusaugen.^%yFangmodus%w: Sauge Feuer,&Eis oder Elektrizität auf&als spezielle "
      "Munition.^%ySchussmodus%w: Lass %y\xA1%w los&um das Gefangene zu feuern.&%c\xA5%w = Erste-Person" },
    { ITEM_BALL_AND_CHAIN, PLAYER_IA_BALL_AND_CHAIN, PLAYER_MODELGROUP_DEFAULT, SLOT_BALL_AND_CHAIN, AGE_REQ_ADULT,
      (void*)gItemIconBallAndChainTex, func_8083485C, Player_InitBallAndChainIA, Randomizer_DrawBallAndChain,
      RG_BALL_AND_CHAIN,
      "You got the %yBall and Chain%w!&A heavy weapon from the&snow palace.^Hold %y\xA1%w to charge,&release to "
      "throw.&Crush ice and enemies!^With %g\xA4%w it homes in&on the enemy automatically.&Breaks %rRed "
      "Ice%w!^%rNote%w: Your speed is reduced&while it's equipped.",
      "Vous obtenez le %yBoulet%w!&Une arme lourde du palais&des neiges.^Maintenez %y\xA1%w pour charger,&relâchez "
      "pour "
      "lancer.&Écrasez glace et ennemis!^Avec %g\xA4%w il suit&automatiquement l'ennemi.&Brise la %rGlace "
      "Rouge%w!^%rNote%w: Votre vitesse est réduite&tant qu'il est équipé.",
      "Du hast die %yKettenkugel%w!&Eine schwere Waffe aus dem&Schneepalast.^Halte %y\xA1%w zum Aufladen,&lass los zum "
      "Werfen.&Zerschmettere Eis und Feinde!^Mit %g\xA4%w verfolgt sie&automatisch den Feind.&Zerbricht %rRotes "
      "Eis%w!^%rHinweis%w: Deine Geschwindigkeit&ist reduziert während sie&ausgerüstet ist." },
    { ITEM_WHIP, PLAYER_IA_WHIP, PLAYER_MODELGROUP_DEFAULT, SLOT_WHIP, AGE_REQ_NONE, (void*)gItemIconWhipTex,
      func_8083485C, Player_InitWhipIA, Randomizer_DrawWhip, RG_WHIP,
      "You got the %yWhip%w!&A versatile tool for combat&and exploration.^Press %y\xA1%w to lash forward.&It latches "
      "onto beams and bars&for pendulum swinging.^%ySwinging%w: Use the stick to&control the pendulum.&Release to "
      "launch with momentum!^%yCombat%w: Paralyze enemies,&pull shields, and disarm.&Also grabs items!",
      "Vous obtenez le %yFouet%w!&Un outil polyvalent pour le combat&et l'exploration.^Appuyez sur %y\xA1%w pour "
      "fouetter.&S'accroche aux poutres et barres&pour se balancer en pendule.^%yBalancement%w: Utilisez le stick&pour "
      "contrôler le pendule.&Relâchez pour vous lancer!^%yCombat%w: Paralysez les ennemis,&tirez les boucliers et "
      "désarmez.&Attrape aussi des objets!",
      "Du hast die %yPeitsche%w!&Ein vielseitiges Werkzeug für&Kampf und Erkundung.^Drücke %y\xA1%w zum Schlagen.&Hakt "
      "sich an Balken und Stangen&zum Pendelschwingen ein.^%ySchwingen%w: Nutze den Stick um&das Pendel zu "
      "steuern.&Lass los für Schwung-Start!^%yKampf%w: Lähme Feinde,&ziehe Schilde weg und entwaffne.&Greift auch "
      "Items!" },
    { ITEM_SPINNER, PLAYER_IA_SPINNER, PLAYER_MODELGROUP_DEFAULT, SLOT_SPINNER, AGE_REQ_NONE,
      (void*)gItemIconSpinnerTex, func_8083485C, Player_InitSpinnerIA, Randomizer_DrawSpinner, RG_SPINNER,
      "You got the %ySpinner%w!&Ancient technology from the&desert sands.^%rHold%w %y\xA1%w to charge, then let&go to "
      "ride. The longer the charge,&the faster you go.^Let go while %g\xA4%w-targeting for&a homing dash attack. "
      "Breaks rocks!",
      "Vous obtenez la %yToupie%w!&Technologie ancienne des&sables du désert.^%rMaintenez%w %y\xA1%w pour charger, "
      "puis&relâchez pour rouler. Plus la charge&est longue, plus vous filez vite.^Relâchez en visant avec %g\xA4%w "
      "pour&une attaque guidée. Brise les rochers!",
      "Du hast den %yKreisel%w!&Uralte Technologie aus dem&Wüstensand.^%rHalte%w %y\xA1%w zum Aufladen und&lass los "
      "zum "
      "Fahren. Je länger&die Ladung, desto schneller.^Loslassen beim %g\xA4%w-Zielen gibt&einen Verfolgungs-Angriff. "
      "Zerbricht Felsen!" },
    { ITEM_CANE_OF_SOMARIA, PLAYER_IA_CANE_OF_SOMARIA, PLAYER_MODELGROUP_BGS, SLOT_CANE_OF_SOMARIA, AGE_REQ_NONE,
      (void*)gItemIconCaneOfSomariaTex, Player_UpperAction_CaneOfSomaria, Player_InitCaneOfSomariaIA,
      Randomizer_DrawCaneOfSomaria, RG_CANE_OF_SOMARIA,
      "You got the %rCane of Somaria%w!&A wand that creates magical&blocks out of thin air.^Press %y\x9F%w on its "
      "pause "
      "cell to&cycle between the canes you own.^%y\xA1%w draws the cane, then casts.&%y\xA3%w and %y\xA2%w step to the "
      "next&summon: %rStatue%w, %rBlock%w, %rPlatform%w.^Use them for switches, to block&enemies, or to climb.",
      "Vous obtenez la %rCanne de Somaria%w!&Une baguette qui crée des&blocs magiques de nulle part.^%y\x9F%w sur sa "
      "case du menu pause&change de canne.^%y\xA1%w sort la canne, puis lance.&%y\xA3%w et %y\xA2%w passent à "
      "l'invocation&suivante: %rStatue%w, %rBloc%w, %rPlateforme%w.^Pour les interrupteurs, bloquer&les ennemis ou "
      "grimper.",
      "Du hast den %rStab von Somaria%w!&Ein Stab der magische Blöcke&aus dem Nichts erschafft.^%y\x9F%w auf seiner "
      "Menüzelle wechselt&zwischen deinen Stäben.^%y\xA1%w zieht den Stab, dann wirkt er.&%y\xA3%w und %y\xA2%w "
      "schalten zur nächsten&Beschwörung: %rStatue%w, %rBlock%w, %rPlattform%w.^Für Schalter, zum Blockieren&oder zum "
      "Klettern." },
    // 2026-08-06: the rod rides the SHOVEL cell (46) as a wheel entry; cell 47 is the Rod of
    // Seasons. A dominion pickup lands on the shared cell; the wheel flips between the two.
    { ITEM_DOMINION_ROD, PLAYER_IA_DOMINION_ROD, PLAYER_MODELGROUP_DEFAULT, SLOT_SHOVEL, AGE_REQ_NONE,
      (void*)gItemIconDominionRodTex, func_8083485C, Player_InitDominionRodIA, Randomizer_DrawDominionRod,
      RG_DOMINION_ROD,
      "You got the %pDominion Rod%w!&An ancient artifact that can&possess and control enemies.^Press %y\xA1%w to fire "
      "a "
      "golden orb.&It can possess: %rBeamos%w,&%yArmos%w, and %cAnubis%w.^Once possessed, the enemy will&%gmimic your "
      "movements%w!&Walk to make it walk,&attack to make it attack.^Uses %gMagic%w while controlling.",
      "Vous obtenez la %pBaguette des Animes%w!&Un artefact ancien qui peut&posséder et contrôler les ennemis.^Appuyez "
      "sur %y\xA1%w pour tirer un&orbe doré. Il peut posséder:&%rBeamos%w, %yArmos%w et %cAnubis%w.^Une fois possédé, "
      "l'ennemi va&%gimiter vos mouvements%w!&Marchez pour le faire marcher,&attaquez pour le faire attaquer.^Utilise "
      "de la %gMagie%w pendant&le contrôle.",
      "Du hast den %pKopierstab%w!&Ein uraltes Artefakt das Feinde&besitzen und kontrollieren kann.^Drücke %y\xA1%w um "
      "einen goldenen Orb&zu feuern. Er kann besitzen:&%rBeamos%w, %yArmos%w und %cAnubis%w.^Einmal besessen, wird der "
      "Feind&%gdeine Bewegungen imitieren%w!&Laufe um ihn laufen zu lassen,&greife an um ihn angreifen zu "
      "lassen.^Verbraucht %gMagie%w beim Kontrollieren." },
    { ITEM_TIME_GATE, PLAYER_IA_TIME_GATE, PLAYER_MODELGROUP_DEFAULT, SLOT_TIME_GATE, AGE_REQ_NONE,
      (void*)gItemIconTimeGateTex, func_8083485C, Player_InitTimeGateIA, Randomizer_DrawTimeGate, RG_TIME_GATE,
      "You got the %cTime Gate%w!&A portable door through the ages,&the power of the Temple of Time&in your "
      "hands.^Press %y\xA1%w to activate.&A prompt will ask: %g\"Travel&through time?\"%w^Select %yYes%w to switch "
      "between&%rChild%w and %gAdult%w Link&anywhere in the world!^Costs %g48 Magic%w per use.",
      "Vous obtenez la %cPorte du Temps%w!&Une porte portable à travers les&âges, le pouvoir du Temple du Temps&dans "
      "vos mains.^Appuyez sur %y\xA1%w pour activer.&Une question apparaît: %g\"Voyager&dans le "
      "temps?\"%w^Sélectionnez "
      "%yOui%w pour passer&entre Link %rEnfant%w et %gAdulte%w&n'importe où!^Coûte %g48 Magie%w par utilisation.",
      "Du hast das %cZeittor%w!&Eine tragbare Tür durch die Zeit,&die Macht des Zeitturms in&deinen Händen.^Drücke "
      "%y\xA1%w zum Aktivieren.&Eine Frage erscheint: %g\"Durch&die Zeit reisen?\"%w^Wähle %yJa%w um zwischen&%rKind%w "
      "und %gErwachsenem%w Link&überall zu wechseln!^Kostet %g48 Magie%w pro Nutzung." },
    // Bomb Arrows keeps every column EXCEPT the slot: it is the 7th value of the bow's element flag
    // (SW97_ELEM_BOMB) and owns no inventory cell, so .slot is NEI_NO_SLOT. The IA/upper-action/init
    // are still reached — ExtPlayer_GetItemAction returns PLAYER_IA_BOMB_ARROWS for a bow whose flag
    // is BOMB. NEI_NO_SLOT also makes ExtInv_GetItemSlot() return 0xFF, so every legacy
    // `baSlot != 0xFF` guard fails safe. Skijer's NEI
    { ITEM_BOMB_ARROWS, PLAYER_IA_BOMB_ARROWS, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_ADULT,
      (void*)gItemIconBombArrowsTex, Player_UpperAction_BombArrows, Player_InitBombArrowsIA, Randomizer_DrawBombArrows,
      RG_BOMB_ARROWS,
      "You got %rBomb Arrows%w!&An explosive combination.^Requires %yArrows%w and %rBombs%w.&Use %y\xA1%w to enter "
      "first-person&mode and aim.^The arrow explodes on impact.&Consumes %y1 arrow%w + %r1 bomb%w&per shot.",
      "Vous obtenez les %rFlèches-Bombes%w!&Une combinaison explosive.^Nécessite des %yFlèches%w et "
      "%rBombes%w.&Utilisez %y\xA1%w pour entrer en&première personne et viser.^La flèche explose à l'impact.&Consomme "
      "%y1 flèche%w + %r1 bombe%w&par tir.",
      "Du hast %rBombenpfeile%w!&Eine explosive Kombination.^Benötigt %yPfeile%w und %rBomben%w.&Benutze %y\xA1%w für "
      "Erste-Person&Modus und zielen.^Der Pfeil explodiert beim&Aufprall. Verbraucht %y1 Pfeil%w&+ %r1 Bombe%w pro "
      "Schuss." },
    // Elemental Wand — six rods sharing page-2 slot 27 (the cell Bomb Arrows vacated). One row, one
    // IA: the active rod is NeiSaveData.wandMode. Per-rod behavior is a separate task, so the update
    // func is the generic aim handler and the init is a stub. Skijer's NEI
    { ITEM_ELEMENTAL_WAND, PLAYER_IA_ELEMENTAL_WAND, PLAYER_MODELGROUP_DEFAULT, SLOT_ELEMENTAL_WAND, AGE_REQ_NONE,
      (void*)gItemIconElementalWandTex, Nei_SharedIA_UpperAction, Nei_SharedIA_Init, Randomizer_DrawElementalWand,
      RG_ELEMENTAL_WAND,
      "You got the %cElemental Wand%w!&Six rods in one.^%rHold%w %y\xA2%w for the rod wheel;&the stick picks and "
      "letting go&confirms.^%y\xA1%w casts the active rod.",
      "Vous obtenez la %cBaguette&Élémentaire%w!&Six sceptres en un.^%rMaintenez%w %y\xA2%w pour la roue des&sceptres; "
      "le stick choisit, relâcher&confirme.^%y\xA1%w lance le sceptre actif.",
      "Du hast den %cElementarstab%w!&Sechs Stäbe in einem.^%rHalte%w %y\xA2%w für das Stab-Rad;&der Stick wählt, "
      "Loslassen bestätigt.^%y\xA1%w wirkt den aktiven Stab." },
    // Rods use the BGS (two-handed) model group + sword mechanics for charge attacks.
    { ITEM_ROD_FIRE, PLAYER_IA_ROD_FIRE, PLAYER_MODELGROUP_BGS, SLOT_FIRE_ROD, AGE_REQ_NONE, (void*)gItemIconFireRodTex,
      Player_UpperAction_Sword, Player_InitFireRodIA, Randomizer_DrawFireRod, RG_FIRE_ROD,
      "You got the %rFire Rod%w!&A magical weapon that channels&the power of fire.^%yBasic attacks%w:&Slash = 3 "
      "fireballs&Stab = 1 fireball&Jump = Flamethrower down^%ySpecial attacks%w:&Spin = Expanding fire wave&Hold "
      "%y\xA1%w = Charge attack&%c\xA5%w = First-person mode^%rWarning%w: Without magic, the&fire will burn YOU. Make "
      "sure&you have enough magic!",
      "Vous obtenez la %rBaguette de Feu%w!&Une arme magique qui canalise&le pouvoir du feu.^%yAttaques de "
      "base%w:&Taille = 3 boules de feu&Estoc = 1 boule de feu&Saut = Lance-flammes^%yAttaques spéciales%w:&Tourbillon "
      "= Vague de feu&Maintenez %y\xA1%w = Charge&%c\xA5%w = Première personne^%rAttention%w: Sans magie, le feu&VOUS "
      "brûlera. Assurez-vous&d'avoir assez de magie!",
      "Du hast den %rFeuerstab%w!&Eine magische Waffe mit der&Kraft des Feuers.^%yBasisangriffe%w:&Hieb = 3 "
      "Feuerbälle&Stoß = 1 Feuerball&Sprung = Flammenwerfer^%ySpezialangriffe%w:&Wirbelattacke = Feuerwelle&Halte "
      "%y\xA1%w = Aufladen&%c\xA5%w = Erste-Person^%rWarnung%w: Ohne Magie verbrennt&das Feuer DICH. Achte auf&genug "
      "Magie!" },
    { ITEM_ROD_ICE, PLAYER_IA_ROD_ICE, PLAYER_MODELGROUP_BGS, SLOT_ICE_ROD, AGE_REQ_NONE, (void*)gItemIconIceRodTex,
      Player_UpperAction_Sword, Player_InitIceRodIA, Randomizer_DrawIceRod, RG_ICE_ROD,
      "You got the %bIce Rod%w!&A magical weapon that channels&the power of ice.^%yBasic attacks%w:&Slash = 3 ice "
      "projectiles&Stab = 1 ice projectile&Jump = Freezing blast down^%ySpecial attacks%w:&Spin = Expanding ice "
      "wave&Hold %y\xA1%w = Charge attack&%c\xA5%w = First-person mode^%rWarning%w: Without magic, the&ice will freeze "
      "YOU. Make sure&you have enough magic!",
      "Vous obtenez la %bBaguette de Glace%w!&Une arme magique qui canalise&le pouvoir de la glace.^%yAttaques de "
      "base%w:&Taille = 3 projectiles de glace&Estoc = 1 projectile de glace&Saut = Souffle glacial^%yAttaques "
      "spéciales%w:&Tourbillon = Vague de glace&Maintenez %y\xA1%w = Charge&%c\xA5%w = Première "
      "personne^%rAttention%w: Sans magie, la glace&VOUS gèlera. Assurez-vous&d'avoir assez de magie!",
      "Du hast den %bEisstab%w!&Eine magische Waffe mit der&Kraft des Eises.^%yBasisangriffe%w:&Hieb = 3 "
      "Eisprojektile&Stoß = 1 Eisprojektil&Sprung = Eisstrahl^%ySpezialangriffe%w:&Wirbelattacke = Eiswelle&Halte "
      "%y\xA1%w = Aufladen&%c\xA5%w = Erste-Person^%rWarnung%w: Ohne Magie friert&das Eis DICH ein. Achte auf&genug "
      "Magie!" },
    { ITEM_ROD_LIGHT, PLAYER_IA_ROD_LIGHT, PLAYER_MODELGROUP_BGS, SLOT_LIGHT_ROD, AGE_REQ_NONE,
      (void*)gItemIconLightRodTex, Player_UpperAction_Sword, Player_InitLightRodIA, Randomizer_DrawLightRod,
      RG_LIGHT_ROD,
      "You got the %yLight Rod%w!&A magical weapon that channels&the power of lightning.^%yBasic attacks%w:&Slash = 3 "
      "lightning bolts&Stab = 1 lightning bolt&Jump = Electric discharge^%ySpecial attacks%w:&Spin = Expanding "
      "electric wave&Hold %y\xA1%w = Charge attack&%c\xA5%w = First-person mode^%rWarning%w: Without magic, "
      "the&lightning will shock YOU.&Make sure you have enough magic!",
      "Vous obtenez la %yBaguette de Lumière%w!&Une arme magique qui canalise&le pouvoir de la foudre.^%yAttaques de "
      "base%w:&Taille = 3 éclairs en éventail&Estoc = 1 éclair direct&Saut = Décharge électrique^%yAttaques "
      "spéciales%w:&Tourbillon = Vague électrique&Maintenez %y\xA1%w = Charge&%c\xA5%w = Première "
      "personne^%rAttention%w: Sans magie, la foudre&VOUS électrocutera. Assurez-vous&d'avoir assez de magie!",
      "Du hast den %yLichtstab%w!&Eine magische Waffe mit der&Kraft des Blitzes.^%yBasisangriffe%w:&Hieb = 3 Blitze im "
      "Bogen&Stoß = 1 direkter Blitz&Sprung = Elektrische Entladung^%ySpezialangriffe%w:&Wirbelattacke = "
      "Elektrowelle&Halte %y\xA1%w = Aufladen&%c\xA5%w = Erste-Person^%rWarnung%w: Ohne Magie trifft&der Blitz DICH. "
      "Achte auf&genug Magie!" },
    { ITEM_BEETLE, PLAYER_IA_BEETLE, PLAYER_MODELGROUP_DEFAULT, SLOT_BEETLE, AGE_REQ_ADULT, (void*)gItemIconBeetleTex,
      Player_UpperAction_Beetle, Player_InitBeetleIA, Randomizer_DrawBeetle, RG_BEETLE,
      "You got the %gBeetle%w!&A remote-controlled mechanical&insect from ancient times.^Hold %y\xA1%w to aim, release "
      "to&launch. %yAnalog Stick%w steers,&%y\x9F%w boosts, %g\xA4%w locks on.^%y\xA0%w hands control back: the beetle&"
      "flies home, or dives at a locked&target first.^The camera follows it while you fly.",
      "Vous obtenez le %gScarabée%w!&Un insecte mécanique télécommandé&des temps anciens.^Maintenez %y\xA1%w pour "
      "viser,&relâchez pour lancer. Le %yStick%w dirige,&%y\x9F%w accélère, %g\xA4%w cible.^%y\xA0%w vous rend le "
      "contrôle: il rentre,&ou fonce d'abord sur la cible.^La caméra le suit pendant le vol.",
      "Du hast den %gKäfer%w erhalten!&Ein ferngesteuertes mechanisches&Insekt aus alter Zeit.^Halte %y\xA1%w zum "
      "Zielen, lass los&zum Starten. %yAnalog-Stick%w steuert,&%y\x9F%w beschleunigt, %g\xA4%w zielt.^%y\xA0%w gibt "
      "die "
      "Kontrolle zurück: er&fliegt heim, oder stürzt vorher&auf das erfasste Ziel." },
    { ITEM_SHOVEL, PLAYER_IA_SHOVEL, PLAYER_MODELGROUP_DEFAULT, SLOT_SHOVEL, AGE_REQ_NONE, (void*)gItemIconShovelTex,
      Player_UpperAction_Shovel, Player_InitDefaultIA, Randomizer_DrawShovel, RG_SHOVEL,
      "You got the %yShovel%w!&A reliable tool for&excavation.^Use %y\xA1%w on soft soil&to dig and find "
      "hidden&treasures.^It can also reveal secret&%gGrottos%w and damage&buried enemies!",
      "Vous obtenez la %yPelle%w!&Un outil fiable pour&l'excavation.^Utilisez %y\xA1%w sur terre&meuble pour creuser "
      "et&trouver des trésors cachés.^Elle peut aussi révéler des&%gGrottes secrètes%w et blesser&les ennemis "
      "enterrés!",
      "Du hast die %ySchaufel%w!&Ein zuverlässiges Werkzeug&zum Graben.^Benutze %y\xA1%w auf weichem&Boden um zu "
      "graben "
      "und&verborgene Schätze zu finden.^Sie kann auch geheime&%gGrotten%w aufdecken und&vergrabene Feinde "
      "verletzen!" },
    { ITEM_MINISH_CAP, PLAYER_IA_MINISH_CAP, PLAYER_MODELGROUP_DEFAULT, SLOT_MINISH_CAP, AGE_REQ_CHILD,
      (void*)gItemIconMinishCapTex, func_8083485C, Player_InitMinishCapIA, Randomizer_DrawMinishCap, RG_MINISH_CAP,
      "You got %pThe Minish Cap%w!&%y\xA1%w by a pod soil opens the&fast travel map.^%y\xA1%w away from one shrinks "
      "you "
      "to&%gMinish size%w - small enough for&crawlspaces, and slower.&Press again, or load a scene, to grow.",
      "Vous obtenez %pla Casquette Minish%w!&%y\xA1%w près d'une terre meuble ouvre&la carte de voyage "
      "rapide.^%y\xA1%w "
      "ailleurs vous réduit à la&%gtaille Minish%w - assez petit pour&les passages étroits, mais plus lent.",
      "Du hast %pdie Minish-Mütze%w!&%y\xA1%w an einem Pod Soil öffnet die&Schnellreise-Karte.^%y\xA1%w anderswo "
      "schrumpft dich auf&%gMinish-Größe%w - klein genug für&Kriechgänge, dafür langsamer." },
    // Lantern: icon is dynamic (chosen by fire type) -> NULL, getter handles it. Skijer's NEI
    { ITEM_LANTERN, PLAYER_IA_LANTERN, PLAYER_MODELGROUP_DEFAULT, SLOT_LANTERN, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitLanternIA, Randomizer_DrawLantern, RG_LANTERN,
      "You got the %yLantern%w!&Catch fire from torches and&use it to light your way!",
      "Vous obtenez la %yLanterne%w!&Capturez le feu des torches et&utilisez-le pour éclairer votre chemin!",
      "Du hast die %yLaterne%w erhalten!&Fang Feuer von Fackeln und&nutze es um deinen Weg zu erleuchten!" },
    // 2026-08-06: the Pokeball left page 2 (cell 44 = Shadow Crystal) for the Crossover Items page
    // (Pikachu form). NEI_NO_SLOT keeps the row for icon/textbox lookups only.
    { ITEM_POKEBALL, PLAYER_IA_POKEBALL, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      (void*)gItemIconPokeballTex, func_8083485C, Player_InitPokeballIA, Randomizer_DrawPokeball, RG_POKEBALL,
      "You got the %yPoké Ball%w!&Use it to give orders to&a transformed Pikachu."
      "^%y\x9F%w combo  %y\xA0%w Thunder Jolt&Stick+%y\x9F%w/%y\xA0%w: smash / special&%y\xA2%w crouch  %y\xA3%w "
      "bubble shield&%y\xA1%w-buttons: special items",
      "Vous obtenez la %yPoké Ball%w!&Donnez des ordres à un&Pikachu transformé."
      "^%y\x9F%w combo  %y\xA0%w Tonnerre&Stick+%y\x9F%w/%y\xA0%w: smash / spécial&%y\xA2%w accroupi  %y\xA3%w "
      "bouclier&%y\xA1%w: objets spéciaux",
      "Du hast den %yPokéball%w erhalten!&Damit gibst du einem&verwandelten Pikachu Befehle."
      "^%y\x9F%w Combo  %y\xA0%w Donner-Schock&Stick+%y\x9F%w/%y\xA0%w: Smash / Special&%y\xA2%w Hocken  %y\xA3%w "
      "Blasen-Schild&%y\xA1%w-Tasten: Special-Items" },
    // Mario Mask — claims the formerly-reserved PLAYER_IA_UNUSED_5B row. Slotless
    // on purpose: page 2 is full (24/24), and this item is not C-button usable.
    // Receiving it sets RAND_INF_OBTAINED_MARIO_MASK, which is what unlocks
    // MARIO MODE in the Crossover Items form selector. Skijer's NEI
    // Shares PLAYER_IA_UNUSED_5B with the Elemental Wand (SoH has no free action left), so it uses
    // the same trampolines — they branch on the held ITEM, which is what keeps the two apart.
    { ITEM_MARIO_MASK, PLAYER_IA_UNUSED_5B, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      (void*)gItemIconMarioMaskTex, Nei_SharedIA_UpperAction, Nei_SharedIA_Init, Randomizer_DrawMarioMask,
      RG_MARIO_MASK,
      "You got the %rMario Mask%w!&\"I must save the princess...\"^Equip %gMARIO MODE%w from the&equipment subscreen "
      "to&become Mario.",
      "Vous obtenez le %rMasque de Mario%w!&\"Je dois sauver la princesse...\"^Équipez %gMARIO MODE%w depuis&le "
      "sous-écran d'équipement&pour devenir Mario.",
      "Du hast die %rMario-Maske%w erhalten!&\"Ich muss die Prinzessin retten...\"^Rüste %gMARIO MODE%w "
      "im&Ausrüstungsmenü aus,&um Mario zu werden." },
    // Bottle with Magic Mushroom — bottle behavior (drop on B-swing via vanilla path). Give stays on old path
    // (bottle-loop before the switch).
    { ITEM_BOTTLE_WITH_MAGIC_MUSHROOM, PLAYER_IA_BOTTLE_MAGIC_MUSHROOM, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT,
      AGE_REQ_NONE, NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawBottleWithMagicMushroom,
      RG_BOTTLE_WITH_MAGIC_MUSHROOM,
      "You got a %gBottle with Magic Mushroom%w!&A fragrant Termina mushroom plucked&by the keen nose of the Mask of "
      "Scents.^Stored in an empty bottle.&Drop it later for unknown effects -&or simply admire the catch.",
      "Vous obtenez une %gFiole avec Champignon Magique%w!&Un champignon parfumé de Termina,&flairé par le Masque des "
      "Odeurs.^Stocké dans une fiole vide.&À déposer plus tard pour des effets&inconnus - ou à contempler.",
      "Du hast eine %gFlasche mit Zauberpilz%w!&Ein duftender Termina-Pilz, geschnüffelt&von der Geruchsmaske.^In "
      "einer leeren Flasche aufbewahrt.&Lass ihn später fallen für unbekannte&Effekte - oder bewundere ihn." },

    // MM bottle-content custom items (Bottle Randomizer, Skijer's NEI). Standalone custom items —
    // icon is dynamic (mm.o2r, resolved in ExtInv_GetItemIcon), behavior dispatched from
    // mm_bottles_behavior when used. Generic no-op IA + no get-item model yet (placeholder),
    // not a rando item yet (NEI_NO_RG). Stored directly in SLOT_BOTTLE_* by the wheel. (Chateau
    // Romani 0xB6 + Magic Mushroom 0xDD already exist and keep their own rows.)
    { ITEM_GOLD_DUST, PLAYER_IA_BOTTLE_GOLD_DUST, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got a %gBottle with Gold Dust%w!&Rare Termina powder, prized by smiths.", NULL, NULL },
    { ITEM_HOT_SPRING_WATER, PLAYER_IA_BOTTLE_HOT_SPRING_WATER, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      NULL, func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got %gBottled Hot Spring Water%w!&Warm spring water from Termina.", NULL, NULL },
    { ITEM_DEKU_PRINCESS, PLAYER_IA_BOTTLE_DEKU_PRINCESS, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got the %gDeku Princess%w!&The Deku King's daughter, safe in a bottle.", NULL, NULL },
    { ITEM_SEAHORSE, PLAYER_IA_BOTTLE_SEAHORSE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got a %gBottled Seahorse%w!&A loyal Great Bay companion.", NULL, NULL },
    { ITEM_SPRING_WATER, PLAYER_IA_BOTTLE_SPRING_WATER, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got %gBottled Spring Water%w!&Cool, clear spring water.", NULL, NULL },
    { ITEM_ZORA_EGG, PLAYER_IA_BOTTLE_ZORA_EGG, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got a %gZora Egg%w!&A fragile egg kept safe in a bottle.", NULL, NULL },
    { ITEM_HYLIAN_LOACH, PLAYER_IA_BOTTLE_HYLIAN_LOACH, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got the %gHylian Loach%w!&A rare fish prized by anglers.", NULL, NULL },
    { ITEM_OBABA_DRINK, PLAYER_IA_BOTTLE_OBABA_DRINK, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, NULL, NEI_NO_RG,
      "You got %gObaba's Special Drink%w!&A peculiar Termina brew.", NULL, NULL },

    // Bottle Randomizer extra items: Net + Bottomless Bottle (occupy SLOT_BOTTLE_3/4). Icons from
    // soh.otr (icon_item_custom). The empty Bottomless Bottle behaves as a bottle via the IA alias
    // in ExtPlayer_GetItemAction (PLAYER_IA_BOTTLE); when filled, the slot holds the content id.
    // rg wired to the real rando items (RG_NET / RG_BOTTOMLESS_BOTTLE) so GetCustomItemMessage's
    // Nei_FindByRg fallback serves these textbox strings. The give does NOT flow through the
    // registry-default ExtInv arm — randomizer.cpp has explicit cases calling Bottle_Set*Owned.
    { ITEM_NET, PLAYER_IA_NET, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, (void*)gItemIconNetTex,
      Player_UpperAction_Net, Player_InitDefaultIA, NULL, RG_NET,
      "You got the %gNet%w!&Spin to scoop things in a wider radius.",
      "Vous obtenez le %gFilet%w!&Tournoyez pour ramasser les objets&dans un plus grand rayon.",
      "Du hast das %gNetz%w!&Wirble, um Dinge in größerem&Umkreis einzusammeln." },
    { ITEM_BOTTOMLESS_BOTTLE, PLAYER_IA_BOTTOMLESS_BOTTLE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      (void*)gItemIconBottomlessBottleTex, func_8083485C, Player_InitDefaultIA, NULL, RG_BOTTOMLESS_BOTTLE,
      "You got the %gBottomless Bottle%w!&Its contents multiply with use.",
      "Vous obtenez la %gBouteille sans Fond%w!&Son contenu se multiplie à l'usage.",
      "Du hast die %gBodenlose Flasche%w!&Ihr Inhalt vermehrt sich&beim Gebrauch." },

    // MM Mask IAs (all no-op: default model, generic update + init). Page-3 slots + icons stay on gPage3Mask* tables.
    // All 24 share Randomizer_DrawMmMask (dispatches by RG internally). Names relocated from customItemMessages[].
    { ITEM_MM_MASK_POSTMAN, PLAYER_IA_MM_MASK_POSTMAN, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_POSTMAN,
      "You got the %yPostman's Hat%w!&The official cap of Termina's&most punctual courier.^Equip from the mask "
      "page.^Walk up to any %gunlocked mailbox%w&and press %y\xA0%w to open the&%cMailbox Warp Menu%w - fast travel&to "
      "any other unlocked mailbox.",
      "Vous obtenez le %yChapeau du Facteur%w!&Le képi officiel du courrier le&plus ponctuel de Termina.^Équipez "
      "depuis la page des masques.^Approchez n'importe quelle %gboîte aux&lettres débloquée%w et %y\xA0%w pour "
      "ouvrir&le %cMenu de Téléportation%w - voyage&rapide vers toute autre boîte.",
      "Du hast den %yBriefträgerhut%w!&Die offizielle Mütze von Terminas&pünktlichstem Boten.^Aufsetzen auf der "
      "Maskenseite.^Geh zu einem %gfreigeschalteten Briefkasten%w&und drücke %y\xA0%w für "
      "das&%cBriefkasten-Warp-Menü%w - Schnellreise&zu jedem anderen freigeschalteten Briefkasten." },
    { ITEM_MM_MASK_ALL_NIGHT, PLAYER_IA_MM_MASK_ALL_NIGHT, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_ALL_NIGHT,
      "You got the %yAll-Night Mask%w!&A mask said to grant insomnia&and the gift of seeing in the dark.^Equip from "
      "the mask page.^While worn during %gdaytime%w, all&%cnight-only Gold Skulltulas%w spawn&as if it were night - "
      "Graveyard,&Zora's Fountain, Gerudo Fortress,&Kakariko, and Lon Lon Ranch.",
      "Vous obtenez le %yMasque de Nuit%w!&Un masque qui octroierait l'insomnie&et le don de voir dans "
      "l'obscurité.^Équipez depuis la page des masques.^Porté de %gjour%w, toutes les&%cSkulltulas d'Or de nuit%w "
      "apparaissent&comme s'il faisait nuit - Cimetière,&Fontaine Zora, Forteresse Gerudo,&Kakariko et Ranch Lon Lon.",
      "Du hast die %yNachtmaske%w!&Eine Maske, die Schlaflosigkeit&und Nachtsicht verleihen soll.^Aufsetzen auf der "
      "Maskenseite.^Beim Tragen am %gTag%w erscheinen alle&%cnur-nachts Goldskulltulas%w, als wäre&es Nacht - "
      "Friedhof, Zora-Quelle,&Gerudo-Festung, Kakariko und&Lon Lon Ranch." },
    { ITEM_MM_MASK_BLAST, PLAYER_IA_MM_MASK_BLAST, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_BLAST,
      "You got the %yBlast Mask%w!&A mask of explosive power born&of pure detonation.^Equip from the mask "
      "page.^%y\xA0%w detonates an %rinstant explosion%w&at Link's position - no bombs needed.&Cooldown: %g~310 "
      "frames%w (~16 s).&With %cgMods.BlastMask.Instant%w on,&cooldown drops to 1 frame.",
      "Vous obtenez le %yMasque d'Explosion%w!&Un masque de pure détonation&aux pouvoirs explosifs.^Équipez depuis la "
      "page des masques.^%y\xA0%w déclenche une %rexplosion instantanée%w&à la position de Link - aucune "
      "bombe.&Recharge: %g~310 frames%w (~16 s).&Avec %cgMods.BlastMask.Instant%w activé,&la recharge tombe à 1 frame.",
      "Du hast die %yExplosionsmaske%w!&Eine Maske explosiver Kraft,&geboren aus reiner Detonation.^Aufsetzen auf der "
      "Maskenseite.^%y\xA0%w zündet eine %rsofortige Explosion%w&an Links Position - keine Bomben nötig.&Abklingzeit: "
      "%g~310 Frames%w (~16 s).&Mit %cgMods.BlastMask.Instant%w an,&fällt die Abklingzeit auf 1 Frame." },
    { ITEM_MM_MASK_STONE, PLAYER_IA_MM_MASK_STONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_STONE,
      "You got the %yStone Mask%w!&A featureless gray mask said to&render its wearer beneath notice.^Equip from the "
      "mask page.^While worn, %cenemies cannot see you%w&- they will not target you,&aggro you, or react to "
      "your&presence at all. Stealth pure.",
      "Vous obtenez le %yMasque de Pierre%w!&Un masque gris sans visage qui&rend son porteur invisible.^Équipez depuis "
      "la page des masques.^Pendant le port, %cles ennemis ne&peuvent pas vous voir%w - ils ne&vous ciblent pas, ne "
      "deviennent pas&agressifs, ne réagissent pas. Furtivité pure.",
      "Du hast die %ySteinmaske%w!&Eine merkmallose graue Maske, die&ihren Träger unsichtbar macht.^Aufsetzen auf der "
      "Maskenseite.^Beim Tragen können dich %cFeinde&nicht sehen%w - sie zielen nicht&auf dich, werden nicht "
      "aggressiv&und reagieren nicht auf dich. Reine Tarnung." },
    { ITEM_MM_MASK_GREAT_FAIRY, PLAYER_IA_MM_MASK_GREAT_FAIRY, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GREAT_FAIRY,
      "You got the %yGreat Fairy Mask%w!&A wreath of long pink hair&blessed by the fairies.^Equip from the mask "
      "page.&In a fairy fountain, %y\xA0%w claims&the Great Fairy reward.^Press %y\xA1%w anywhere to open the&%cFairy "
      "Warp Menu%w - teleport to&any unlocked Great Fairy fountain.&Hair physics flow as you move.",
      "Vous obtenez le %yMasque de la Grande Fée%w!&Une couronne de longs cheveux roses&bénie par les fées.^Équipez "
      "depuis la page des masques.&Dans une fontaine, %y\xA0%w réclame&la récompense de la Grande Fée.^%y\xA1%w "
      "n'importe où ouvre le&%cMenu de Téléportation%w - voyagez&vers toute fontaine débloquée.&Physique de cheveux en "
      "mouvement.",
      "Du hast die %yFeenmaske%w!&Ein Kranz langer rosa Haare,&von den Feen gesegnet.^Aufsetzen auf der "
      "Maskenseite.&In einer Feenquelle %y\xA0%w drücken,&um die Belohnung zu erhalten.^Drücke %y\xA1%w überall für "
      "das&%cFeen-Warp-Menü%w - teleportiere&zu jeder freigeschalteten Feenquelle.&Haar-Physik beim Bewegen." },
    { ITEM_MM_MASK_DEKU, PLAYER_IA_MM_MASK_DEKU, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_DEKU,
      "You got the %gDeku Mask%w!&Holds the spirit of a fallen&Deku Scrub.^Equip from the mask page -&Link transforms "
      "into a small,&light Deku Scrub.^%y\xA0%w spin attack (pn_attack).&Hold %y\xA0%w to aim -> release fires&a "
      "%gbubble projectile%w (costs Magic).^Stand on a %gDeku Flower%w + %y\xA0%w to&burrow, charge, then launch "
      "into&a finite-distance %gglide%w.^%bWater%w skips you across the&surface like a stone (5 "
      "hops).&%rFire/lava/water%w is fatal.",
      "Vous obtenez le %gMasque Mojo%w!&Renferme l'esprit d'une Pestoène&tombée au combat.^Équipez depuis la page des "
      "masques -&Link se transforme en petite&Pestoène légère.^%y\xA0%w attaque tournoyante (pn_attack).&Maintenez "
      "%y\xA0%w pour viser -> relâchez&pour tirer une %gbulle%w (coûte de la Magie).^Sur une %gFleur Mojo%w + %y\xA0%w "
      "pour&s'enfouir, charger et se lancer&en %gvol plané%w à distance limitée.^%bL'eau%w vous fait ricocher comme&un "
      "caillou (5 sauts). %rFeu/lave/eau%w&est fatal.",
      "Du hast die %gDeku-Maske%w!&Birgt den Geist eines gefallenen&Deku-Höriger.^Aufsetzen auf der Maskenseite -&Link "
      "verwandelt sich in einen kleinen,&leichten Deku-Höriger.^%y\xA0%w Drehangriff (pn_attack).&Halte %y\xA0%w zum "
      "Zielen -> loslassen&feuert ein %gBlasenprojektil%w (Magie).^Auf einer %gDeku-Blume%w + %y\xA0%w zum&Eingraben, "
      "Aufladen und Abschuss&in einen begrenzten %gGleitflug%w.^%bWasser%w lässt dich wie ein Stein&hüpfen (5 "
      "Sprünge). %rFeuer/Lava/Wasser%w&ist tödlich." },
    { ITEM_MM_MASK_KEATON, PLAYER_IA_MM_MASK_KEATON, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_KEATON,
      "You got the %yKeaton Mask%w!&A fox-fairy mask said to summon&the trickster Keaton.^Equip from the mask "
      "page.&%rNo gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque de Keaton%w!&Un masque de renard-esprit qui&invoquerait le farceur Keaton.^Équipez "
      "depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast die %yKeaton-Maske%w!&Eine Fuchsgeist-Maske, die den&Trickser Keaton beschwören soll.^Aufsetzen auf der "
      "Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_BREMEN, PLAYER_IA_MM_MASK_BREMEN, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_BREMEN,
      "You got the %yBremen Mask%w!&The mask of the marching musician&from the Bremen Town Musicians.^Equip from the "
      "mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque de Brême%w!&Le masque du musicien en marche&des Musiciens de Brême.^Équipez depuis la "
      "page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast die %yBremen-Maske%w!&Die Maske des marschierenden Musikers&aus den Bremer Stadtmusikanten.^Aufsetzen "
      "auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_BUNNY, PLAYER_IA_MM_MASK_BUNNY, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_BUNNY,
      "You got the %yBunny Hood%w (MM)!&The fluffy long-eared hood of&Majora's Mask.^Equip from the mask page.^Grants "
      "%gincreased run speed%w&and %ghigher jumps%w (uses the&existing Bunny Hood enhancement&while wearing the MM "
      "hood).",
      "Vous obtenez le %yMasque de Lapin%w (MM)!&La capuche aux longues oreilles&velues de Majora's Mask.^Équipez "
      "depuis la page des masques.^Octroie %gvitesse de course%w accrue&et %gsauts plus hauts%w "
      "(utilise&l'amélioration existante du Masque&de Lapin).",
      "Du hast die %yHasenohren%w (MM)!&Die flauschige lange-Ohren-Mütze&aus Majoras Mask.^Aufsetzen auf der "
      "Maskenseite.^Gewährt %gerhöhte Laufgeschwindigkeit%w&und %ghöhere Sprünge%w (nutzt die&bestehende "
      "Hasenohren-Erweiterung&beim Tragen der MM-Mütze)." },
    { ITEM_MM_MASK_DON_GERO, PLAYER_IA_MM_MASK_DON_GERO, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_DON_GERO,
      "You got %yDon Gero's Mask%w!&The conductor's mask of the&frog choir.^Equip from the mask page.&Approach the "
      "%glog at Zora's River%w&and press %y\xA0%w to %ccollect every&unclaimed Frog Song reward%w at once.^Frog flags "
      "0-4 = purple rupee&each, flags 5-6 = heart piece.",
      "Vous obtenez le %yMasque de Don Gero%w!&Le masque du chef d'orchestre&du chœur des grenouilles.^Équipez depuis "
      "la page des masques.&Approchez la %gbûche à la Rivière Zora%w&et appuyez sur %y\xA0%w pour %crécupérer&toutes "
      "les récompenses non-réclamées%w.^Drapeaux 0-4 = rubis violet chacun,&drapeaux 5-6 = pièce de cœur.",
      "Du hast %yDon Geros Maske%w!&Die Dirigentenmaske des&Froschchors.^Aufsetzen auf der Maskenseite.&Geh zum "
      "%gBaumstamm an Zoras Fluss%w&und drücke %y\xA0%w um %calle ungeholten&Froschlied-Belohnungen%w zu "
      "sammeln.^Flags 0-4 = je lila Rupie,&Flags 5-6 = Herzteil." },
    { ITEM_MM_MASK_SCENTS, PLAYER_IA_MM_MASK_SCENTS, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_SCENTS,
      "You got the %yMask of Scents%w!&A mask said to grant the keen&nose of a beast.^Equip from the mask page.&%rNo "
      "gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque des Odeurs%w!&Un masque qui octroierait le&flair d'une bête sauvage.^Équipez depuis la "
      "page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast die %yGeruchsmaske%w!&Eine Maske, die den scharfen&Geruchssinn eines Tieres verleiht.^Aufsetzen auf der "
      "Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_GORON, PLAYER_IA_MM_MASK_GORON, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GORON,
      "You got the %rGoron Mask%w!&Holds the spirit of the fallen&Goron hero Darmani.^Equip from the mask page -&Link "
      "transforms into a heavy,&powerful Goron.^3-hit %rpunch combo%w (%y\xA0%w / %y\xA0%w / %y\xA0%w):&left fist, "
      "right fist, butt slam.&Same heavy blunt damage as the&%rMegaton Hammer%w.^Hold %y\xA3%w to %rcurl into a ball%w "
      "-&fast Goron Roll. %y\xA0%w mid-roll&to %rground pound%w (jump -> slam).^%rImmune to lava and fire%w.&%bSinks "
      "in water%w - voids out from&deep water.",
      "Vous obtenez le %rMasque de Goron%w!&Renferme l'esprit du héros&goron déchu Darmani.^Équipez depuis la page des "
      "masques -&Link se transforme en Goron&lourd et puissant.^%rCombo de 3 coups%w (%y\xA0%w / %y\xA0%w / "
      "%y\xA0%w):&poing gauche, poing droit, attaque-fesse.&Même dégâts lourds que la&%rMasse des Titans%w.^Maintenez "
      "%y\xA3%w pour vous %renrouler%w&en boule - Roulade Goron rapide.&%y\xA0%w en roulant pour un "
      "%rgroundpound%w&(saut -> impact).^%rImmunisé au feu et à la lave%w.&%bCoule dans l'eau%w - sortie forcée&en eau "
      "profonde.",
      "Du hast die %rGoronen-Maske%w!&Birgt den Geist des gefallenen&Goronen-Helden Darmani.^Aufsetzen auf der "
      "Maskenseite -&Link verwandelt sich in einen schweren,&kraftvollen Goronen.^3-Hit %rFaustkombo%w (%y\xA0%w / "
      "%y\xA0%w / %y\xA0%w):&linke Faust, rechte Faust, Sturzangriff.&Selber schwerer Schaden wie "
      "der&%rStahlhammer%w.^Halte %y\xA3%w zum %rEinrollen%w -&schneller Goronen-Roll. %y\xA0%w im Roll&für "
      "%rStampfangriff%w (Sprung -> Schlag).^%rImmun gegen Lava und Feuer%w.&%bSinkt im Wasser%w - Voids aus&tiefem "
      "Wasser." },
    { ITEM_MM_MASK_ROMANI, PLAYER_IA_MM_MASK_ROMANI, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_ROMANI,
      "You got %yRomani's Mask%w!&A young rancher's mask carrying&her trust with cattle.^Equip from the mask "
      "page.&Walk up to %gany cow%w and press %y\xA0%w -&the cow gives you milk %cdirectly%w&without needing Epona's "
      "Song.",
      "Vous obtenez le %yMasque de Romani%w!&Le masque d'une jeune fermière&qui inspire confiance au bétail.^Équipez "
      "depuis la page des masques.&Approchez %gn'importe quelle vache%w et %y\xA0%w -&elle vous donne du lait "
      "%cdirectement%w&sans la Chanson d'Épona.",
      "Du hast %yRomanis Maske%w!&Eine junge Bauernmaske die ihr&Vertrauen zu Kühen trägt.^Aufsetzen auf der "
      "Maskenseite.&Geh zu %gjeder Kuh%w und drücke %y\xA0%w -&die Kuh gibt dir Milch %cdirekt%w&ohne Eponas Lied." },
    { ITEM_MM_MASK_CIRCUS_LEADER, PLAYER_IA_MM_MASK_CIRCUS_LEADER, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_CIRCUS_LEADER,
      "You got the %yCircus Leader's Mask%w!&Worn, you become Ganondorf's&%cTax Collector%w. NPCs cower&and pay "
      "tribute on sight.^Talk to a minigame NPC and the&%centire minigame is skipped%w -&its reward is granted "
      "directly:^%gShooting Gallery%w (bullet bag/quiver),&%gBombchu Bowling%w (bomb bag -> heart piece),&%gIngo%w "
      "(Epona + Hyrule Field warp),&%gTalon%w (Milk Bottle, child Lon Lon),&%gAdult Malon%w (sells cow, %p100 "
      "Rupees%w),&%gHBA%w, %gFishing Pond%w, %gChest Game%w,&%gZora Diving Game%w (Silver Scale).^Repeat visits give a "
      "small bribe.&Rando-aware: delivers shuffled checks.",
      "Vous obtenez le %yMasque du Chef de Cirque%w!&Porté, vous devenez le %cCollecteur&d'Impôts%w de Ganondorf. Les "
      "PNJ&se soumettent à votre vue.^Parler à un PNJ de mini-jeu et le&%cmini-jeu entier est sauté%w -&sa récompense "
      "est donnée directement:^%gStand de Tir%w (sac à billes/carquois),&%gBombchu Bowling%w (sac de bombes -> "
      "cœur),&%gIngo%w (Épona + transition Plaine d'Hyrule),&%gTalon%w (Bouteille de Lait, Lon Lon enfant),&%gMalon "
      "adulte%w (vend vache, %p100 Rubis%w),&%gHBA%w, %gPêche%w, %gJeu de Coffres%w,&%gJeu de Plongée Zora%w (Écaille "
      "d'Argent).^Visites répétées donnent un pourboire.&Conscient du rando: livre les items shufflés.",
      "Du hast die %yZirkusleitermaske%w!&Beim Tragen wirst du zu Ganondorfs&%cSteuereintreiber%w. NPCs zahlen&Tribut "
      "bei deinem Anblick.^Mit einem Minispiel-NPC reden und das&%ggesamte Minispiel wird übersprungen%w -&Belohnung "
      "wird direkt gegeben:^%gSchießbude%w (Munitionstasche/Köcher),&%gBombchu-Bowling%w (Bombentasche -> "
      "Herzteil),&%gIngo%w (Epona + Hyrule-Feld-Warp),&%gTalon%w (Milchflasche, Kind Lon Lon),&%gErwachsene Malon%w "
      "(verkauft Kuh, %p100 Rupien%w),&%gHBA%w, %gAngelteich%w, %gKistenspiel%w,&%gZora-Tauchspiel%w "
      "(Silberschuppe).^Wiederholungsbesuche geben Bestechungsgeld.&Rando-bewusst: liefert die geshufflten Items." },
    { ITEM_MM_MASK_KAFEI, PLAYER_IA_MM_MASK_KAFEI, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_KAFEI,
      "You got %yKafei's Mask%w!&A small mask carved in the&likeness of a missing groom.^Equip from the mask "
      "page.&While worn, %y\xA0%w toggles a&%cKafei character model%w on Link.&Cosmetic only.",
      "Vous obtenez le %yMasque de Kafei%w!&Un petit masque sculpté à l'image&d'un fiancé disparu.^Équipez depuis la "
      "page des masques.&Pendant le port, %y\xA0%w bascule un&%cmodèle de Kafei%w sur Link.&Cosmétique seulement.",
      "Du hast %yKafeis Maske%w!&Eine kleine Maske im Antlitz&eines verschwundenen Bräutigams.^Aufsetzen auf der "
      "Maskenseite.&Beim Tragen schaltet %y\xA0%w ein&%cKafei-Charaktermodell%w an Link um.&Nur Kosmetik." },
    { ITEM_MM_MASK_COUPLE, PLAYER_IA_MM_MASK_COUPLE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_COUPLE,
      "You got the %yCouple's Mask%w!&The reunion mask of two lovers&forever entwined.^Equip from the mask "
      "page.^%cPassive regen%w while worn:&%rDay%w -> %g+1 HP every 4 frames%w&(full hearts in ~32s).&%bNight%w -> "
      "%g+1 MP every 7 frames%w&(full magic in ~32s).",
      "Vous obtenez le %yMasque des Amoureux%w!&Le masque des retrouvailles de deux&amants à jamais "
      "entrelacés.^Équipez depuis la page des masques.^%cRégénération passive%w pendant le port:&%rJour%w -> %g+1 PV "
      "toutes les 4 frames%w&(cœurs pleins en ~32s).&%bNuit%w -> %g+1 PM toutes les 7 frames%w&(magie pleine en ~32s).",
      "Du hast die %yPaarmaske%w!&Die Wiedervereinigungsmaske zweier&für immer verbundener Liebender.^Aufsetzen auf "
      "der Maskenseite.^%cPassive Regeneration%w beim Tragen:&%rTag%w -> %g+1 HP alle 4 Frames%w&(volle Herzen in "
      "~32s).&%bNacht%w -> %g+1 MP alle 7 Frames%w&(volle Magie in ~32s)." },
    { ITEM_MM_MASK_TRUTH, PLAYER_IA_MM_MASK_TRUTH, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_TRUTH,
      "You got the %yMask of Truth%w (MM)!&The all-seeing eye that hears the&voices of beasts and stones.^Equip from "
      "the mask page.&%rNo gameplay effect yet%w -&currently cosmetic only.&(OOT's vanilla Mask of Truth is&a separate "
      "item.)",
      "Vous obtenez le %yMasque de Vérité%w (MM)!&L'œil omniscient qui entend les&voix des bêtes et des "
      "pierres.^Équipez depuis la page des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.&(Le "
      "Masque de Vérité OOT est&un objet distinct.)",
      "Du hast die %yMaske der Wahrheit%w (MM)!&Das allsehende Auge, das die Stimmen&der Tiere und Steine "
      "hört.^Aufsetzen auf der Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik.&(OOTs Vanilla-Maske der "
      "Wahrheit&ist ein separates Item.)" },
    { ITEM_MM_MASK_ZORA, PLAYER_IA_MM_MASK_ZORA, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_ZORA,
      "You got the %bZora Mask%w!&Holds the spirit of the fallen&Zora guitarist Mikau.^Equip from the mask page -&Link "
      "transforms into a Zora,&master of the waters.^Real %bZora swim mechanics%w 1:1:&surface walk, %bfast dolphin "
      "swim%w,&%bswim dash%w (%y\xA0%w), %bdolphin jump%w arc.^On land: %y\xA0%w throws %bBoomerang Fins%w&(twin fin "
      "projectiles).&%y\xA3%w + %y\xA0%w raises an %cElectric Barrier%w&that shocks attackers (costs Magic).^Aerial "
      "%y\xA0%w -> flying %bjump kick%w.",
      "Vous obtenez le %bMasque de Zora%w!&Renferme l'esprit du guitariste&zora déchu Mikau.^Équipez depuis la page "
      "des masques -&Link se transforme en Zora,&maître des eaux.^Vraies %bmécaniques de nage Zora%w 1:1:&marche en "
      "surface, %bnage dauphin rapide%w,&%bdash de nage%w (%y\xA0%w), %bsaut de dauphin%w.^À terre: %y\xA0%w lance des "
      "%bAilerons Boomerang%w&(deux projectiles).&%y\xA3%w + %y\xA0%w élève une %cBarrière Électrique%w&qui foudroie "
      "les attaquants (Magie).^En l'air %y\xA0%w -> %bcoup de pied%w volant.",
      "Du hast die %bZora-Maske%w!&Birgt den Geist des gefallenen&Zora-Gitarristen Mikau.^Aufsetzen auf der "
      "Maskenseite -&Link verwandelt sich in einen Zora,&Meister des Wassers.^Echte %bZora-Schwimmmechanik%w "
      "1:1:&Wasserlauf, %bschnelles Delfinschwimmen%w,&%bSchwimm-Dash%w (%y\xA0%w), %bDelfinsprung%w-Bogen.^An Land: "
      "%y\xA0%w wirft %bBumerang-Flossen%w&(zwei Flossen-Projektile).&%y\xA3%w + %y\xA0%w erhebt eine %cElektrische "
      "Barriere%w&die Angreifer schockt (Magie).^In der Luft %y\xA0%w -> fliegender %bSprungkick%w." },
    { ITEM_MM_MASK_KAMARO, PLAYER_IA_MM_MASK_KAMARO, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_KAMARO,
      "You got %yKamaro's Mask%w!&The mask of a wandering ghost&dancer who lost his audience.^Equip from the mask "
      "page.&%cHold %y\xA0%w to dance%w (movement locks).&Release to stop.^At %gGoron City%w near Darunia,&dance with "
      "him for ~5 seconds&to trigger %cDarunia's Joy%w reward.",
      "Vous obtenez le %yMasque de Kamaro%w!&Le masque d'un fantôme danseur&errant ayant perdu son public.^Équipez "
      "depuis la page des masques.&%cMaintenez %y\xA0%w pour danser%w (mouvement verrouillé).&Relâchez pour "
      "arrêter.^Au %gVillage Goron%w près de Darunia,&dansez avec lui ~5 secondes pour&déclencher la %cJoie de "
      "Darunia%w.",
      "Du hast %yKamaros Maske%w!&Die Maske eines umherwandernden Geist-&Tänzers ohne Publikum.^Aufsetzen auf der "
      "Maskenseite.&%cHalte %y\xA0%w zum Tanzen%w (Bewegung sperrt).&Loslassen zum Stoppen.^In %gGoronen-Stadt%w nahe "
      "Darunia,&tanze mit ihm für ~5 Sekunden,&um %cDarunias Freude%w auszulösen." },
    { ITEM_MM_MASK_GIBDO, PLAYER_IA_MM_MASK_GIBDO, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GIBDO,
      "You got the %yGibdo Mask%w!&The decayed face of a mummy,&worn by ancient cult initiates.^Equip from the mask "
      "page.^While worn, %cReDeads and Gibdos%w&%gignore you completely%w -&they will not lunge or grab&while you wear "
      "the mask.",
      "Vous obtenez le %yMasque de Gibdo%w!&Le visage décrépit d'une momie,&porté par les initiés cultistes.^Équipez "
      "depuis la page des masques.^Pendant le port, %cReDead et Gibdo%w&%gvous ignorent complètement%w -&ils ne vous "
      "attaquent ni ne&vous attrapent.",
      "Du hast die %yGibdo-Maske%w!&Das verwitterte Antlitz einer Mumie,&getragen von Kultanwärtern.^Aufsetzen auf der "
      "Maskenseite.^Beim Tragen %gignorieren%w dich&%cReDead und Gibdo%w komplett -&sie greifen dich nicht an "
      "und&packen dich nicht." },
    { ITEM_MM_MASK_GARO, PLAYER_IA_MM_MASK_GARO, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GARO,
      "You got %yGaro's Mask%w!&The shrouded mask of a Garo&ninja, sworn to silence.^Equip from the mask page.&%rNo "
      "gameplay effect yet%w -&currently cosmetic only.",
      "Vous obtenez le %yMasque de Garo%w!&Le masque drapé d'un ninja Garo,&qui a juré silence.^Équipez depuis la page "
      "des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.",
      "Du hast %yGaros Maske%w!&Die verhüllte Maske eines Garo-&Ninjas, der Stille geschworen hat.^Aufsetzen auf der "
      "Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik." },
    { ITEM_MM_MASK_CAPTAIN, PLAYER_IA_MM_MASK_CAPTAIN, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_CAPTAIN,
      "You got the %yCaptain's Hat%w!&The crested helm of Captain&Keeta, leader of the Stalfos.^Equip from the mask "
      "page.^At night in %gHyrule Field%w only,&summons %rgiant Stalfos%w (adult Link)&or %rgiant Stalchildren%w (2x "
      "scale&and speed for child) to roam the field.&One spawns every ~5 seconds, max 3.",
      "Vous obtenez la %yCasquette du Capitaine%w!&Le casque du Capitaine Keeta,&chef des Stalfos.^Équipez depuis la "
      "page des masques.^La nuit dans la %gPlaine d'Hyrule%w,&invoque des %rStalfos géants%w (Link adulte)&ou des "
      "%rStalchild géants%w (2x taille&et vitesse pour Jeune Link).&Un toutes les ~5s, max. 3.",
      "Du hast den %yKapitänshut%w!&Der Kammhelm von Captain Keeta,&Anführer der Stalfos.^Aufsetzen auf der "
      "Maskenseite.^Nur nachts in %gHyrule-Feld%w werden&%rriesige Stalfos%w (Erwachsener) oder&%rriesige Stalchild%w "
      "(2x Größe und&Tempo für Jung) gerufen.&Einer alle ~5s, max. 3." },
    { ITEM_MM_MASK_GIANT, PLAYER_IA_MM_MASK_GIANT, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL,
      func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_GIANT,
      "You got the %yGiant's Mask%w!&The colossal mask said to grow&its wearer to monstrous size.^Equip from the mask "
      "page.&%rNo gameplay effect yet%w -&currently cosmetic only.&(In MM it scales Link to fight&Twinmold.)",
      "Vous obtenez le %yMasque de Géant%w!&Le masque colossal qui ferait&grandir son porteur.^Équipez depuis la page "
      "des masques.&%rPas d'effet de jeu%w -&actuellement cosmétique seulement.&(Dans MM, il agrandit Link "
      "pour&combattre Twinmold.)",
      "Du hast die %yRiesenmaske%w!&Die kolossale Maske, die ihren&Träger riesenhaft wachsen lässt.^Aufsetzen auf der "
      "Maskenseite.&%rNoch kein Effekt%w -&derzeit nur Kosmetik.&(In MM vergrößert sie Link&für den Twinmold-Kampf.)" },
    { ITEM_MM_MASK_FIERCE_DEITY, PLAYER_IA_MM_MASK_FIERCE_DEITY, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE,
      NULL, func_8083485C, Player_InitDefaultIA, Randomizer_DrawMmMask, RG_MM_MASK_FIERCE_DEITY,
      "You got %pFierce Deity's Mask%w!&The legendary forbidden mask of&a god-like warrior.^Equip from the mask page "
      "-&Link transforms into the towering&%pFierce Deity%w. The Final Form.^%c1.5x movement speed%w - the only&form "
      "with a speed multiplier.^Wields a massive %ptwo-handed sword%w&(PLAYER_ANIMTYPE_3 stance).&Every full-health "
      "%y\xA0%w swing fires&a long-range %psword beam%w projectile.^Hyrule's strongest combat form.",
      "Vous obtenez le %pMasque du Dieu Féroce%w!&Le masque légendaire interdit d'un&guerrier divin.^Équipez depuis la "
      "page des masques -&Link se transforme en imposant&%pDieu Féroce%w. La Forme Finale.^%c1,5x vitesse de "
      "déplacement%w - la&seule forme avec un bonus de vitesse.^Manie une massive %pépée à deux mains%w&(posture "
      "PLAYER_ANIMTYPE_3).&Chaque coup %y\xA0%w à pleine santé tire&un %prayon d'épée%w à longue portée.^La forme de "
      "combat la plus puissante.",
      "Du hast %pMajoras Maske%w!&Die legendäre verbotene Maske eines&gottgleichen Kriegers.^Aufsetzen auf der "
      "Maskenseite -&Link verwandelt sich in den hoch&aufragenden %pFinsteren Gott%w. Die Endform.^%c1,5x "
      "Bewegungsgeschwindigkeit%w - die&einzige Form mit Geschwindigkeitsbonus.^Führt ein massives "
      "%pZweihandschwert%w&(PLAYER_ANIMTYPE_3-Haltung).&Jeder %y\xA0%w-Schwung bei voller Gesundheit&feuert einen "
      "%pSchwertstrahl%w in die Ferne.^Hyrules stärkste Kampfform." },

    // MM collectibles ported into OoT rando (Stray Fairy + 4 Boss Remains). No OoT inventory item
    // (item=NEI_NO_ITEM) and no player action (ia=PLAYER_IA_NONE, generic funcs) — these rows exist only
    // so Nei_FindByRg supplies the get-item 3D model (drawFunc) + textbox name. Give is a no-op in
    // Randomizer_Item_Give. Stray Fairy uses the Flex-skeleton draw; Remains share Randomizer_DrawMmRemains.
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmStrayFairy, RG_MM_STRAY_FAIRY,
      "You got a %cClock Town Stray Fairy%w!&A lost fairy from the Great Fairy&of Clock Town, far from home.",
      "Vous obtenez une %cFée Égarée de Bourg-Clocher%w!&Une fée perdue de la Grande Fée&de Bourg-Clocher, loin de "
      "chez elle.",
      "Du hast eine %cVerirrte Fee von Unruhstadt%w!&Eine verlorene Fee der Großen&Fee von Unruhstadt, weit von zu "
      "Hause." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmStrayFairy, RG_MM_STRAY_FAIRY_WOODFALL,
      "You got a %cWoodfall Stray Fairy%w!&A lost fairy from the Great Fairy&of Woodfall, far from home.",
      "Vous obtenez une %cFée Égarée des Bois-Cascade%w!&Une fée perdue de la Grande Fée&des Bois-Cascade, loin de "
      "chez elle.",
      "Du hast eine %cVerirrte Fee vom Waldfall%w!&Eine verlorene Fee der Großen&Fee vom Waldfall, weit von zu "
      "Hause." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmStrayFairy, RG_MM_STRAY_FAIRY_SNOWHEAD,
      "You got a %cSnowhead Stray Fairy%w!&A lost fairy from the Great Fairy&of Snowhead, far from home.",
      "Vous obtenez une %cFée Égarée du Mont-Neige%w!&Une fée perdue de la Grande Fée&du Mont-Neige, loin de chez "
      "elle.",
      "Du hast eine %cVerirrte Fee vom Schneegipfel%w!&Eine verlorene Fee der Großen&Fee vom Schneegipfel, weit von zu "
      "Hause." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmStrayFairy, RG_MM_STRAY_FAIRY_GREAT_BAY,
      "You got a %cGreat Bay Stray Fairy%w!&A lost fairy from the Great Fairy&of Great Bay, far from home.",
      "Vous obtenez une %cFée Égarée de la Grande Baie%w!&Une fée perdue de la Grande Fée&de la Grande Baie, loin de "
      "chez elle.",
      "Du hast eine %cVerirrte Fee der Großen Bucht%w!&Eine verlorene Fee der Großen&Fee der Großen Bucht, weit von zu "
      "Hause." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmStrayFairy, RG_MM_STRAY_FAIRY_STONE_TOWER,
      "You got a %cStone Tower Stray Fairy%w!&A lost fairy from the Great Fairy&of Stone Tower, far from home.",
      "Vous obtenez une %cFée Égarée du Donjon de Pierre%w!&Une fée perdue de la Grande Fée&du Donjon de Pierre, loin "
      "de chez elle.",
      "Du hast eine %cVerirrte Fee vom Steinturm%w!&Eine verlorene Fee der Großen&Fee vom Steinturm, weit von zu "
      "Hause." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmRemains, RG_MM_REMAINS_ODOLWA,
      "You got %gOdolwa's Remains%w!&Proof of the fallen jungle warrior&of Woodfall Temple.",
      "Vous obtenez le %gReliquat d'Odolwa%w!&Preuve de la chute du guerrier&de la jungle du Temple des Bois.",
      "Du hast %gOdolwas Überreste%w!&Beweis für den gefallenen&Dschungelkrieger des Waldtempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmRemains, RG_MM_REMAINS_GOHT,
      "You got %gGoht's Remains%w!&Proof of the fallen mechanical bull&of Snowhead Temple.",
      "Vous obtenez le %gReliquat de Goht%w!&Preuve de la chute du taureau&mécanique du Temple des Neiges.",
      "Du hast %gGohts Überreste%w!&Beweis für den gefallenen&mechanischen Stier des Schneetempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmRemains, RG_MM_REMAINS_GYORG,
      "You got %gGyorg's Remains%w!&Proof of the fallen giant masked fish&of Great Bay Temple.",
      "Vous obtenez le %gReliquat de Gyorg%w!&Preuve de la chute du poisson&masqué géant du Temple de la Baie.",
      "Du hast %gGyorgs Überreste%w!&Beweis für den gefallenen&maskierten Riesenfisch des Meerestempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmRemains, RG_MM_REMAINS_TWINMOLD,
      "You got %gTwinmold's Remains%w!&Proof of the fallen twin sand worms&of Stone Tower Temple.",
      "Vous obtenez le %gReliquat de Twinmold%w!&Preuve de la chute des vers de&sable jumeaux de la Tour de Pierre.",
      "Du hast %gTwinmolds Überreste%w!&Beweis für die gefallenen&Zwillings-Sandwürmer des Steinturms." },

    // MM per-dungeon items ported into OoT rando. No OoT inventory item / slot; each row exists only so
    // Nei_FindByRg supplies the shared per-type get-item model (drawFunc) + textbox name. Give is a no-op.
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSmallKey, RG_MM_SMALL_KEY_WOODFALL,
      "You got a %cWoodfall Small Key%w!&A key from Woodfall Temple.",
      "Vous obtenez une %cPetite Clé des Bois-Cascade%w!&Une clé du Temple des Bois.",
      "Du hast einen %cKleinen Schlüssel vom Waldfall%w!&Ein Schlüssel aus dem Waldtempel." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSmallKey, RG_MM_SMALL_KEY_SNOWHEAD,
      "You got a %cSnowhead Small Key%w!&A key from Snowhead Temple.",
      "Vous obtenez une %cPetite Clé du Mont-Neige%w!&Une clé du Temple des Neiges.",
      "Du hast einen %cKleinen Schlüssel vom Schneegipfel%w!&Ein Schlüssel aus dem Schneetempel." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSmallKey, RG_MM_SMALL_KEY_GREAT_BAY,
      "You got a %cGreat Bay Small Key%w!&A key from Great Bay Temple.",
      "Vous obtenez une %cPetite Clé de la Grande Baie%w!&Une clé du Temple de la Baie.",
      "Du hast einen %cKleinen Schlüssel der Großen Bucht%w!&Ein Schlüssel aus dem Meerestempel." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSmallKey, RG_MM_SMALL_KEY_STONE_TOWER,
      "You got a %cStone Tower Small Key%w!&A key from Stone Tower Temple.",
      "Vous obtenez une %cPetite Clé du Donjon de Pierre%w!&Une clé de la Tour de Pierre.",
      "Du hast einen %cKleinen Schlüssel vom Steinturm%w!&Ein Schlüssel aus dem Steinturm." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmBossKey, RG_MM_BOSS_KEY_WOODFALL,
      "You got the %cWoodfall Boss Key%w!&Opens the door to Odolwa,&master of Woodfall Temple.",
      "Vous obtenez la %cClé du Boss des Bois-Cascade%w!&Ouvre la porte d'Odolwa,&maître du Temple des Bois.",
      "Du hast den %cBossschlüssel vom Waldfall%w!&Öffnet das Tor zu Odolwa,&Meister des Waldtempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmBossKey, RG_MM_BOSS_KEY_SNOWHEAD,
      "You got the %cSnowhead Boss Key%w!&Opens the door to Goht,&master of Snowhead Temple.",
      "Vous obtenez la %cClé du Boss du Mont-Neige%w!&Ouvre la porte de Goht,&maître du Temple des Neiges.",
      "Du hast den %cBossschlüssel vom Schneegipfel%w!&Öffnet das Tor zu Goht,&Meister des Schneetempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmBossKey, RG_MM_BOSS_KEY_GREAT_BAY,
      "You got the %cGreat Bay Boss Key%w!&Opens the door to Gyorg,&master of Great Bay Temple.",
      "Vous obtenez la %cClé du Boss de la Grande Baie%w!&Ouvre la porte de Gyorg,&maître du Temple de la Baie.",
      "Du hast den %cBossschlüssel der Großen Bucht%w!&Öffnet das Tor zu Gyorg,&Meister des Meerestempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmBossKey, RG_MM_BOSS_KEY_STONE_TOWER,
      "You got the %cStone Tower Boss Key%w!&Opens the door to Twinmold,&master of Stone Tower Temple.",
      "Vous obtenez la %cClé du Boss du Donjon de Pierre%w!&Ouvre la porte de Twinmold,&maître de la Tour de Pierre.",
      "Du hast den %cBossschlüssel vom Steinturm%w!&Öffnet das Tor zu Twinmold,&Meister des Steinturms." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmDungeonMap, RG_MM_MAP_WOODFALL,
      "You got the %cWoodfall Map%w!&A map of Woodfall Temple.",
      "Vous obtenez la %cCarte des Bois-Cascade%w!&Une carte du Temple des Bois.",
      "Du hast die %cWaldfall-Karte%w!&Eine Karte des Waldtempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmDungeonMap, RG_MM_MAP_SNOWHEAD,
      "You got the %cSnowhead Map%w!&A map of Snowhead Temple.",
      "Vous obtenez la %cCarte du Mont-Neige%w!&Une carte du Temple des Neiges.",
      "Du hast die %cSchneegipfel-Karte%w!&Eine Karte des Schneetempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmDungeonMap, RG_MM_MAP_GREAT_BAY,
      "You got the %cGreat Bay Map%w!&A map of Great Bay Temple.",
      "Vous obtenez la %cCarte de la Grande Baie%w!&Une carte du Temple de la Baie.",
      "Du hast die %cGroße-Bucht-Karte%w!&Eine Karte des Meerestempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmDungeonMap, RG_MM_MAP_STONE_TOWER,
      "You got the %cStone Tower Map%w!&A map of Stone Tower Temple.",
      "Vous obtenez la %cCarte du Donjon de Pierre%w!&Une carte de la Tour de Pierre.",
      "Du hast die %cSteinturm-Karte%w!&Eine Karte des Steinturms." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmCompass, RG_MM_COMPASS_WOODFALL,
      "You got the %cWoodfall Compass%w!&Reveals treasures in Woodfall Temple.",
      "Vous obtenez la %cBoussole des Bois-Cascade%w!&Révèle les trésors du Temple des Bois.",
      "Du hast den %cWaldfall-Kompass%w!&Zeigt die Schätze des Waldtempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmCompass, RG_MM_COMPASS_SNOWHEAD,
      "You got the %cSnowhead Compass%w!&Reveals treasures in Snowhead Temple.",
      "Vous obtenez la %cBoussole du Mont-Neige%w!&Révèle les trésors du Temple des Neiges.",
      "Du hast den %cSchneegipfel-Kompass%w!&Zeigt die Schätze des Schneetempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmCompass, RG_MM_COMPASS_GREAT_BAY,
      "You got the %cGreat Bay Compass%w!&Reveals treasures in Great Bay Temple.",
      "Vous obtenez la %cBoussole de la Grande Baie%w!&Révèle les trésors du Temple de la Baie.",
      "Du hast den %cGroße-Bucht-Kompass%w!&Zeigt die Schätze des Meerestempels." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmCompass, RG_MM_COMPASS_STONE_TOWER,
      "You got the %cStone Tower Compass%w!&Reveals treasures in Stone Tower Temple.",
      "Vous obtenez la %cBoussole du Donjon de Pierre%w!&Révèle les trésors de la Tour de Pierre.",
      "Du hast den %cSteinturm-Kompass%w!&Zeigt die Schätze des Steinturms." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_GOHT,
      "You got the %gSoul of Goht%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Goht%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Goht%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_GYORG,
      "You got the %gSoul of Gyorg%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Gyorg%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Gyorg%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_MAJORA,
      "You got the %gSoul of Majora%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Majora%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Majora%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_ODOLWA,
      "You got the %gSoul of Odolwa%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Odolwa%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Odolwa%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_TWINMOLD,
      "You got the %gSoul of Twinmold%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Twinmold%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Twinmold%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_ALIEN,
      "You got the %gSoul of Aliens%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Aliens%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Aliens%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_ARMOS,
      "You got the %gSoul of Armos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Armos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Armos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_BAD_BAT,
      "You got the %gSoul of Bad Bats%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Bad Bats%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Bad Bats%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_BEAMOS,
      "You got the %gSoul of Beamos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Beamos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Beamos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_BOE,
      "You got the %gSoul of Boes%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Boes%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Boes%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_BUBBLE,
      "You got the %gSoul of Bubbles%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Bubbles%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Bubbles%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_CAPTAIN_KEETA,
      "You got the %gSoul of Captain Keeta%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Captain Keeta%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Captain Keeta%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_CHUCHU,
      "You got the %gSoul of Chuchus%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Chuchus%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Chuchus%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_DEATH_ARMOS,
      "You got the %gSoul of Death Armos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Death Armos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Death Armos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_DEEP_PYTHON,
      "You got the %gSoul of Deep Pythons%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Deep Pythons%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Deep Pythons%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_DEKU_BABA,
      "You got the %gSoul of Deku Babas%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Deku Babas%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Deku Babas%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_DEXIHAND,
      "You got the %gSoul of Dexihands%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Dexihands%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Dexihands%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_DINOLFOS,
      "You got the %gSoul of Dinolfos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Dinolfos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Dinolfos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_DODONGO,
      "You got the %gSoul of Dodongos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Dodongos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Dodongos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_DRAGONFLY,
      "You got the %gSoul of Dragonflies%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Dragonflies%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Dragonflies%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_EENO,
      "You got the %gSoul of Eenos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Eenos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Eenos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_EYEGORE,
      "You got the %gSoul of Eyegores%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Eyegores%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Eyegores%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_FREEZARD,
      "You got the %gSoul of Freezards%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Freezards%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Freezards%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_GARO,
      "You got the %gSoul of Garos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Garos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Garos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_GEKKO,
      "You got the %gSoul of Gekkos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Gekkos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Gekkos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_GIANT_BEE,
      "You got the %gSoul of Giant Bees%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Giant Bees%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Giant Bees%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_GOMESS,
      "You got the %gSoul of Gomess%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Gomess%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Gomess%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_GUAY,
      "You got the %gSoul of Guays%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Guays%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Guays%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_HIPLOOP,
      "You got the %gSoul of Hiploops%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Hiploops%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Hiploops%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_IGOS_DU_IKANA,
      "You got the %gSoul of Igos du Ikana%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Igos du Ikana%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Igos du Ikana%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_IRON_KNUCKLE,
      "You got the %gSoul of Iron Knuckles%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Iron Knuckles%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Iron Knuckles%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_KEESE,
      "You got the %gSoul of Keese%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Keese%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Keese%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_LEEVER,
      "You got the %gSoul of Leevers%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Leevers%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Leevers%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_LIKE_LIKE,
      "You got the %gSoul of Like Likes%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Like Likes%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Like Likes%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_MAD_SCRUB,
      "You got the %gSoul of Mad Scrubs%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Mad Scrubs%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Mad Scrubs%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_NEJIRON,
      "You got the %gSoul of Nejirons%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Nejirons%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Nejirons%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_OCTOROK,
      "You got the %gSoul of Octoroks%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Octoroks%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Octoroks%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_PEAHAT,
      "You got the %gSoul of Peahats%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Peahats%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Peahats%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_PIRATE,
      "You got the %gSoul of Pirates%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Pirates%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Pirates%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_POE,
      "You got the %gSoul of Poes%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Poes%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Poes%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_REDEAD,
      "You got the %gSoul of Redeads%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Redeads%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Redeads%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_SHELLBLADE,
      "You got the %gSoul of Shellblades%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Shellblades%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Shellblades%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_SKULLFISH,
      "You got the %gSoul of Skullfish%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Skullfish%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Skullfish%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_SKULLTULA,
      "You got the %gSoul of Skulltulas%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Skulltulas%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Skulltulas%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_SNAPPER,
      "You got the %gSoul of Snappers%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Snappers%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Snappers%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_STALCHILD,
      "You got the %gSoul of Stalchildren%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Stalchildren%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Stalchildren%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_TAKKURI,
      "You got the %gSoul of Takkuri%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Takkuri%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Takkuri%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_TEKTITE,
      "You got the %gSoul of Tektites%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Tektites%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Tektites%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_WALLMASTER,
      "You got the %gSoul of Wallmasters%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Wallmasters%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Wallmasters%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_WART,
      "You got the %gSoul of Warts%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Warts%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Warts%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_WIZROBE,
      "You got the %gSoul of Wizrobes%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Wizrobes%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Wizrobes%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmSoul, RG_MM_SOUL_WOLFOS,
      "You got the %gSoul of Wolfos%w!&A restless spirit bound in&the land of Termina.",
      "Vous obtenez l'%gÂme de Wolfos%w!&Un esprit tourmenté lié&à la terre de Termina.",
      "Du hast die %gSeele von Wolfos%w!&Ein ruheloser Geist, gebunden&an das Land von Termina." },

    // MM trade / quest-chain items ported into OoT rando. No OoT inventory item (item=NEI_NO_ITEM) and no
    // player action — these rows exist only so Nei_FindByRg supplies the get-item 3D model (drawFunc) +
    // textbox name. Give is a no-op in Randomizer_Item_Give. All share Randomizer_DrawMmTradeQuest.
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_MOONS_TEAR,
      "You got a %gMoon's Tear%w!&A beautiful gem that fell&from the moon over Termina.",
      "Vous obtenez une %gLarme de Lune%w!&Une belle gemme tombée de&la lune au-dessus de Termina.",
      "Du hast eine %gMondträne%w!&Ein schöner Edelstein, der vom&Mond über Termina gefallen ist." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_DEED_LAND,
      "You got the %gTown Title Deed%w!&A deed of ownership from&the swamp trading chain.",
      "Vous obtenez le %gTitre de Propriété (Ville)%w!&Un acte de propriété de la&chaîne d'échange du marais.",
      "Du hast das %gGrundbuch (Stadt)%w!&Ein Eigentumsnachweis aus der&Sumpf-Tauschkette." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_DEED_SWAMP,
      "You got the %gSwamp Title Deed%w!&A deed of ownership from&the Deku trading chain.",
      "Vous obtenez le %gTitre de Propriété (Marais)%w!&Un acte de propriété de la&chaîne d'échange Mojo.",
      "Du hast das %gGrundbuch (Sumpf)%w!&Ein Eigentumsnachweis aus der&Deku-Tauschkette." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_DEED_MOUNTAIN,
      "You got the %gMountain Title Deed%w!&A deed of ownership from&the mountain trading chain.",
      "Vous obtenez le %gTitre de Propriété (Montagne)%w!&Un acte de propriété de la&chaîne d'échange de la montagne.",
      "Du hast das %gGrundbuch (Berg)%w!&Ein Eigentumsnachweis aus der&Berg-Tauschkette." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_DEED_OCEAN,
      "You got the %gOcean Title Deed%w!&A deed of ownership from&the ocean trading chain.",
      "Vous obtenez le %gTitre de Propriété (Océan)%w!&Un acte de propriété de la&chaîne d'échange de l'océan.",
      "Du hast das %gGrundbuch (Meer)%w!&Ein Eigentumsnachweis aus der&Meer-Tauschkette." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_ROOM_KEY,
      "You got the %gRoom Key%w!&The key to Kafei's hidden&room in Clock Town.",
      "Vous obtenez la %gClé de Chambre%w!&La clé de la chambre secrète&de Kafei à Bourg-Clock.",
      "Du hast den %gZimmerschlüssel%w!&Der Schlüssel zu Kafeis&verstecktem Zimmer in Unruhstadt." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_LETTER_TO_KAFEI,
      "You got the %gLetter to Kafei%w!&A letter Anju entrusted to&you, addressed to Kafei.",
      "Vous obtenez la %gLettre à Kafei%w!&Une lettre qu'Anju vous a&confiée, adressée à Kafei.",
      "Du hast den %gBrief an Kafei%w!&Ein Brief, den Anju dir&anvertraut hat, an Kafei." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_LETTER_TO_MAMA,
      "You got the %gLetter to Mama%w!&A special delivery for the&mail-loving woman's mother.",
      "Vous obtenez la %gLettre à Maman%w!&Une livraison spéciale pour la&mère de la femme du courrier.",
      "Du hast den %gBrief an Mama%w!&Eine Sonderlieferung für die&Mutter der Postfrau." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_PENDANT_OF_MEMORIES,
      "You got the %gPendant of Memories%w!&A keepsake pendant from&Kafei, proof of his promise.",
      "Vous obtenez le %gPendentif des Souvenirs%w!&Un pendentif souvenir de&Kafei, preuve de sa promesse.",
      "Du hast das %gAmulett der Erinnerungen%w!&Ein Andenken von Kafei,&Beweis seines Versprechens." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_PICTOGRAPH_BOX,
      "You got the %gPictograph Box%w!&A camera for capturing&pictographs across Termina.",
      "Vous obtenez la %gBoîte à Pictographies%w!&Un appareil pour capturer des&pictographies à travers Termina.",
      "Du hast die %gFotobox%w!&Eine Kamera zum Festhalten von&Fotografien in ganz Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_POWDER_KEG,
      "You got a %gPowder Keg%w!&A massive Goron explosive.&Handle it with great care!",
      "Vous obtenez un %gBaril de Poudre%w!&Un énorme explosif Goron.&Maniez-le avec précaution!",
      "Du hast ein %gPulverfass%w!&Ein riesiger Goronen-Sprengstoff.&Geh vorsichtig damit um!" },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_BOMBERS_NOTEBOOK,
      "You got the %gBomber's Notebook%w!&A schedule to track the people&of Termina and their troubles.",
      "Vous obtenez le %gCarnet des Bombers%w!&Un agenda pour suivre les gens&de Termina et leurs soucis.",
      "Du hast das %gBomber-Notizbuch%w!&Ein Terminplaner für die Leute&Terminas und ihre Sorgen." },

    // MM ocarina songs ported into OoT rando. No OoT slot (item=NEI_NO_ITEM) and drawFunc=NULL: these reuse
    // OoT's own note model via OBJECT_GI_MELODY + GID_SONG_* (GetItemEntry_Draw falls to GetItem_Draw(gid)).
    // These rows exist only so Nei_FindByRg supplies the textbox name. Give is a no-op.
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_SONATA,
      "You learned the %gSonata of Awakening%w!&It stirs the sleeping from&their slumber.",
      "Vous apprenez la %gSonate de l'Éveil%w!&Elle tire les dormeurs&de leur sommeil.",
      "Du lernst die %gSonate des Erwachens%w!&Sie weckt die Schlafenden&aus ihrem Schlummer." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_LULLABY,
      "You learned the %gGoron Lullaby%w!&A soothing melody that lulls&even Gorons to sleep.",
      "Vous apprenez la %gBerceuse Goron%w!&Une mélodie apaisante qui&endort même les Gorons.",
      "Du lernst das %gGoronen-Wiegenlied%w!&Eine sanfte Melodie, die sogar&Goronen einschläfert." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_LULLABY_INTRO,
      "You learned the %gGoron Lullaby Intro%w!&The opening bars of the&Goron's lullaby.",
      "Vous apprenez l'%gIntro de la Berceuse Goron%w!&Les premières mesures de&la berceuse Goron.",
      "Du lernst das %gGoronen-Wiegenlied (Intro)%w!&Die ersten Takte des&Goronen-Wiegenlieds." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_NOVA,
      "You learned the %gNew Wave Bossa Nova%w!&The song that awakens&new life in the bay.",
      "Vous apprenez la %gNouvelle Vague Bossa Nova%w!&Le chant qui éveille&une vie nouvelle dans la baie.",
      "Du lernst die %gNew Wave Bossa Nova%w!&Das Lied, das neues Leben&in der Bucht weckt." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_ELEGY,
      "You learned the %gElegy of Emptiness%w!&It leaves behind a hollow&shell of yourself.",
      "Vous apprenez l'%gÉlégie du Néant%w!&Elle laisse derrière vous&une coquille vide.",
      "Du lernst die %gElegie der Leere%w!&Sie hinterlässt eine hohle&Hülle deiner selbst." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_OATH,
      "You learned the %gOath to Order%w!&The song that calls the&four giants of Termina.",
      "Vous apprenez le %gChant de l'Ordre%w!&Le chant qui appelle les&quatre géants de Termina.",
      "Du lernst den %gSchwur der Ordnung%w!&Das Lied, das die vier&Giganten Terminas ruft." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_SARIA,
      "You learned %gSaria's Song%w!&A melody carried over&from a distant forest.",
      "Vous apprenez le %gChant de Saria%w!&Une mélodie venue d'une&forêt lointaine.",
      "Du lernst %gSarias Lied%w!&Eine Melodie aus einem&fernen Wald." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_EPONA,
      "You learned %gEpona's Song%w!&A tune shared between&a girl and her horse.",
      "Vous apprenez le %gChant d'Epona%w!&Un air partagé entre&une fille et son cheval.",
      "Du lernst %gEponas Lied%w!&Eine Weise zwischen einem&Mädchen und seinem Pferd." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_SOARING,
      "You learned the %gSong of Soaring%w!&Warp swiftly to any owl&statue you have touched.",
      "Vous apprenez le %gChant de l'Envol%w!&Téléportez-vous vers toute&statue-chouette activée.",
      "Du lernst das %gLied des Aufschwungs%w!&Reise flink zu jeder berührten&Eulenstatue." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_STORMS,
      "You learned the %gSong of Storms%w!&Summon rain and thunder&at will.",
      "Vous apprenez le %gChant de l'Orage%w!&Invoquez pluie et tonnerre&à volonté.",
      "Du lernst das %gLied des Sturms%w!&Rufe Regen und Donner&nach Belieben herbei." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_SUN,
      "You learned the %gSun's Song%w!&It turns night to day&and day to night.",
      "Vous apprenez le %gChant du Soleil%w!&Il transforme la nuit en jour&et le jour en nuit.",
      "Du lernst das %gSonnenlied%w!&Es verwandelt Nacht in Tag&und Tag in Nacht." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_TIME,
      "You learned the %gSong of Time%w!&It bends the flow of the&three days of Termina.",
      "Vous apprenez le %gChant du Temps%w!&Il plie le cours des&trois jours de Termina.",
      "Du lernst die %gHymne der Zeit%w!&Sie beugt den Lauf der&drei Tage Terminas." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_HEALING,
      "You learned the %gSong of Healing%w!&It soothes troubled souls&and seals them into masks.",
      "Vous apprenez le %gChant de l'Apaisement%w!&Il apaise les âmes troublées&et les scelle en masques.",
      "Du lernst das %gLied der Heilung%w!&Es beruhigt verstörte Seelen&und bannt sie in Masken." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_DOUBLE_TIME,
      "You learned the %gSong of Double Time%w!&Skip ahead to the next&dawn or dusk.",
      "Vous apprenez le %gChant de l'Accéléré%w!&Sautez à l'aube ou&au crépuscule suivant.",
      "Du lernst das %gLied der doppelten Zeit%w!&Springe zur nächsten&Dämmerung vor." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_SONG_INVERTED_TIME,
      "You learned the %gInverted Song of Time%w!&It slows the passage of&the three days.",
      "Vous apprenez le %gChant du Temps Inversé%w!&Il ralentit l'écoulement&des trois jours.",
      "Du lernst die %gUmgekehrte Hymne der Zeit%w!&Sie verlangsamt den Lauf&der drei Tage." },

    // MM Clawshot expressed in OoT rando for cross-collection. No OoT slot (item=NEI_NO_ITEM) and
    // drawFunc=NULL: it reuses OoT's native hookshot get-item model via OBJECT_GI_HOOKSHOT + GID_HOOKSHOT
    // (GetItemEntry_Draw falls to GetItem_Draw(gid)), same as the MM ocarina-song ports above. This row
    // exists only so Nei_FindByRg supplies the textbox name. Give is a no-op (OoT has no clawshot mechanic).
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_CLAWSHOT, "You got the %gClawshot%w!&A grappling hook from a&distant land.",
      "Vous obtenez le %gGrappin-griffe%w!&Un grappin venu d'une&terre lointaine.",
      "Du hast den %gKlauenhaken%w!&Ein Enterhaken aus einem&fernen Land." },

    // MM owl-statue warp points ported into OoT rando. No OoT slot; all share Randomizer_DrawMmOwlStatue.
    // Rows exist for the get-item model + textbox name. Give is a no-op.
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_CLOCK_TOWN_SOUTH,
      "You reached the %gClock Town Owl Statue%w!&A soaring waypoint in the&heart of Termina.",
      "Vous atteignez la %gStatue-Chouette de Bourg-Clock%w!&Un point d'envol au cœur&de Termina.",
      "Du erreichst die %gEulenstatue (Unruhstadt)%w!&Ein Flugpunkt im Herzen&Terminas." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_GREAT_BAY_COAST,
      "You reached the %gGreat Bay Coast Owl Statue%w!&A soaring waypoint by&the shining sea.",
      "Vous atteignez la %gStatue-Chouette de la Côte de Great Bay%w!&Un point d'envol au bord&de la mer scintillante.",
      "Du erreichst die %gEulenstatue (Große-Bucht-Küste)%w!&Ein Flugpunkt am&glitzernden Meer." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_IKANA_CANYON,
      "You reached the %gIkana Canyon Owl Statue%w!&A soaring waypoint in the&haunted valley.",
      "Vous atteignez la %gStatue-Chouette du Canyon d'Ikana%w!&Un point d'envol dans la&vallée hantée.",
      "Du erreichst die %gEulenstatue (Ikana-Schlucht)%w!&Ein Flugpunkt im&verwunschenen Tal." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_MILK_ROAD,
      "You reached the %gMilk Road Owl Statue%w!&A soaring waypoint on the&road to the ranch.",
      "Vous atteignez la %gStatue-Chouette de la Route du Lait%w!&Un point d'envol sur la&route du ranch.",
      "Du erreichst die %gEulenstatue (Milchstraße)%w!&Ein Flugpunkt auf dem&Weg zur Ranch." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_MOUNTAIN_VILLAGE,
      "You reached the %gMountain Village Owl Statue%w!&A soaring waypoint in the&snowbound village.",
      "Vous atteignez la %gStatue-Chouette du Village Montagnard%w!&Un point d'envol dans le&village enneigé.",
      "Du erreichst die %gEulenstatue (Bergdorf)%w!&Ein Flugpunkt im&verschneiten Dorf." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_SNOWHEAD,
      "You reached the %gSnowhead Owl Statue%w!&A soaring waypoint amid&the frozen peaks.",
      "Vous atteignez la %gStatue-Chouette de Tête-de-Neige%w!&Un point d'envol parmi les&sommets gelés.",
      "Du erreichst die %gEulenstatue (Schneekopf)%w!&Ein Flugpunkt zwischen&den gefrorenen Gipfeln." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_SOUTHERN_SWAMP,
      "You reached the %gSouthern Swamp Owl Statue%w!&A soaring waypoint over&the poisoned marsh.",
      "Vous atteignez la %gStatue-Chouette du Marais du Sud%w!&Un point d'envol au-dessus&du marais empoisonné.",
      "Du erreichst die %gEulenstatue (Südsumpf)%w!&Ein Flugpunkt über dem&vergifteten Sumpf." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_STONE_TOWER,
      "You reached the %gStone Tower Owl Statue%w!&A soaring waypoint beneath&the ancient tower.",
      "Vous atteignez la %gStatue-Chouette de la Tour de Pierre%w!&Un point d'envol au pied&de la tour ancienne.",
      "Du erreichst die %gEulenstatue (Steinturm)%w!&Ein Flugpunkt unter dem&uralten Turm." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_WOODFALL,
      "You reached the %gWoodfall Owl Statue%w!&A soaring waypoint above&the swamp temple.",
      "Vous atteignez la %gStatue-Chouette des Bois Perdus%w!&Un point d'envol au-dessus&du temple du marais.",
      "Du erreichst die %gEulenstatue (Waldfall)%w!&Ein Flugpunkt über dem&Sumpftempel." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmOwlStatue, RG_MM_OWL_ZORA_CAPE,
      "You reached the %gZora Cape Owl Statue%w!&A soaring waypoint along&the rocky cape.",
      "Vous atteignez la %gStatue-Chouette du Cap Zora%w!&Un point d'envol le long&du cap rocheux.",
      "Du erreichst die %gEulenstatue (Zora-Kap)%w!&Ein Flugpunkt entlang des&felsigen Kaps." },

    // Tingle's region maps ported into OoT rando. No OoT slot; all share Randomizer_DrawMmTradeQuest (OPA01).
    // Rows exist for the get-item model + textbox name. Give is a no-op.
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_TINGLE_MAP_CLOCK_TOWN,
      "You got %gTingle's Clock Town Map%w!&A hand-drawn map of&Clock Town and beyond.",
      "Vous obtenez la %gCarte de Bourg-Clock de Tingle%w!&Une carte dessinée à la main&de Bourg-Clock.",
      "Du hast %gTingles Unruhstadt-Karte%w!&Eine handgezeichnete Karte&von Unruhstadt." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_TINGLE_MAP_WOODFALL,
      "You got %gTingle's Woodfall Map%w!&A hand-drawn map of&the Woodfall region.",
      "Vous obtenez la %gCarte des Bois Perdus de Tingle%w!&Une carte dessinée à la main&des Bois Perdus.",
      "Du hast %gTingles Waldfall-Karte%w!&Eine handgezeichnete Karte&der Waldfall-Region." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_TINGLE_MAP_SNOWHEAD,
      "You got %gTingle's Snowhead Map%w!&A hand-drawn map of&the Snowhead region.",
      "Vous obtenez la %gCarte de Tête-de-Neige de Tingle%w!&Une carte dessinée à la main&de Tête-de-Neige.",
      "Du hast %gTingles Schneekopf-Karte%w!&Eine handgezeichnete Karte&der Schneekopf-Region." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_TINGLE_MAP_ROMANI_RANCH,
      "You got %gTingle's Romani Ranch Map%w!&A hand-drawn map of&the ranch and Milk Road.",
      "Vous obtenez la %gCarte du Ranch Romani de Tingle%w!&Une carte dessinée à la main&du ranch et de la Route du "
      "Lait.",
      "Du hast %gTingles Romani-Ranch-Karte%w!&Eine handgezeichnete Karte&der Ranch und Milchstraße." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_TINGLE_MAP_GREAT_BAY,
      "You got %gTingle's Great Bay Map%w!&A hand-drawn map of&the Great Bay region.",
      "Vous obtenez la %gCarte de Great Bay de Tingle%w!&Une carte dessinée à la main&de Great Bay.",
      "Du hast %gTingles Große-Bucht-Karte%w!&Eine handgezeichnete Karte&der Große-Bucht-Region." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmTradeQuest, RG_MM_TINGLE_MAP_STONE_TOWER,
      "You got %gTingle's Stone Tower Map%w!&A hand-drawn map of&the Stone Tower region.",
      "Vous obtenez la %gCarte de la Tour de Pierre de Tingle%w!&Une carte dessinée à la main&de la Tour de Pierre.",
      "Du hast %gTingles Steinturm-Karte%w!&Eine handgezeichnete Karte&der Steinturm-Region." },
    // ── Final MM cross items (third wave). Great Spin + clock halves use the DEFAULT native draw
    // (no setNeiDraw); their rows exist for the get-item textbox name only (drawFunc NULL).
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmGsToken, RG_MM_GS_TOKEN_SWAMP,
      "You got a %cSwamp Gold Skulltula Token%w!&Proof you destroyed a Skulltula&of the Southern Swamp spider house.",
      "Vous obtenez un %cSymbole de Skulltula d'Or du Marais%w!&Preuve de la destruction d'une Skulltula&de la maison "
      "des araignées du marais.",
      "Du hast ein %cSumpf-Skulltula-Symbol%w!&Beweis, dass du eine Skulltula des&Sumpf-Spinnenhauses vernichtet "
      "hast." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmGsToken, RG_MM_GS_TOKEN_OCEAN,
      "You got an %cOcean Gold Skulltula Token%w!&Proof you destroyed a Skulltula&of the Great Bay spider house.",
      "Vous obtenez un %cSymbole de Skulltula d'Or de l'Océan%w!&Preuve de la destruction d'une Skulltula&de la maison "
      "des araignées de la baie.",
      "Du hast ein %cOzean-Skulltula-Symbol%w!&Beweis, dass du eine Skulltula des&Bucht-Spinnenhauses vernichtet "
      "hast." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmFrog, RG_MM_FROG_BLUE,
      "You found the %bBlue Frog%w!&A member of Don Gero's&frog choir returns home.",
      "Vous trouvez la %bGrenouille Bleue%w!&Un membre de la chorale de&Don Gero rentre chez lui.",
      "Du hast den %bBlauen Frosch%w!&Ein Mitglied von Don Geros&Froschchor kehrt heim." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmFrog, RG_MM_FROG_CYAN,
      "You found the %bCyan Frog%w!&A member of Don Gero's&frog choir returns home.",
      "Vous trouvez la %bGrenouille Cyan%w!&Un membre de la chorale de&Don Gero rentre chez lui.",
      "Du hast den %bTürkisen Frosch%w!&Ein Mitglied von Don Geros&Froschchor kehrt heim." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmFrog, RG_MM_FROG_PINK,
      "You found the %rPink Frog%w!&A member of Don Gero's&frog choir returns home.",
      "Vous trouvez la %rGrenouille Rose%w!&Un membre de la chorale de&Don Gero rentre chez lui.",
      "Du hast den %rRosa Frosch%w!&Ein Mitglied von Don Geros&Froschchor kehrt heim." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmFrog, RG_MM_FROG_WHITE,
      "You found the %cWhite Frog%w!&A member of Don Gero's&frog choir returns home.",
      "Vous trouvez la %cGrenouille Blanche%w!&Un membre de la chorale de&Don Gero rentre chez lui.",
      "Du hast den %cWeißen Frosch%w!&Ein Mitglied von Don Geros&Froschchor kehrt heim." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, Randomizer_DrawMmGoldDustBottle, RG_MM_BOTTLE_GOLD_DUST,
      "You got a %yBottle With Gold Dust%w!&Prize of the Goron Races.&A very rare smithing powder.",
      "Vous obtenez une %yBouteille de Poudre d'Or%w!&Prix de la course Goron.&Une poudre de forge très rare.",
      "Du hast eine %yFlasche mit Goldstaub%w!&Preis des Goronen-Rennens.&Ein sehr seltenes Schmiedepulver." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_GREAT_SPIN_ATTACK,
      "You learned the %rGreat Spin Attack%w!&Its true power awaits&in the land of Termina.",
      "Vous apprenez la %rSuper Attaque Tornade%w!&Sa vraie puissance vous attend&sur les terres de Termina.",
      "Du hast die %rGroße Wirbelattacke%w!&Ihre wahre Kraft erwartet&dich im Land Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_TIME_DAY_1,
      "You got %yTime (Day 1)%w!&The First Day opens up&in the land of Termina.",
      "Vous obtenez le %yTemps (Jour 1)%w!&Le Premier Jour s'ouvre&sur les terres de Termina.",
      "Du hast %yZeit (Tag 1)%w!&Der Erste Tag öffnet sich&im Land Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_TIME_DAY_2,
      "You got %yTime (Day 2)%w!&The Second Day opens up&in the land of Termina.",
      "Vous obtenez le %yTemps (Jour 2)%w!&Le Deuxième Jour s'ouvre&sur les terres de Termina.",
      "Du hast %yZeit (Tag 2)%w!&Der Zweite Tag öffnet sich&im Land Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_TIME_DAY_3,
      "You got %yTime (Day 3)%w!&The Final Day opens up&in the land of Termina.",
      "Vous obtenez le %yTemps (Jour 3)%w!&Le Dernier Jour s'ouvre&sur les terres de Termina.",
      "Du hast %yZeit (Tag 3)%w!&Der Letzte Tag öffnet sich&im Land Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_TIME_NIGHT_1,
      "You got %pTime (Night 1)%w!&The Night of the First Day opens&up in the land of Termina.",
      "Vous obtenez le %pTemps (Nuit 1)%w!&La Nuit du Premier Jour s'ouvre&sur les terres de Termina.",
      "Du hast %pZeit (Nacht 1)%w!&Die Nacht des Ersten Tages öffnet&sich im Land Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_TIME_NIGHT_2,
      "You got %pTime (Night 2)%w!&The Night of the Second Day opens&up in the land of Termina.",
      "Vous obtenez le %pTemps (Nuit 2)%w!&La Nuit du Deuxième Jour s'ouvre&sur les terres de Termina.",
      "Du hast %pZeit (Nacht 2)%w!&Die Nacht des Zweiten Tages öffnet&sich im Land Termina." },
    { NEI_NO_ITEM, PLAYER_IA_NONE, PLAYER_MODELGROUP_DEFAULT, NEI_NO_SLOT, AGE_REQ_NONE, NULL, func_8083485C,
      Player_InitDefaultIA, NULL, RG_MM_TIME_NIGHT_3,
      "You got %pTime (Night 3)%w!&The Night of the Final Day opens&up in the land of Termina.",
      "Vous obtenez le %pTemps (Nuit 3)%w!&La Nuit du Dernier Jour s'ouvre&sur les terres de Termina.",
      "Du hast %pZeit (Nacht 3)%w!&Die Nacht des Letzten Tages öffnet&sich im Land Termina." },
};

#define NEI_ITEMS_COUNT (sizeof(sNeiItems) / sizeof(sNeiItems[0]))

// Skijer's NEI
const NeiItem* Nei_FindByItem(int32_t item) {
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].item != NEI_NO_ITEM && sNeiItems[i].item == item) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

// Skijer's NEI
const NeiItem* Nei_FindBySlot(uint8_t slot) {
    if (slot == NEI_NO_SLOT) {
        return NULL;
    }
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].slot == slot) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

// Skijer's NEI
const NeiItem* Nei_FindByRg(int16_t rg) {
    if (rg == NEI_NO_RG) {
        return NULL;
    }
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].rg == rg) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

static const NeiItem* ExtPlayer_FindByIA(int32_t itemAction) {
    for (size_t i = 0; i < NEI_ITEMS_COUNT; i++) {
        if (sNeiItems[i].ia == itemAction) {
            return &sNeiItems[i];
        }
    }
    return NULL;
}

/**
 * Get the PLAYER_IA_xxx value for a given ITEM_xxx value.
 */
int8_t ExtPlayer_GetItemAction(int32_t item) {
    // Handle special cases first
    if (item >= ITEM_NONE_FE) {
        return PLAYER_IA_NONE;
    }
    if (item == ITEM_LAST_USED) {
        return PLAYER_IA_SWORD_CS;
    }
    if (item == ITEM_FISHING_POLE) {
        return PLAYER_IA_FISHING_POLE;
    }

    // Vanilla-IA aliases: custom items that behave as an existing vanilla action.
    // (Their model group / update / init come from the vanilla arrays, so they are
    // intentionally NOT table rows.)
    switch (item) {
        // Bow combos and swords (originally in the expanded vanilla array).
        case ITEM_BOW_ARROW_FIRE:
            return PLAYER_IA_BOW_FIRE;
        case ITEM_BOW_ARROW_ICE:
            return PLAYER_IA_BOW_ICE;
        case ITEM_BOW_ARROW_LIGHT:
            return PLAYER_IA_BOW_LIGHT;
        case ITEM_SWORD_KOKIRI:
            return PLAYER_IA_SWORD_KOKIRI;
        case ITEM_SWORD_MASTER:
            return PLAYER_IA_SWORD_MASTER;
        case ITEM_SWORD_BGS:
            return PLAYER_IA_SWORD_BIGGORON;
        // Four Sword / Trident sit on B as themselves (ExtEquip_SetSlot) and swing as a one-hand
        // sword; their behaviors supply the model — no Kokiri Sword is ever written to the save.
        case ITEM_EXT_SWORD_2:
        case ITEM_EXT_SWORD_3:
            return PLAYER_IA_SWORD_KOKIRI;

        // Chateau Romani (bottle item - drink to activate infinite magic)
        case ITEM_CHATEAU_ROMANI:
            return PLAYER_IA_BOTTLE_POTION_BLUE;

        // Skijer's NEI switchhook rework: the Switch Hook now IS the hookshot (real arms_hook aim/
        // anim/model). It's differentiated only by heldItemId in z_arms_hook.c (swap-on-hit) and by a
        // variant-scaled reach. Routing it here avoids the janky custom PLAYER_IA_SWITCH_HOOK action
        // (which had no player action of its own, so it fell back to a boomerang throw pose and never
        // aimed). Its inventory icon/name/slot still come from its sNeiItems row (looked up by itemId,
        // not IA).
        case ITEM_SWITCH_HOOK:
            return PLAYER_IA_HOOKSHOT;

        // Bottle Randomizer: the EMPTY Bottomless Bottle behaves as an empty bottle (so the vanilla
        // catch action triggers); when filled, SLOT_BOTTLE_4 holds the real content id instead, so
        // this alias only applies to the empty state. Kept as an alias (not a table row's ia) so
        // ExtPlayer_FindByIA(PLAYER_IA_BOTTLE) does NOT shadow normal bottles. The row still supplies
        // the icon/name. Skijer's NEI
        case ITEM_BOTTOMLESS_BOTTLE:
            return PLAYER_IA_BOTTLE;

        // Rito form trigger: behaves as a wearable mask so it lands in z_player.c's
        // `itemAction >= PLAYER_IA_MASK_KEATON && <= PLAYER_IA_MASK_TRUTH` branch,
        // where CustomForms_TrySkinItem already toggles skin forms by ITEM id (and
        // returns before any mask is actually worn — the Keaton mask itself is
        // matched by its own item id, so nothing is shadowed). Alias, not a table
        // row's ia, so ExtPlayer_FindByIA(PLAYER_IA_MASK_KEATON) is untouched.
        // Skijer's NEI
        case ITEM_RITO_MASK:
            return PLAYER_IA_MASK_KEATON;

        // Net: wields 1:1 like the Master Sword (all sword melee via the vanilla IA — normal slashes
        // AND the spin attack, exactly like the Cane of Byrna). Net identity is kept via heldItemId ==
        // ITEM_NET (NOT this IA), so z_player/z_player_lib special-case it to draw the net model instead
        // of the sword (following the hand-bone rotation) and capture at the blade instead of dealing
        // damage. Alias (not a row ia) so it doesn't shadow real swords; the row still supplies the
        // icon/name. Skijer's NEI
        case ITEM_NET:
            return PLAYER_IA_SWORD_MASTER;

        // SW97 Medallion spells (quest medallions → spell IAs)
        case ITEM_MEDALLION_FOREST:
            return PLAYER_IA_MAGIC_SPELL_15;
        case ITEM_MEDALLION_SPIRIT:
            return PLAYER_IA_MAGIC_SPELL_16;
        case ITEM_MEDALLION_SHADOW:
            return PLAYER_IA_MAGIC_SPELL_17;
        case ITEM_MEDALLION_WATER:
            return PLAYER_IA_FARORES_WIND;
        case ITEM_MEDALLION_LIGHT:
            return PLAYER_IA_NAYRUS_LOVE;
        case ITEM_MEDALLION_FIRE:
            return PLAYER_IA_DINS_FIRE;

        // SW97 elemental shots (Skijer's NEI). The element used to be six separate item ids sitting
        // on the C-button; it is a flag now, so the button holds a PLAIN bow/slingshot and the IA is
        // picked here from that flag. Dynamic — cannot be a static table cell.
        //
        // SW97_ELEM_BOMB routes to the Bomb Arrows item action even though ITEM_BOMB_ARROWS never
        // reaches a button: that is what gives it Player_UpperAction_BombArrows / Player_InitBombArrowsIA
        // without an inventory slot.
        case ITEM_BOW: {
            uint8_t elem = Sw97_EffectiveElement(0);
            if (elem == SW97_ELEM_BOMB) {
                return PLAYER_IA_BOMB_ARROWS;
            }
            switch (elem) {
                case SW97_ELEM_FIRE:
                    return PLAYER_IA_BOW_FIRE;
                case SW97_ELEM_ICE:
                    return PLAYER_IA_BOW_ICE;
                case SW97_ELEM_LIGHT:
                    return PLAYER_IA_BOW_LIGHT;
                case SW97_ELEM_DARK:
                    return PLAYER_IA_BOW_0C;
                case SW97_ELEM_SOUL:
                    return PLAYER_IA_BOW_0D;
                case SW97_ELEM_WIND:
                    return PLAYER_IA_BOW_0E;
                default:
                    return PLAYER_IA_BOW;
            }
        }
        case ITEM_SLINGSHOT:
            // The slingshot's element rides its own flag and is decoded in func_80834380; the IA
            // itself never varies, so vanilla seed behavior is untouched.
            return PLAYER_IA_SLINGSHOT;

        default:
            break;
    }

    // Custom items: unified NEI registry. Skijer's NEI
    const NeiItem* desc = Nei_FindByItem(item);
    if (desc != NULL) {
        return (int8_t)desc->ia;
    }

    // For vanilla items, use the original array if within bounds
    if (item < VANILLA_SITEMACTIONS_SIZE) {
        return sItemActions[item];
    }

    // For items in the gap (equipment, songs, quest items, etc.), return NONE
    return PLAYER_IA_NONE;
}

// mods/items/logic/item_cane_of_somaria.c — which of the four canes is in hand.
// CANE_TYPE_ULTRAHAND is 3; see item_cane_of_somaria.h.
uint8_t Cane_GetType(void);
uint8_t Pacci_IsHoldingUltrahand(void); // mods/actors/cane_pacci.c
#ifndef CANE_TYPE_ULTRAHAND
#define CANE_TYPE_ULTRAHAND 3
#endif

/**
 * Get the model group for a given PLAYER_IA_xxx value.
 */
uint8_t ExtPlayer_GetActionModelGroup(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        // Dual Cane: FOUR different things share one row of this table, so the row's
        // static modelGroup cannot be right for all of them. It is resolved from the
        // active cane instead — the same context variable the icon and name already
        // use. Ultrahand is the one that differs: it is a bare-handed gesture, so it
        // must not put Link in the two-handed stance the three staves want.
        if (itemAction == PLAYER_IA_CANE_OF_SOMARIA) {
            if (Cane_GetType() != CANE_TYPE_ULTRAHAND) {
                return PLAYER_MODELGROUP_BGS;
            }
            // Ultrahand: empty-handed while idle, and the HOOKSHOT hold once it has
            // something — that group already poses Link with an arm extended
            // forward, which is exactly the "reaching out at the object" read.
            return Pacci_IsHoldingUltrahand() ? PLAYER_MODELGROUP_HOOKSHOT : PLAYER_MODELGROUP_DEFAULT;
        }
        return desc->modelGroup;
    }

    // For vanilla item actions, use the original array if within bounds
    // Lower bound matters: heldItemAction is s8, so a bad//unknown action arrives NEGATIVE and
    // would index off the FRONT of this table. Skijer's NEI
    if ((itemAction >= 0) && (itemAction < VANILLA_PLAYER_IA_COUNT)) {
        return sActionModelGroups[itemAction];
    }

    return PLAYER_MODELGROUP_DEFAULT;
}

/**
 * Get the update function for a given PLAYER_IA_xxx value.
 */
ItemActionUpdateFunc ExtPlayer_GetItemActionUpdateFunc(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->updateFn;
    }

    // For vanilla item actions, use the original array if within bounds
    // Lower bound matters: heldItemAction is s8, so a bad//unknown action arrives NEGATIVE and
    // would index off the FRONT of this table. Skijer's NEI
    if ((itemAction >= 0) && (itemAction < VANILLA_PLAYER_IA_COUNT)) {
        return sItemActionUpdateFuncs[itemAction];
    }

    return func_8083485C;
}

/**
 * Get the init function for a given PLAYER_IA_xxx value.
 */
ItemActionInitFunc ExtPlayer_GetItemActionInitFunc(int32_t itemAction) {
    const NeiItem* desc = ExtPlayer_FindByIA(itemAction);
    if (desc != NULL) {
        return desc->initFn;
    }

    // For vanilla item actions, use the original array if within bounds
    // Lower bound matters: heldItemAction is s8, so a bad//unknown action arrives NEGATIVE and
    // would index off the FRONT of this table. Skijer's NEI
    if ((itemAction >= 0) && (itemAction < VANILLA_PLAYER_IA_COUNT)) {
        return sItemActionInitFuncs[itemAction];
    }

    return Player_InitDefaultIA;
}
