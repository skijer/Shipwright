// Native get-item drawing, privately ported from mm/src/code/z_draw.c.
static const ALIGN_ASSET(2) char gGiAdultWalletColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiAdultWalletColorDL";
static const ALIGN_ASSET(2) char gGiAdultWalletRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiAdultWalletRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiAdultWalletRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiAdultWalletRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiAdultWalletStringColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiAdultWalletStringColorDL";
static const ALIGN_ASSET(2) char gGiAllNightMaskEyesDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask06/gGiAllNightMaskEyesDL";
static const ALIGN_ASSET(2) char gGiAllNightMaskFaceDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask06/gGiAllNightMaskFaceDL";
static const ALIGN_ASSET(2) char gGiArrowLargeDL[] = "__OTR__fleet/mm/objects/object_gi_arrow/gGiArrowLargeDL";
static const ALIGN_ASSET(2) char gGiArrowMediumDL[] = "__OTR__fleet/mm/objects/object_gi_arrow/gGiArrowMediumDL";
static const ALIGN_ASSET(2) char gGiArrowSmallDL[] = "__OTR__fleet/mm/objects/object_gi_arrow/gGiArrowSmallDL";
static const ALIGN_ASSET(2) char gGiBeanDL[] = "__OTR__fleet/mm/objects/object_gi_bean/gGiBeanDL";
static const ALIGN_ASSET(2) char gGiBlastMaskDL[] = "__OTR__fleet/mm/objects/object_gi_mask21/gGiBlastMaskDL";
static const ALIGN_ASSET(2) char gGiBlastMaskEmptyDL[] = "__OTR__fleet/mm/objects/object_gi_mask21/gGiBlastMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiBlueRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiBlueRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiBlueRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiBlueRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiBombBag20BagColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBag20BagColorDL";
static const ALIGN_ASSET(2) char gGiBombBag20RingColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBag20RingColorDL";
static const ALIGN_ASSET(2) char gGiBombBag30BagColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBag30BagColorDL";
static const ALIGN_ASSET(2) char gGiBombBag30RingColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBag30RingColorDL";
static const ALIGN_ASSET(2) char gGiBombBag40BagColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBag40BagColorDL";
static const ALIGN_ASSET(2) char gGiBombBag40RingColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBag40RingColorDL";
static const ALIGN_ASSET(2) char gGiBombBagDL[] = "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBagDL";
static const ALIGN_ASSET(2) char gGiBombBagRingDL[] = "__OTR__fleet/mm/objects/object_gi_bombpouch/gGiBombBagRingDL";
static const ALIGN_ASSET(2) char gGiBombDL[] = "__OTR__fleet/mm/objects/object_gi_bomb_1/gGiBombDL";
static const ALIGN_ASSET(2) char gGiBombchuDL[] = "__OTR__fleet/mm/objects/object_gi_bomb_2/gGiBombchuDL";
static const ALIGN_ASSET(2) char gGiBombersNotebookDL[] =
    "__OTR__fleet/mm/objects/object_gi_schedule/gGiBombersNotebookDL";
static const ALIGN_ASSET(2) char gGiBombersNotebookEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_schedule/gGiBombersNotebookEmptyDL";
static const ALIGN_ASSET(2) char gGiBossKeyDL[] = "__OTR__fleet/mm/objects/object_gi_bosskey/gGiBossKeyDL";
static const ALIGN_ASSET(2) char gGiBossKeyGemDL[] = "__OTR__fleet/mm/objects/object_gi_bosskey/gGiBossKeyGemDL";
static const ALIGN_ASSET(2) char gGiBowHandleDL[] = "__OTR__fleet/mm/objects/object_gi_bow/gGiBowHandleDL";
static const ALIGN_ASSET(2) char gGiBowStringDL[] = "__OTR__fleet/mm/objects/object_gi_bow/gGiBowStringDL";
static const ALIGN_ASSET(2) char gGiBremenMaskDL[] = "__OTR__fleet/mm/objects/object_gi_mask20/gGiBremenMaskDL";
static const ALIGN_ASSET(2) char gGiBremenMaskEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask20/gGiBremenMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiBugContainerContentsDL[] =
    "__OTR__fleet/mm/objects/object_gi_insect/gGiBugContainerContentsDL";
static const ALIGN_ASSET(2) char gGiBugContainerGlassDL[] =
    "__OTR__fleet/mm/objects/object_gi_insect/gGiBugContainerGlassDL";
static const ALIGN_ASSET(2) char gGiBunnyHoodDL[] = "__OTR__fleet/mm/objects/object_gi_rabit_mask/gGiBunnyHoodDL";
static const ALIGN_ASSET(2) char gGiBunnyHoodEyesDL[] =
    "__OTR__fleet/mm/objects/object_gi_rabit_mask/gGiBunnyHoodEyesDL";
static const ALIGN_ASSET(2) char gGiCaptainsHatBodyDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask18/gGiCaptainsHatBodyDL";
static const ALIGN_ASSET(2) char gGiCaptainsHatFaceDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask18/gGiCaptainsHatFaceDL";
static const ALIGN_ASSET(2) char gGiChateauRomaniBottleDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_21/gGiChateauRomaniBottleDL";
static const ALIGN_ASSET(2) char gGiChateauRomaniBottleEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_21/gGiChateauRomaniBottleEmptyDL";
static const ALIGN_ASSET(2) char gGiCircusLeaderMaskEyebrowsDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask11/gGiCircusLeaderMaskEyebrowsDL";
static const ALIGN_ASSET(2) char gGiCircusLeaderMaskFaceDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask11/gGiCircusLeaderMaskFaceDL";
static const ALIGN_ASSET(2) char gGiCompassDL[] = "__OTR__fleet/mm/objects/object_gi_compass/gGiCompassDL";
static const ALIGN_ASSET(2) char gGiCompassGlassDL[] = "__OTR__fleet/mm/objects/object_gi_compass/gGiCompassGlassDL";
static const ALIGN_ASSET(2) char gGiCouplesMaskFullDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask13/gGiCouplesMaskFullDL";
static const ALIGN_ASSET(2) char gGiCouplesMaskHalfDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask13/gGiCouplesMaskHalfDL";
static const ALIGN_ASSET(2) char gGiDekuMaskDL[] = "__OTR__fleet/mm/objects/object_gi_nutsmask/gGiDekuMaskDL";
static const ALIGN_ASSET(2) char gGiDekuMaskEmptyDL[] = "__OTR__fleet/mm/objects/object_gi_nutsmask/gGiDekuMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiDonGeroMaskBodyDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask16/gGiDonGeroMaskBodyDL";
static const ALIGN_ASSET(2) char gGiDonGeroMaskFaceDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask16/gGiDonGeroMaskFaceDL";
static const ALIGN_ASSET(2) char gGiDungeonMapDL[] = "__OTR__fleet/mm/objects/object_gi_map/gGiDungeonMapDL";
static const ALIGN_ASSET(2) char gGiEmptyBottleCorkDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle/gGiEmptyBottleCorkDL";
static const ALIGN_ASSET(2) char gGiEmptyBottleGlassDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle/gGiEmptyBottleGlassDL";
static const ALIGN_ASSET(2) char gGiFairyContainerBaseCapDL[] =
    "__OTR__fleet/mm/objects/object_gi_soul/gGiFairyContainerBaseCapDL";
