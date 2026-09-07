#ifndef RANDODRAW_H
#define RANDODRAW_H
#pragma once

#include "../item-tables/ItemTableTypes.h"

typedef struct PlayState PlayState;

#ifdef __cplusplus
extern "C" {
#endif
void Randomizer_DrawSmallKey(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMap(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawCompass(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawKeyRing(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBossKey(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBeanSprout(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBossSoul(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawDoubleDefense(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMasterSword(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawTriforcePiece(PlayState* play, GetItemEntry getItemEntry);
void Randomizer_DrawTriforcePieceGI(PlayState* play, GetItemEntry getItemEntry);
void Randomizer_DrawOcarinaButton(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBronzeScale(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawPowerBracelet(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawLadder(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawKneePads(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawJabberNut(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawOpenChest(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawFishingPoleGI(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSkeletonKey(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMysteryItem(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBombchuBag(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawOverworldKey(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawRocsFeather(PlayState* play, GetItemEntry* getItemEntry);

// Custom 24 Items - Draw functions (Skijer)
void Randomizer_DrawRocsFeatherSkijer(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawRocsCape(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawWhip(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSpinner(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBombArrows(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawElementalWand(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawFireRod(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawIceRod(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawLightRod(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawDekuLeaf(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSwitchHook(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMogmaMitts(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawGustJar(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBallAndChain(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawCaneOfSomaria(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawDominionRod(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawTimeGate(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBeetle(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawShovel(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawHyliaGrace(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawZonaiPermafrost(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawDemiseDestruction(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMagnesis(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawStasis(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawLantern(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawCryonis(PlayState* play, GetItemEntry* getItemEntry);

// Custom items - Pokeball & Minish Cap
void Randomizer_DrawPokeball(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMinishCap(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMarioMask(PlayState* play, GetItemEntry* getItemEntry);
// Bottle Randomizer Net (RG_NET) — soh.o2r object_nei_net held model as the get-item
void Randomizer_DrawNet(PlayState* play, GetItemEntry* getItemEntry);

// Extended Equipment Get-Item 3D Models
void Randomizer_DrawExtCaneOfByrna(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtFourSword(PlayState* play, GetItemEntry* getItemEntry);
// NEI Weapon Upgrades (progressive weapons)
void Randomizer_DrawProgressiveHammer(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawProgressiveKokiriSword(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawProgressiveMasterSword(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawProgressiveBGS(PlayState* play, GetItemEntry* getItemEntry);
// Per-level identities of the NEI weapon chains (what the progressive resolution hands out).
void Randomizer_DrawRazorSword(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawGildedSword(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawTrueMasterSword(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawGreatFairySword(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawIronKnuckleAxe(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawUltrashot(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawQuartzOfMotion(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawClawshot(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawBottomlessBottle(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawCanePacci(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawCaneSomariaUpgrade(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawCanePacciUpgrade(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawCanePacciUltrahand(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtDivineShield(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtSheikahShield(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtShieldOfIkana(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtMagicCape(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtSpiritBreastplate(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtSagesTunic(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtChampionsTunic(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtPegasusAnklet(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtTrident(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtClimbBoots(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtRocBoots(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawNeiSheikahSlate(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSlateRuneBomb(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSlateRuneMasterCycle(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSlateRuneStasis(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSlateRuneCryonis(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSlateRuneSensor(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSeasonSpring(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSeasonSummer(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSeasonAutumn(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawSeasonWinter(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawNeiPhantomHourglass(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawNeiShadowCrystal(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawNeiRodOfSeasons(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtPendantOfMemories(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawExtWaterDragonScale(PlayState* play, GetItemEntry* getItemEntry);

// MM Mask Get-Item 3D Models (all 24, from mm.o2r)
void Randomizer_DrawMmMask(PlayState* play, GetItemEntry* getItemEntry);

// Chateau Romani bottle (from mm.o2r)
void Randomizer_DrawChateauRomani(PlayState* play, GetItemEntry* getItemEntry);

// Bottle with Magic Mushroom (Mask of Scents reward, uses OOT Odd Mushroom DL)
void Randomizer_DrawBottleWithMagicMushroom(PlayState* play, GetItemEntry* getItemEntry);

// MM Boss Remains (single DL each, dispatches by RG) + Stray Fairy (Flex skeleton), from mm.o2r
void Randomizer_DrawMmRemains(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmStrayFairy(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmSoul(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmTradeQuest(PlayState* play, GetItemEntry* getItemEntry);
// MM owl-statue warp points (single-DL MM owl model from mm.o2r), shared by all 10 owls
void Randomizer_DrawMmOwlStatue(PlayState* play, GetItemEntry* getItemEntry);
// MM time items (6 clock halves): static Clock Town clock-tower face from mm.o2r object_obj_tokeidai,
// day variants at face rotation 0xC000, night variants with sun/moon panel flipped 0x8000
void Randomizer_DrawMmClock(PlayState* play, GetItemEntry* getItemEntry);
// MM per-dungeon items (one shared MM get-item model per type; RG name distinguishes the dungeon)
void Randomizer_DrawMmSmallKey(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmDungeonMap(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmBossKey(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmCompass(PlayState* play, GetItemEntry* getItemEntry);
// Final MM cross items (third wave): Swamp/Ocean GS token (OoT's own token model + MM region flame
// tint), healed frogs (OoT's own object_fr frog skeleton, env-tinted), Bottle with Gold Dust (mm.o2r)
void Randomizer_DrawMmGsToken(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmFrog(PlayState* play, GetItemEntry* getItemEntry);
void Randomizer_DrawMmGoldDustBottle(PlayState* play, GetItemEntry* getItemEntry);

#define GET_ITEM_MYSTERY                                                                                 \
    {                                                                                                    \
        ITEM_NONE_FE, 0, 0, 0, 0, MOD_RANDOMIZER, MOD_RANDOMIZER, ITEM_NONE_FE, 0, false, ITEM_FROM_NPC, \
            ITEM_CATEGORY_JUNK, ITEM_NONE_FE, MOD_RANDOMIZER, (CustomDrawFunc)Randomizer_DrawMysteryItem \
    }
#ifdef __cplusplus
};
#endif

#endif
