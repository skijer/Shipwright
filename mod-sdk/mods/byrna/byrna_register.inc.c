static void ForgetSceneState(int16_t sceneNum) {
    memset(&sByrna,0,sizeof(sByrna));sCollidersReady=false;sFairiesReady=false;
    sTrail=-1;sTrailDrawing=false;memset(sProjectiles,0,sizeof(sProjectiles));
    ResetDamageRecipients();
}
static void PutCaneAway(const char* key) {
    if (gPlayState && sByrna.active) EndMove(gPlayState,GET_PLAYER(gPlayState));
    if (gPlayState && sTrail>=0) Effect_Delete(gPlayState,sTrail);
    ForgetSceneState(0);
}
static const char* const sRequiredHooks[]={"OnPlayerUpdate","OnPlayerActionHandler","OnPlayerFilterInput",
    "OnPlayerResolveAnim","OnPlayerResolveAnimSite","OnPlayerResolveModelGroup","OnPlayerResolveLimbDraw",
    "OnPlayerPostLimbDraw","OnActorDrawEnd","OnActorUpdate","OnActorKill","ShouldActorUpdate","OnSceneInit",
    "OnCollisionRegisterAC","OnCollisionResolveDamage","OnResolveSwordDamage"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sRequiredHooks,ARRAY_COUNT(sRequiredHooks)};
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {sApi=api;}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {return &sRequirements;}
SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomEquipDefinition definition={0};definition.structSize=sizeof(definition);definition.key=BYRNA_KEY;
    definition.iconPath=sIcon;definition.namePath=sName;
    definition.pauseText="%rCane of Byrna&%w%y\xA0%w: slash  Run + %y\xA0%w: dash&%y\x9F%w replaces roll with a vault.^"
        "In air: %y\xA0%w strikes; %y\x9F%w dashes;&%y\xA3%w pounds. Enemy hits rebound.^"
        "Ground %y\xA3%w: magic guard, all sides.&Costs %g2 MP%w every 20 frames.&Other moves use no magic.^"
        "Normal foes: Kokiri Sword power.&Bosses: Fierce Deity power&with Master Sword properties.";
    definition.getItemText="You got the %rCane of Byrna%w!&Slash, vault and rebound&through the air.^"
        "%y\x9F%w while moving vaults;&%y\x9F%w standing still puts it away.&%y\xA3%w guards at %g2 MP%w / 20 frames.";
    definition.slot=SOH_EQUIP_SLOT_SWORD;definition.page=0;definition.row=SOH_EQUIP_SLOT_SWORD;definition.column=1;
    definition.ageRequirement=SOH_CUSTOM_ITEM_AGE_ANY;definition.vanillaBase=EQUIP_VALUE_SWORD_KOKIRI;
    definition.toggles=1;definition.onUnequip=PutCaneAway;definition.getItemEntry.drawFunc=DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) return;
    SOH_REGISTER_HOOK(sApi,OnPlayerUpdate,TickByrna);
    SOH_REGISTER_HOOK_FOR_ID(sApi,OnPlayerActionHandler,SOH_PLAYER_ACTION_MELEE,TakeMeleeButton);
    SOH_REGISTER_HOOK_FOR_ID(sApi,OnPlayerActionHandler,SOH_PLAYER_ACTION_ROLL,TakeAButton);
    SOH_REGISTER_HOOK_FOR_ID(sApi,OnPlayerActionHandler,SOH_PLAYER_ACTION_ZTARGET_A,TakeAButton);
    SOH_REGISTER_HOOK_FOR_ID(sApi,OnPlayerActionHandler,SOH_PLAYER_ACTION_SHIELD,TakeShieldButton);
    SOH_REGISTER_HOOK(sApi,OnPlayerFilterInput,KeepButtonsFromVanilla);
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveAnim,ResolveStaffAnimation);
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveAnimSite,ResolveStaffHop);
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveModelGroup,WieldWithBothHands);
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveLimbDraw,GripCane);
    SOH_REGISTER_HOOK(sApi,OnPlayerPostLimbDraw,DrawCane);
    SOH_REGISTER_HOOK_FOR_ID(sApi,OnActorDrawEnd,ACTOR_PLAYER,DrawByrnaEffects);
    SOH_REGISTER_HOOK(sApi,OnActorUpdate,CacheProjectileColliders);
    SOH_REGISTER_HOOK(sApi,OnActorKill,ForgetProjectile);
    SOH_REGISTER_HOOK(sApi,OnCollisionRegisterAC,CacheDamageAC);
    SOH_REGISTER_HOOK(sApi,OnCollisionResolveDamage,ResolveByrnaDamage);
    SOH_REGISTER_HOOK(sApi,OnResolveSwordDamage,ResolveByrnaSwordDamage);
    SOH_REGISTER_HOOK(sApi,ShouldActorUpdate,PrepareDamageRecipient);
    SOH_REGISTER_HOOK(sApi,OnActorUpdate,FinishDamageRecipient);
    SOH_REGISTER_HOOK(sApi,OnActorKill,ForgetDamageRecipient);
    SOH_REGISTER_HOOK(sApi,ShouldActorUpdate,ReflectProjectile);
    SOH_REGISTER_HOOK(sApi,OnSceneInit,ForgetSceneState);
    sApi->RegisterVB(VB_CHANGE_HELD_ITEM_AND_USE_ITEM,InterceptDraw);
}