static const ALIGN_ASSET(2) char gGiFairyContainerContentsDL[] =
    "__OTR__fleet/mm/objects/object_gi_soul/gGiFairyContainerContentsDL";
static const ALIGN_ASSET(2) char gGiFairyContainerGlassDL[] =
    "__OTR__fleet/mm/objects/object_gi_soul/gGiFairyContainerGlassDL";
static const ALIGN_ASSET(2) char gGiFierceDeityMaskFaceDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask03/gGiFierceDeityMaskFaceDL";
static const ALIGN_ASSET(2) char gGiFierceDeityMaskHairAndHatDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask03/gGiFierceDeityMaskHairAndHatDL";
static const ALIGN_ASSET(2) char gGiGarosMaskCloakDL[] = "__OTR__fleet/mm/objects/object_gi_mask09/gGiGarosMaskCloakDL";
static const ALIGN_ASSET(2) char gGiGarosMaskFaceDL[] = "__OTR__fleet/mm/objects/object_gi_mask09/gGiGarosMaskFaceDL";
static const ALIGN_ASSET(2) char gGiGiantMaskDL[] = "__OTR__fleet/mm/objects/object_gi_mask23/gGiGiantMaskDL";
static const ALIGN_ASSET(2) char gGiGiantMaskEmptyDL[] = "__OTR__fleet/mm/objects/object_gi_mask23/gGiGiantMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiGiantsWalletColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiGiantsWalletColorDL";
static const ALIGN_ASSET(2) char gGiGiantsWalletRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiGiantsWalletRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiGiantsWalletRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiGiantsWalletRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiGiantsWalletStringColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiGiantsWalletStringColorDL";
static const ALIGN_ASSET(2) char gGiGibdoMaskDL[] = "__OTR__fleet/mm/objects/object_gi_mask15/gGiGibdoMaskDL";
static const ALIGN_ASSET(2) char gGiGibdoMaskEmptyDL[] = "__OTR__fleet/mm/objects/object_gi_mask15/gGiGibdoMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiGildedSwordDL[] = "__OTR__fleet/mm/objects/object_gi_sword_3/gGiGildedSwordDL";
static const ALIGN_ASSET(2) char gGiGildedSwordEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_sword_3/gGiGildedSwordEmptyDL";
static const ALIGN_ASSET(2) char gGiGoldDustPowderDL[] =
    "__OTR__fleet/mm/objects/object_gi_gold_dust/gGiGoldDustPowderDL";
static const ALIGN_ASSET(2) char gGiGoldDustPowderEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_gold_dust/gGiGoldDustPowderEmptyDL";
static const ALIGN_ASSET(2) char gGiGoldRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiGoldRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiGoldRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiGoldRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiGoronMaskDL[] = "__OTR__fleet/mm/objects/object_gi_golonmask/gGiGoronMaskDL";
static const ALIGN_ASSET(2) char gGiGoronMaskEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_golonmask/gGiGoronMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiGreatFairyMaskFaceDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask14/gGiGreatFairyMaskFaceDL";
static const ALIGN_ASSET(2) char gGiGreatFairyMaskLeavesDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask14/gGiGreatFairyMaskLeavesDL";
static const ALIGN_ASSET(2) char gGiGreatFairysSwordBladeDL[] =
    "__OTR__fleet/mm/objects/object_gi_sword_4/gGiGreatFairysSwordBladeDL";
static const ALIGN_ASSET(2) char gGiGreatFairysSwordHiltEmblemDL[] =
    "__OTR__fleet/mm/objects/object_gi_sword_4/gGiGreatFairysSwordHiltEmblemDL";
static const ALIGN_ASSET(2) char gGiGreenRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiGreenRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiGreenRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiGreenRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiHeartBorderDL[] = "__OTR__fleet/mm/objects/object_gi_hearts/gGiHeartBorderDL";
static const ALIGN_ASSET(2) char gGiHeartContainerDL[] = "__OTR__fleet/mm/objects/object_gi_hearts/gGiHeartContainerDL";
static const ALIGN_ASSET(2) char gGiHeartPieceDL[] = "__OTR__fleet/mm/objects/object_gi_hearts/gGiHeartPieceDL";
static const ALIGN_ASSET(2) char gGiHerosShieldDL[] = "__OTR__fleet/mm/objects/object_gi_shield_2/gGiHerosShieldDL";
static const ALIGN_ASSET(2) char gGiHerosShieldEmblemDL[] =
    "__OTR__fleet/mm/objects/object_gi_shield_2/gGiHerosShieldEmblemDL";
static const ALIGN_ASSET(2) char gGiHookshotDL[] = "__OTR__fleet/mm/objects/object_gi_hookshot/gGiHookshotDL";
static const ALIGN_ASSET(2) char gGiHookshotEmptyDL[] = "__OTR__fleet/mm/objects/object_gi_hookshot/gGiHookshotEmptyDL";
static const ALIGN_ASSET(2) char gGiHylianLoachBottleContentsDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_22/gGiHylianLoachBottleContentsDL";
static const ALIGN_ASSET(2) char gGiHylianLoachBottleGlassCorkWaterDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_22/gGiHylianLoachBottleGlassCorkWaterDL";
static const ALIGN_ASSET(2) char gGiHylianLoachDL[] = "__OTR__fleet/mm/objects/object_gi_loach/gGiHylianLoachDL";
static const ALIGN_ASSET(2) char gGiHylianLoachEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_loach/gGiHylianLoachEmptyDL";
static const ALIGN_ASSET(2) char gGiKafeiMaskDL[] = "__OTR__fleet/mm/objects/object_gi_mask05/gGiKafeiMaskDL";
static const ALIGN_ASSET(2) char gGiKafeiMaskEmptyDL[] = "__OTR__fleet/mm/objects/object_gi_mask05/gGiKafeiMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiKamaroMaskDL[] = "__OTR__fleet/mm/objects/object_gi_mask17/gGiKamaroMaskDL";
static const ALIGN_ASSET(2) char gGiKamaroMaskEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask17/gGiKamaroMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiKeatonMaskDL[] = "__OTR__fleet/mm/objects/object_gi_ki_tan_mask/gGiKeatonMaskDL";
static const ALIGN_ASSET(2) char gGiKeatonMaskEyesDL[] =
    "__OTR__fleet/mm/objects/object_gi_ki_tan_mask/gGiKeatonMaskEyesDL";
static const ALIGN_ASSET(2) char gGiKokiriSwordBladeHiltDL[] =
    "__OTR__fleet/mm/objects/object_gi_sword_1/gGiKokiriSwordBladeHiltDL";
static const ALIGN_ASSET(2) char gGiKokiriSwordGuardDL[] =
    "__OTR__fleet/mm/objects/object_gi_sword_1/gGiKokiriSwordGuardDL";
static const ALIGN_ASSET(2) char gGiLensDL[] = "__OTR__fleet/mm/objects/object_gi_glasses/gGiLensDL";
static const ALIGN_ASSET(2) char gGiLensGlassDL[] = "__OTR__fleet/mm/objects/object_gi_glasses/gGiLensGlassDL";
static const ALIGN_ASSET(2) char gGiLetterToKafeiEnvelopeLetterDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve_c_00/gGiLetterToKafeiEnvelopeLetterDL";
static const ALIGN_ASSET(2) char gGiLetterToKafeiInscriptionsDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve_c_00/gGiLetterToKafeiInscriptionsDL";
static const ALIGN_ASSET(2) char gGiLetterToMamaEnvelopeLetterDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve_b_01/gGiLetterToMamaEnvelopeLetterDL";
static const ALIGN_ASSET(2) char gGiLetterToMamaInscriptionsDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve_b_01/gGiLetterToMamaInscriptionsDL";
static const ALIGN_ASSET(2) char gGiMagicArrowAmmoDL[] =
    "__OTR__fleet/mm/objects/object_gi_m_arrow/gGiMagicArrowAmmoDL";
static const ALIGN_ASSET(2) char gGiMagicArrowFireColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_m_arrow/gGiMagicArrowFireColorDL";
static const ALIGN_ASSET(2) char gGiMagicArrowGlowDL[] =
    "__OTR__fleet/mm/objects/object_gi_m_arrow/gGiMagicArrowGlowDL";
static const ALIGN_ASSET(2) char gGiMagicArrowIceColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_m_arrow/gGiMagicArrowIceColorDL";
static const ALIGN_ASSET(2) char gGiMagicArrowLightColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_m_arrow/gGiMagicArrowLightColorDL";
static const ALIGN_ASSET(2) char gGiMagicJarLargeDL[] = "__OTR__fleet/mm/objects/object_gi_magicpot/gGiMagicJarLargeDL";
static const ALIGN_ASSET(2) char gGiMagicJarSmallDL[] = "__OTR__fleet/mm/objects/object_gi_magicpot/gGiMagicJarSmallDL";
static const ALIGN_ASSET(2) char gGiMagicMushroomDL[] =
    "__OTR__fleet/mm/objects/object_gi_magicmushroom/gGiMagicMushroomDL";
static const ALIGN_ASSET(2) char gGiMagicMushroomEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_magicmushroom/gGiMagicMushroomEmptyDL";
static const ALIGN_ASSET(2) char gGiMaskOfScentsFaceDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask22/gGiMaskOfScentsFaceDL";
static const ALIGN_ASSET(2) char gGiMaskOfScentsTeethDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask22/gGiMaskOfScentsTeethDL";
static const ALIGN_ASSET(2) char gGiMaskOfTruthAccentsDL[] =
    "__OTR__fleet/mm/objects/object_gi_truth_mask/gGiMaskOfTruthAccentsDL";
static const ALIGN_ASSET(2) char gGiMaskOfTruthDL[] = "__OTR__fleet/mm/objects/object_gi_truth_mask/gGiMaskOfTruthDL";
static const ALIGN_ASSET(2) char gGiMilkBottleContentsDL[] =
    "__OTR__fleet/mm/objects/object_gi_milk/gGiMilkBottleContentsDL";
static const ALIGN_ASSET(2) char gGiMilkBottleGlassDL[] = "__OTR__fleet/mm/objects/object_gi_milk/gGiMilkBottleGlassDL";
static const ALIGN_ASSET(2) char gGiMirrorShieldDL[] = "__OTR__fleet/mm/objects/object_gi_shield_3/gGiMirrorShieldDL";
static const ALIGN_ASSET(2) char gGiMirrorShieldEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_shield_3/gGiMirrorShieldEmptyDL";
static const ALIGN_ASSET(2) char gGiMoonsTearGlowDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve00/gGiMoonsTearGlowDL";
static const ALIGN_ASSET(2) char gGiMoonsTearItemDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve00/gGiMoonsTearItemDL";
static const ALIGN_ASSET(2) char gGiMoonsTearTexAnim[] =
    "__OTR__fleet/mm/objects/object_gi_reserve00/gGiMoonsTearTexAnim";
static const ALIGN_ASSET(2) char gGiNocturneColorDL[] = "__OTR__fleet/mm/objects/object_gi_melody/gGiNocturneColorDL";
static const ALIGN_ASSET(2) char gGiNutDL[] = "__OTR__fleet/mm/objects/object_gi_nuts/gGiNutDL";
static const ALIGN_ASSET(2) char gGiOcarinaOfTimeDL[] = "__OTR__fleet/mm/objects/object_gi_ocarina/gGiOcarinaOfTimeDL";
static const ALIGN_ASSET(2) char gGiOcarinaOfTimeHolesDL[] =
    "__OTR__fleet/mm/objects/object_gi_ocarina/gGiOcarinaOfTimeHolesDL";
static const ALIGN_ASSET(2) char gGiPendantOfMemoriesDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL";
static const ALIGN_ASSET(2) char gGiPendantOfMemoriesEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve_c_01/gGiPendantOfMemoriesEmptyDL";
static const ALIGN_ASSET(2) char gGiPictoBoxBodyAndLensDL[] =
    "__OTR__fleet/mm/objects/object_gi_camera/gGiPictoBoxBodyAndLensDL";
static const ALIGN_ASSET(2) char gGiPictoBoxFrameDL[] = "__OTR__fleet/mm/objects/object_gi_camera/gGiPictoBoxFrameDL";
static const ALIGN_ASSET(2) char gGiPostmanHatBunnyLogoDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask12/gGiPostmanHatBunnyLogoDL";
static const ALIGN_ASSET(2) char gGiPostmanHatCapDL[] = "__OTR__fleet/mm/objects/object_gi_mask12/gGiPostmanHatCapDL";
static const ALIGN_ASSET(2) char gGiPotionContainerBlueLiquidColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerBlueLiquidColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerBluePatternColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerBluePatternColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerBluePotColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerBluePotColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerGreenLiquidColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerGreenLiquidColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerGreenPatternColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerGreenPatternColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerGreenPotColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerGreenPotColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerLiquidDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerLiquidDL";
static const ALIGN_ASSET(2) char gGiPotionContainerPatternDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerPatternDL";
static const ALIGN_ASSET(2) char gGiPotionContainerPotDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerPotDL";
static const ALIGN_ASSET(2) char gGiPotionContainerRedLiquidColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerRedLiquidColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerRedPatternColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerRedPatternColorDL";
static const ALIGN_ASSET(2) char gGiPotionContainerRedPotColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_liquid/gGiPotionContainerRedPotColorDL";
static const ALIGN_ASSET(2) char gGiPowderKegBarrelDL[] =
    "__OTR__fleet/mm/objects/object_gi_bigbomb/gGiPowderKegBarrelDL";
static const ALIGN_ASSET(2) char gGiPowderKegGoronSkullAndFuseDL[] =
    "__OTR__fleet/mm/objects/object_gi_bigbomb/gGiPowderKegGoronSkullAndFuseDL";
static const ALIGN_ASSET(2) char gGiPreludeColorDL[] = "__OTR__fleet/mm/objects/object_gi_melody/gGiPreludeColorDL";
static const ALIGN_ASSET(2) char gGiPurpleRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiPurpleRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiPurpleRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiPurpleRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiQuiver30InnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiver30InnerColorDL";
static const ALIGN_ASSET(2) char gGiQuiver30OuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiver30OuterColorDL";
static const ALIGN_ASSET(2) char gGiQuiver40InnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiver40InnerColorDL";
static const ALIGN_ASSET(2) char gGiQuiver40OuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiver40OuterColorDL";
static const ALIGN_ASSET(2) char gGiQuiver50InnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiver50InnerColorDL";
static const ALIGN_ASSET(2) char gGiQuiver50OuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiver50OuterColorDL";
static const ALIGN_ASSET(2) char gGiQuiverInnerDL[] = "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiverInnerDL";
static const ALIGN_ASSET(2) char gGiQuiverOuterDL[] = "__OTR__fleet/mm/objects/object_gi_arrowcase/gGiQuiverOuterDL";
static const ALIGN_ASSET(2) char gGiRazorSwordDL[] = "__OTR__fleet/mm/objects/object_gi_sword_2/gGiRazorSwordDL";
static const ALIGN_ASSET(2) char gGiRazorSwordEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_sword_2/gGiRazorSwordEmptyDL";
static const ALIGN_ASSET(2) char gGiRecoveryHeartDL[] = "__OTR__fleet/mm/objects/object_gi_heart/gGiRecoveryHeartDL";
static const ALIGN_ASSET(2) char gGiRedPotionBottleDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_red/gGiRedPotionBottleDL";
static const ALIGN_ASSET(2) char gGiRedPotionBottleEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_red/gGiRedPotionBottleEmptyDL";
static const ALIGN_ASSET(2) char gGiRedRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiRedRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiRedRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiRedRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiRequiemColorDL[] = "__OTR__fleet/mm/objects/object_gi_melody/gGiRequiemColorDL";
static const ALIGN_ASSET(2) char gGiRomaniMaskCapDL[] = "__OTR__fleet/mm/objects/object_gi_mask10/gGiRomaniMaskCapDL";
static const ALIGN_ASSET(2) char gGiRomaniMaskNoseEyeDL[] =
    "__OTR__fleet/mm/objects/object_gi_mask10/gGiRomaniMaskNoseEyeDL";
static const ALIGN_ASSET(2) char gGiRoomKeyDL[] = "__OTR__fleet/mm/objects/object_gi_reserve_b_00/gGiRoomKeyDL";
static const ALIGN_ASSET(2) char gGiRoomKeyEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve_b_00/gGiRoomKeyEmptyDL";
static const ALIGN_ASSET(2) char gGiRupeeInnerDL[] = "__OTR__fleet/mm/objects/object_gi_rupy/gGiRupeeInnerDL";
static const ALIGN_ASSET(2) char gGiRupeeOuterDL[] = "__OTR__fleet/mm/objects/object_gi_rupy/gGiRupeeOuterDL";
static const ALIGN_ASSET(2) char gGiSeahorseBottleEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_16/gGiSeahorseBottleEmptyDL";
static const ALIGN_ASSET(2) char gGiSeahorseBottleGlassAndCorkDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_16/gGiSeahorseBottleGlassAndCorkDL";
static const ALIGN_ASSET(2) char gGiSerenadeColorDL[] = "__OTR__fleet/mm/objects/object_gi_melody/gGiSerenadeColorDL";
static const ALIGN_ASSET(2) char gGiSilverRupeeInnerColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiSilverRupeeInnerColorDL";
static const ALIGN_ASSET(2) char gGiSilverRupeeOuterColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_rupy/gGiSilverRupeeOuterColorDL";
static const ALIGN_ASSET(2) char gGiSkulltulaTokenDL[] = "__OTR__fleet/mm/objects/object_gi_sutaru/gGiSkulltulaTokenDL";
static const ALIGN_ASSET(2) char gGiSkulltulaTokenFlameDL[] =
    "__OTR__fleet/mm/objects/object_gi_sutaru/gGiSkulltulaTokenFlameDL";
static const ALIGN_ASSET(2) char gGiSmallKeyDL[] = "__OTR__fleet/mm/objects/object_gi_key/gGiSmallKeyDL";
static const ALIGN_ASSET(2) char gGiSongNoteDL[] = "__OTR__fleet/mm/objects/object_gi_melody/gGiSongNoteDL";
static const ALIGN_ASSET(2) char gGiStickDL[] = "__OTR__fleet/mm/objects/object_gi_stick/gGiStickDL";
static const ALIGN_ASSET(2) char gGiStoneMaskDL[] = "__OTR__fleet/mm/objects/object_gi_stonemask/gGiStoneMaskDL";
static const ALIGN_ASSET(2) char gGiStoneMaskEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_stonemask/gGiStoneMaskEmptyDL";
static const ALIGN_ASSET(2) char gGiStoneOfAgonyDL[] = "__OTR__fleet/mm/objects/object_gi_map/gGiStoneOfAgonyDL";
static const ALIGN_ASSET(2) char gGiSunMaskEyesSidePatternsDL[] =
    "__OTR__fleet/mm/objects/object_gi_mssa/gGiSunMaskEyesSidePatternsDL";
static const ALIGN_ASSET(2) char gGiSunMaskFaceDL[] = "__OTR__fleet/mm/objects/object_gi_mssa/gGiSunMaskFaceDL";
static const ALIGN_ASSET(2) char gGiTingleMapDL[] = "__OTR__fleet/mm/objects/object_gi_fieldmap/gGiTingleMapDL";
static const ALIGN_ASSET(2) char gGiTingleMapEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_fieldmap/gGiTingleMapEmptyDL";
static const ALIGN_ASSET(2) char gGiTitleDeedEmptyDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve01/gGiTitleDeedEmptyDL";
static const ALIGN_ASSET(2) char gGiTitleDeedLandColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve01/gGiTitleDeedLandColorDL";
static const ALIGN_ASSET(2) char gGiTitleDeedMountainColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve01/gGiTitleDeedMountainColorDL";
static const ALIGN_ASSET(2) char gGiTitleDeedOceanColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve01/gGiTitleDeedOceanColorDL";
static const ALIGN_ASSET(2) char gGiTitleDeedSwampColorDL[] =
    "__OTR__fleet/mm/objects/object_gi_reserve01/gGiTitleDeedSwampColorDL";
static const ALIGN_ASSET(2) char gGiWalletDL[] = "__OTR__fleet/mm/objects/object_gi_purse/gGiWalletDL";
static const ALIGN_ASSET(2) char gGiWalletRupeeInnerDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiWalletRupeeInnerDL";
static const ALIGN_ASSET(2) char gGiWalletRupeeOuterDL[] =
    "__OTR__fleet/mm/objects/object_gi_purse/gGiWalletRupeeOuterDL";
static const ALIGN_ASSET(2) char gGiWalletStringDL[] = "__OTR__fleet/mm/objects/object_gi_purse/gGiWalletStringDL";
static const ALIGN_ASSET(2) char gGiZoraEggBottleContentsDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_15/gGiZoraEggBottleContentsDL";
static const ALIGN_ASSET(2) char gGiZoraEggBottleGlassAndCorkDL[] =
    "__OTR__fleet/mm/objects/object_gi_bottle_15/gGiZoraEggBottleGlassAndCorkDL";
static const ALIGN_ASSET(2) char gGiZoraMaskDL[] = "__OTR__fleet/mm/objects/object_gi_zoramask/gGiZoraMaskDL";
static const ALIGN_ASSET(2) char gGiZoraMaskEmptyDL[] = "__OTR__fleet/mm/objects/object_gi_zoramask/gGiZoraMaskEmptyDL";
static const ALIGN_ASSET(2) char gRemainsGohtDL[] = "__OTR__fleet/mm/objects/object_bsmask/gRemainsGohtDL";
static const ALIGN_ASSET(2) char gRemainsGyorgDL[] = "__OTR__fleet/mm/objects/object_bsmask/gRemainsGyorgDL";
static const ALIGN_ASSET(2) char gRemainsOdolwaDL[] = "__OTR__fleet/mm/objects/object_bsmask/gRemainsOdolwaDL";
static const ALIGN_ASSET(2) char gRemainsTwinmoldDL[] = "__OTR__fleet/mm/objects/object_bsmask/gRemainsTwinmoldDL";
static const ALIGN_ASSET(2) char gSkulltulaTokenDL[] = "__OTR__fleet/mm/objects/object_st/gSkulltulaTokenDL";
static const ALIGN_ASSET(2) char gSkulltulaTokenFlameDL[] = "__OTR__fleet/mm/objects/object_st/gSkulltulaTokenFlameDL";
static constexpr int FD_GID_NONE = -1;
static constexpr int FD_GID_BOTTLE = 0;
static constexpr int FD_GID_KEY_SMALL = 1;
static constexpr int FD_GID_MASK_KAMARO = 2;
static constexpr int FD_GID_MASK_COUPLE = 3;
static constexpr int FD_GID_04 = 4;
static constexpr int FD_GID_05 = 5;
static constexpr int FD_GID_06 = 6;
static constexpr int FD_GID_07 = 7;
static constexpr int FD_GID_RECOVERY_HEART = 8;
static constexpr int FD_GID_KEY_BOSS = 9;
static constexpr int FD_GID_COMPASS = 10;
static constexpr int FD_GID_BOMBERS_NOTEBOOK = 11;
static constexpr int FD_GID_MASK_STONE = 12;
static constexpr int FD_GID_MASK_KAFEIS_MASK = 13;
static constexpr int FD_GID_MASK_CIRCUS_LEADER = 14;
static constexpr int FD_GID_MASK_BREMEN = 15;
static constexpr int FD_GID_MASK_ALL_NIGHT = 16;
static constexpr int FD_GID_DEKU_NUTS = 17;
static constexpr int FD_GID_HEART_CONTAINER = 18;
static constexpr int FD_GID_HEART_PIECE = 19;
static constexpr int FD_GID_QUIVER_30 = 20;
static constexpr int FD_GID_QUIVER_40 = 21;
static constexpr int FD_GID_QUIVER_50 = 22;
static constexpr int FD_GID_BOMB_BAG_20 = 23;
static constexpr int FD_GID_BOMB_BAG_30 = 24;
static constexpr int FD_GID_BOMB_BAG_40 = 25;
static constexpr int FD_GID_DEKU_STICK = 26;
static constexpr int FD_GID_DUNGEON_MAP = 27;
static constexpr int FD_GID_MAGIC_JAR_SMALL = 28;
static constexpr int FD_GID_MAGIC_JAR_BIG = 29;
static constexpr int FD_GID_BOMB = 30;
static constexpr int FD_GID_STONE_OF_AGONY = 31;
static constexpr int FD_GID_WALLET_ADULT = 32;
static constexpr int FD_GID_WALLET_GIANT = 33;
static constexpr int FD_GID_MASK_DON_GERO = 34;
static constexpr int FD_GID_ARROWS_SMALL = 35;
static constexpr int FD_GID_ARROWS_MEDIUM = 36;
static constexpr int FD_GID_ARROWS_LARGE = 37;
static constexpr int FD_GID_BOMBCHU = 38;
static constexpr int FD_GID_SHIELD_HERO = 39;
static constexpr int FD_GID_HOOKSHOT = 40;
static constexpr int FD_GID_29 = 41;
static constexpr int FD_GID_OCARINA = 42;
static constexpr int FD_GID_MILK = 43;
static constexpr int FD_GID_MASK_KEATON = 44;
static constexpr int FD_GID_TINGLE_MAP = 45;
static constexpr int FD_GID_BOW = 46;
static constexpr int FD_GID_LENS = 47;
static constexpr int FD_GID_POTION_GREEN = 48;
static constexpr int FD_GID_POTION_RED = 49;
static constexpr int FD_GID_POTION_BLUE = 50;
static constexpr int FD_GID_SHIELD_MIRROR = 51;
static constexpr int FD_GID_MAGIC_BEANS = 52;
static constexpr int FD_GID_FISH = 53;
static constexpr int FD_GID_LETTER_MAMA = 54;
static constexpr int FD_GID_37 = 55;
static constexpr int FD_GID_SWORD_BGS = 56;
static constexpr int FD_GID_MASK_SUN = 57;
static constexpr int FD_GID_MASK_BLAST = 58;
static constexpr int FD_GID_FAIRY = 59;
static constexpr int FD_GID_MASK_SCENTS = 60;
static constexpr int FD_GID_MASK_CAPTAIN = 61;
static constexpr int FD_GID_MASK_BUNNY = 62;
static constexpr int FD_GID_MASK_TRUTH = 63;
static constexpr int FD_GID_DEED_SWAMP = 64;
static constexpr int FD_GID_DEED_MOUNTAIN = 65;
static constexpr int FD_GID_RUPEE_HUGE = 66;
static constexpr int FD_GID_DEED_OCEAN = 67;
static constexpr int FD_GID_MASK_GORON = 68;
static constexpr int FD_GID_MASK_ZORA = 69;
static constexpr int FD_GID_46 = 70;
static constexpr int FD_GID_ARROW_FIRE = 71;
static constexpr int FD_GID_ARROW_ICE = 72;
static constexpr int FD_GID_ARROW_LIGHT = 73;
static constexpr int FD_GID_SKULL_TOKEN = 74;
static constexpr int FD_GID_BUG = 75;
static constexpr int FD_GID_4C = 76;
static constexpr int FD_GID_POE = 77;
static constexpr int FD_GID_FAIRY_2 = 78;
static constexpr int FD_GID_RUPEE_GREEN = 79;
static constexpr int FD_GID_RUPEE_BLUE = 80;
static constexpr int FD_GID_RUPEE_RED = 81;
static constexpr int FD_GID_BIG_POE = 82;
static constexpr int FD_GID_RUPEE_PURPLE = 83;
static constexpr int FD_GID_RUPEE_SILVER = 84;
static constexpr int FD_GID_SWORD_KOKIRI = 85;
static constexpr int FD_GID_SKULL_TOKEN_2 = 86;
static constexpr int FD_GID_57 = 87;
static constexpr int FD_GID_ZORA_EGG = 88;
static constexpr int FD_GID_MOONS_TEAR = 89;
static constexpr int FD_GID_DEED_LAND = 90;
static constexpr int FD_GID_MASK_DEKU = 91;
static constexpr int FD_GID_REMAINS_ODOLWA = 92;
static constexpr int FD_GID_POWDER_KEG = 93;
static constexpr int FD_GID_GOLD_DUST = 94;
static constexpr int FD_GID_SEAHORSE = 95;
static constexpr int FD_GID_60 = 96;
static constexpr int FD_GID_HYLIAN_LOACH = 97;
static constexpr int FD_GID_SEAHORSE_CAUGHT = 98;
static constexpr int FD_GID_REMAINS_GOHT = 99;
static constexpr int FD_GID_REMAINS_GYORG = 100;
static constexpr int FD_GID_REMAINS_TWINMOLD = 101;
static constexpr int FD_GID_SWORD_RAZOR = 102;
static constexpr int FD_GID_SWORD_GILDED = 103;
static constexpr int FD_GID_SWORD_GREAT_FAIRY = 104;
static constexpr int FD_GID_MASK_GARO = 105;
static constexpr int FD_GID_MASK_GREAT_FAIRY = 106;
static constexpr int FD_GID_MASK_GIBDO = 107;
static constexpr int FD_GID_ROOM_KEY = 108;
static constexpr int FD_GID_LETTER_TO_KAFEI = 109;
static constexpr int FD_GID_PENDANT_OF_MEMORIES = 110;
static constexpr int FD_GID_MUSHROOM = 111;
static constexpr int FD_GID_MASK_ROMANI = 112;
static constexpr int FD_GID_MASK_POSTMAN = 113;
static constexpr int FD_GID_MASK_GIANT = 114;
static constexpr int FD_GID_CHATEAU = 115;
static constexpr int FD_GID_PICTOGRAPH_BOX = 116;
static constexpr int FD_GID_MASK_FIERCE_DEITY = 117;
static constexpr int FD_GID_MAXIMUM = 118;

static void FD_GetItem_DrawBombchu(PlayState*, s16);
static void FD_GetItem_DrawCompass(PlayState*, s16);
static void FD_GetItem_DrawDekuNuts(PlayState*, s16);
static void FD_GetItem_DrawFairyBottle(PlayState*, s16);
static void FD_GetItem_DrawMagicArrow(PlayState*, s16);
static void FD_GetItem_DrawMoonsTear(PlayState*, s16);
static void FD_GetItem_DrawOpa0(PlayState*, s16);
static void FD_GetItem_DrawOpa01(PlayState*, s16);
static void FD_GetItem_DrawOpa0Xlu1(PlayState*, s16);
static void FD_GetItem_DrawPotion(PlayState*, s16);
static void FD_GetItem_DrawRecoveryHeart(PlayState*, s16);
static void FD_GetItem_DrawRemains(PlayState*, s16);
static void FD_GetItem_DrawRupee(PlayState*, s16);
static void FD_GetItem_DrawSkullToken(PlayState*, s16);
static void FD_GetItem_DrawSmallRupee(PlayState*, s16);
static void FD_GetItem_DrawUpgrades(PlayState*, s16);
static void FD_GetItem_DrawWallet(PlayState*, s16);
static void FD_GetItem_DrawXlu01(PlayState*, s16);
struct ForeignDrawRow {
    void (*draw)(PlayState*, s16);
    const void* resources[8];
};
static ForeignDrawRow sForeignDrawTable[] = {
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiEmptyBottleCorkDL, (const void*)gGiEmptyBottleGlassDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiSmallKeyDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiKamaroMaskDL, (const void*)gGiKamaroMaskEmptyDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiCouplesMaskFullDL, (const void*)gGiCouplesMaskHalfDL } },
    { FD_GetItem_DrawXlu01, { (const void*)gGiSerenadeColorDL, (const void*)gGiSongNoteDL } },
    { FD_GetItem_DrawXlu01, { (const void*)gGiRequiemColorDL, (const void*)gGiSongNoteDL } },
    { FD_GetItem_DrawXlu01, { (const void*)gGiNocturneColorDL, (const void*)gGiSongNoteDL } },
    { FD_GetItem_DrawXlu01, { (const void*)gGiPreludeColorDL, (const void*)gGiSongNoteDL } },
    { FD_GetItem_DrawRecoveryHeart, { (const void*)gGiRecoveryHeartDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiBossKeyDL, (const void*)gGiBossKeyGemDL } },
    { FD_GetItem_DrawCompass, { (const void*)gGiCompassDL, (const void*)gGiCompassGlassDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiBombersNotebookEmptyDL, (const void*)gGiBombersNotebookDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiStoneMaskEmptyDL, (const void*)gGiStoneMaskDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiKafeiMaskDL, (const void*)gGiKafeiMaskEmptyDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiCircusLeaderMaskFaceDL, (const void*)gGiCircusLeaderMaskEyebrowsDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiBremenMaskDL, (const void*)gGiBremenMaskEmptyDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiAllNightMaskEyesDL, (const void*)gGiAllNightMaskFaceDL } },
    { FD_GetItem_DrawDekuNuts, { (const void*)gGiNutDL } },
    { FD_GetItem_DrawXlu01, { (const void*)gGiHeartBorderDL, (const void*)gGiHeartContainerDL } },
    { FD_GetItem_DrawXlu01, { (const void*)gGiHeartBorderDL, (const void*)gGiHeartPieceDL } },
    { FD_GetItem_DrawUpgrades,
      { (const void*)gGiQuiverInnerDL, (const void*)gGiQuiver30InnerColorDL, (const void*)gGiQuiver30OuterColorDL,
        (const void*)gGiQuiverOuterDL } },
    { FD_GetItem_DrawUpgrades,
      { (const void*)gGiQuiverInnerDL, (const void*)gGiQuiver40InnerColorDL, (const void*)gGiQuiver40OuterColorDL,
        (const void*)gGiQuiverOuterDL } },
    { FD_GetItem_DrawUpgrades,
      { (const void*)gGiQuiverInnerDL, (const void*)gGiQuiver50InnerColorDL, (const void*)gGiQuiver50OuterColorDL,
        (const void*)gGiQuiverOuterDL } },
    { FD_GetItem_DrawUpgrades,
      { (const void*)gGiBombBagDL, (const void*)gGiBombBag20BagColorDL, (const void*)gGiBombBag20RingColorDL,
        (const void*)gGiBombBagRingDL } },
    { FD_GetItem_DrawUpgrades,
      { (const void*)gGiBombBagDL, (const void*)gGiBombBag30BagColorDL, (const void*)gGiBombBag30RingColorDL,
        (const void*)gGiBombBagRingDL } },
    { FD_GetItem_DrawUpgrades,
      { (const void*)gGiBombBagDL, (const void*)gGiBombBag40BagColorDL, (const void*)gGiBombBag40RingColorDL,
        (const void*)gGiBombBagRingDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiStickDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiDungeonMapDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiMagicJarSmallDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiMagicJarLargeDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiBombDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiStoneOfAgonyDL } },
    { FD_GetItem_DrawWallet,
      { (const void*)gGiWalletDL, (const void*)gGiAdultWalletColorDL, (const void*)gGiAdultWalletRupeeOuterColorDL,
        (const void*)gGiWalletRupeeOuterDL, (const void*)gGiAdultWalletStringColorDL, (const void*)gGiWalletStringDL,
        (const void*)gGiAdultWalletRupeeInnerColorDL, (const void*)gGiWalletRupeeInnerDL } },
    { FD_GetItem_DrawWallet,
      { (const void*)gGiWalletDL, (const void*)gGiGiantsWalletColorDL, (const void*)gGiGiantsWalletRupeeOuterColorDL,
        (const void*)gGiWalletRupeeOuterDL, (const void*)gGiGiantsWalletStringColorDL, (const void*)gGiWalletStringDL,
        (const void*)gGiGiantsWalletRupeeInnerColorDL, (const void*)gGiWalletRupeeInnerDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiDonGeroMaskFaceDL, (const void*)gGiDonGeroMaskBodyDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiArrowSmallDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiArrowMediumDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiArrowLargeDL } },
    { FD_GetItem_DrawBombchu, { (const void*)gGiBombchuDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiHerosShieldEmblemDL, (const void*)gGiHerosShieldDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiHookshotEmptyDL, (const void*)gGiHookshotDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiHookshotEmptyDL, (const void*)gGiHookshotDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiOcarinaOfTimeDL, (const void*)gGiOcarinaOfTimeHolesDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiMilkBottleContentsDL, (const void*)gGiMilkBottleGlassDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiKeatonMaskDL, (const void*)gGiKeatonMaskEyesDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiTingleMapDL, (const void*)gGiTingleMapEmptyDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiBowHandleDL, (const void*)gGiBowStringDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiLensDL, (const void*)gGiLensGlassDL } },
    { FD_GetItem_DrawPotion,
      { (const void*)gGiPotionContainerPotDL, (const void*)gGiPotionContainerGreenPotColorDL,
        (const void*)gGiPotionContainerGreenLiquidColorDL, (const void*)gGiPotionContainerLiquidDL,
        (const void*)gGiPotionContainerGreenPatternColorDL, (const void*)gGiPotionContainerPatternDL } },
    { FD_GetItem_DrawPotion,
      { (const void*)gGiPotionContainerPotDL, (const void*)gGiPotionContainerRedPotColorDL,
        (const void*)gGiPotionContainerRedLiquidColorDL, (const void*)gGiPotionContainerLiquidDL,
        (const void*)gGiPotionContainerRedPatternColorDL, (const void*)gGiPotionContainerPatternDL } },
    { FD_GetItem_DrawPotion,
      { (const void*)gGiPotionContainerPotDL, (const void*)gGiPotionContainerBluePotColorDL,
        (const void*)gGiPotionContainerBlueLiquidColorDL, (const void*)gGiPotionContainerLiquidDL,
        (const void*)gGiPotionContainerBluePatternColorDL, (const void*)gGiPotionContainerPatternDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiMirrorShieldEmptyDL, (const void*)gGiMirrorShieldDL } },
    { FD_GetItem_DrawOpa0, { (const void*)gGiBeanDL } },
    { nullptr, {} },
    { FD_GetItem_DrawOpa0Xlu1,
      { (const void*)gGiLetterToMamaEnvelopeLetterDL, (const void*)gGiLetterToMamaInscriptionsDL } },
    { FD_GetItem_DrawOpa0, { nullptr } },
    { nullptr, {} },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiSunMaskFaceDL, (const void*)gGiSunMaskEyesSidePatternsDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiBlastMaskEmptyDL, (const void*)gGiBlastMaskDL } },
    { nullptr, {} },
    { FD_GetItem_DrawOpa01, { (const void*)gGiMaskOfScentsFaceDL, (const void*)gGiMaskOfScentsTeethDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiCaptainsHatBodyDL, (const void*)gGiCaptainsHatFaceDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiBunnyHoodDL, (const void*)gGiBunnyHoodEyesDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiMaskOfTruthDL, (const void*)gGiMaskOfTruthAccentsDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiTitleDeedEmptyDL, (const void*)gGiTitleDeedSwampColorDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiTitleDeedEmptyDL, (const void*)gGiTitleDeedMountainColorDL } },
    { FD_GetItem_DrawRupee,
      { (const void*)gGiRupeeInnerDL, (const void*)gGiGoldRupeeInnerColorDL, (const void*)gGiRupeeOuterDL,
        (const void*)gGiGoldRupeeOuterColorDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiTitleDeedEmptyDL, (const void*)gGiTitleDeedOceanColorDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiGoronMaskEmptyDL, (const void*)gGiGoronMaskDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiZoraMaskEmptyDL, (const void*)gGiZoraMaskDL } },
    { FD_GetItem_DrawBombchu, { nullptr } },
    { FD_GetItem_DrawMagicArrow,
      { (const void*)gGiMagicArrowAmmoDL, (const void*)gGiMagicArrowFireColorDL, (const void*)gGiMagicArrowGlowDL } },
    { FD_GetItem_DrawMagicArrow,
      { (const void*)gGiMagicArrowAmmoDL, (const void*)gGiMagicArrowIceColorDL, (const void*)gGiMagicArrowGlowDL } },
    { FD_GetItem_DrawMagicArrow,
      { (const void*)gGiMagicArrowAmmoDL, (const void*)gGiMagicArrowLightColorDL, (const void*)gGiMagicArrowGlowDL } },
    { FD_GetItem_DrawSkullToken, { (const void*)gGiSkulltulaTokenDL, (const void*)gGiSkulltulaTokenFlameDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiBugContainerContentsDL, (const void*)gGiBugContainerGlassDL } },
    { FD_GetItem_DrawOpa0Xlu1, { nullptr } },
    { nullptr, {} },
    { FD_GetItem_DrawFairyBottle,
      { (const void*)gGiFairyContainerBaseCapDL, (const void*)gGiFairyContainerGlassDL,
        (const void*)gGiFairyContainerContentsDL } },
    { FD_GetItem_DrawSmallRupee,
      { (const void*)gGiRupeeInnerDL, (const void*)gGiGreenRupeeInnerColorDL, (const void*)gGiRupeeOuterDL,
        (const void*)gGiGreenRupeeOuterColorDL } },
    { FD_GetItem_DrawSmallRupee,
      { (const void*)gGiRupeeInnerDL, (const void*)gGiBlueRupeeInnerColorDL, (const void*)gGiRupeeOuterDL,
        (const void*)gGiBlueRupeeOuterColorDL } },
    { FD_GetItem_DrawSmallRupee,
      { (const void*)gGiRupeeInnerDL, (const void*)gGiRedRupeeInnerColorDL, (const void*)gGiRupeeOuterDL,
        (const void*)gGiRedRupeeOuterColorDL } },
    { nullptr, {} },
    { FD_GetItem_DrawRupee,
      { (const void*)gGiRupeeInnerDL, (const void*)gGiPurpleRupeeInnerColorDL, (const void*)gGiRupeeOuterDL,
        (const void*)gGiPurpleRupeeOuterColorDL } },
    { FD_GetItem_DrawRupee,
      { (const void*)gGiRupeeInnerDL, (const void*)gGiSilverRupeeInnerColorDL, (const void*)gGiRupeeOuterDL,
        (const void*)gGiSilverRupeeOuterColorDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiKokiriSwordBladeHiltDL, (const void*)gGiKokiriSwordGuardDL } },
    { FD_GetItem_DrawSkullToken, { (const void*)gSkulltulaTokenDL, (const void*)gSkulltulaTokenFlameDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiRedPotionBottleEmptyDL, (const void*)gGiRedPotionBottleDL } },
    { FD_GetItem_DrawOpa0Xlu1,
      { (const void*)gGiZoraEggBottleContentsDL, (const void*)gGiZoraEggBottleGlassAndCorkDL } },
    { FD_GetItem_DrawMoonsTear, { (const void*)gGiMoonsTearItemDL, (const void*)gGiMoonsTearGlowDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiTitleDeedEmptyDL, (const void*)gGiTitleDeedLandColorDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiDekuMaskEmptyDL, (const void*)gGiDekuMaskDL } },
    { FD_GetItem_DrawRemains, { (const void*)gRemainsOdolwaDL, (const void*)gRemainsOdolwaDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiPowderKegBarrelDL, (const void*)gGiPowderKegGoronSkullAndFuseDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiGoldDustPowderDL, (const void*)gGiGoldDustPowderEmptyDL } },
    { FD_GetItem_DrawOpa0Xlu1,
      { (const void*)gGiSeahorseBottleEmptyDL, (const void*)gGiSeahorseBottleGlassAndCorkDL } },
    { FD_GetItem_DrawOpa0Xlu1,
      { (const void*)gGiHylianLoachBottleContentsDL, (const void*)gGiHylianLoachBottleGlassCorkWaterDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiHylianLoachEmptyDL, (const void*)gGiHylianLoachDL } },
    { nullptr, {} },
    { FD_GetItem_DrawRemains, { (const void*)gRemainsGohtDL, (const void*)gRemainsGohtDL } },
    { FD_GetItem_DrawRemains, { (const void*)gRemainsGyorgDL, (const void*)gRemainsGyorgDL } },
    { FD_GetItem_DrawRemains, { (const void*)gRemainsTwinmoldDL, (const void*)gRemainsTwinmoldDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiRazorSwordDL, (const void*)gGiRazorSwordEmptyDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiGildedSwordDL, (const void*)gGiGildedSwordEmptyDL } },
    { FD_GetItem_DrawOpa0Xlu1,
      { (const void*)gGiGreatFairysSwordBladeDL, (const void*)gGiGreatFairysSwordHiltEmblemDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiGarosMaskCloakDL, (const void*)gGiGarosMaskFaceDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiGreatFairyMaskFaceDL, (const void*)gGiGreatFairyMaskLeavesDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiGibdoMaskEmptyDL, (const void*)gGiGibdoMaskDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiRoomKeyEmptyDL, (const void*)gGiRoomKeyDL } },
    { FD_GetItem_DrawOpa0Xlu1,
      { (const void*)gGiLetterToKafeiEnvelopeLetterDL, (const void*)gGiLetterToKafeiInscriptionsDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiPendantOfMemoriesEmptyDL, (const void*)gGiPendantOfMemoriesDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiMagicMushroomEmptyDL, (const void*)gGiMagicMushroomDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiRomaniMaskCapDL, (const void*)gGiRomaniMaskNoseEyeDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiPostmanHatCapDL, (const void*)gGiPostmanHatBunnyLogoDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiGiantMaskEmptyDL, (const void*)gGiGiantMaskDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiChateauRomaniBottleEmptyDL, (const void*)gGiChateauRomaniBottleDL } },
    { FD_GetItem_DrawOpa0Xlu1, { (const void*)gGiPictoBoxFrameDL, (const void*)gGiPictoBoxBodyAndLensDL } },
    { FD_GetItem_DrawOpa01, { (const void*)gGiFierceDeityMaskFaceDL, (const void*)gGiFierceDeityMaskHairAndHatDL } }
};
static void FD_GetItem_DrawBombchu(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL23_Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawCompass(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_5);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawDekuNuts(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, play->state.frames * 6, play->state.frames * 6,
                                  32, 32, 1, play->state.frames * 6, play->state.frames * 6, 32, 32, 6, 6, 6, 6));
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawFairyBottle(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[1]);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, play->state.frames * 0, play->state.frames * 0,
                                  32, 32, 1, play->state.frames, -(play->state.frames * 6), 32, 320, 0, 0, 1, -6));

    Matrix_Push();
    Matrix_ReplaceRotation(&play->billboardMtxF);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[2]);

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawMagicArrow(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[1]);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[2]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawMoonsTear(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    FleetMaterial(play, gGiMoonsTearTexAnim);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    Matrix_ReplaceRotation(&play->billboardMtxF);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawOpa0(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawOpa01(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawOpa0Xlu1(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawPotion(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, -play->state.frames, play->state.frames, 32, 32,
                                  1, -play->state.frames, play->state.frames, 32, 32, -1, 1, -1, 1));
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[1]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[2]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[3]);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[4]);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[5]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawRecoveryHeart(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, play->state.frames * 0,
                                  -(play->state.frames * 3), 32, 32, 1, play->state.frames * 0,
                                  -(play->state.frames * 2), 32, 32, 0, -3, 0, -2));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[0]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawRemains(PlayState* play, s16 drawId) {
    s32 pad;
    s16 objectSlot;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawRupee(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[1]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[3]);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[2]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawSkullToken(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, play->state.frames * 0,
                                  -(play->state.frames * 5), 32, 32, 1, play->state.frames * 0, play->state.frames * 0,
                                  32, 64, 0, -5, 0, 0));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawSmallRupee(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Scale(0.7f, 0.7f, 0.7f, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[1]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[3]);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[2]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawUpgrades(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[1]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[2]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[3]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawWallet(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[1]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[0]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[2]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[3]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[4]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[5]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[6]);
    gSPDisplayList(POLY_OPA_DISP++, sForeignDrawTable[drawId].resources[7]);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void FD_GetItem_DrawXlu01(PlayState* play, s16 drawId) {
    s32 pad;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[0]);
    gSPDisplayList(POLY_XLU_DISP++, sForeignDrawTable[drawId].resources[1]);

    CLOSE_DISPS(play->state.gfxCtx);
}
