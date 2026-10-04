static void reset(void) {
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(&hook, 0, sizeof(hook));
    memset(&target, 0, sizeof(target));
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    play.actorCtx.actorLists[ACTORCAT_PROP].head = &target;
    player.actor.update = alive;
    target.update = alive;
    target.category = ACTORCAT_PROP;
    target.world.pos = (Vec3f){ 0, 0, 400 };
    target.focus.pos = target.world.pos;
    hook.actionFunc = ArmsHook_Wait;
    sShCharges = 5;
    sShDepletedTimer = 0;
    sShAimManual = 0;
    sSwitchSelection = NULL;
    variant = 4;
    SwitchHook_ClearSwapColliders();
    memset(&playerCollider, 0, sizeof(playerCollider));
    memset(&targetCollider, 0, sizeof(targetCollider));
    playerCollider.base.actor = &player.actor;
    playerCollider.base.shape = COLSHAPE_CYLINDER;
    targetCollider.base.actor = &target;
    targetCollider.base.shape = COLSHAPE_CYLINDER;
    targetCollider.dim.pos.z = 400;
    play.colChkCtx.colOCCount = 2;
    play.colChkCtx.colOC[0] = &playerCollider.base;
    play.colChkCtx.colOC[1] = &targetCollider.base;
}
int main(void) {
    reset();
    // Fire without a preceding held frame: selection must be fresh and the two
    // real actor positions must exchange before any projectile update.
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.z == 400 && target.world.pos.z == 0);
    assert(hook.actionFunc == ArmsHook_SwitchSwap && hook.actor.speedXZ == 0);
    assert(hook.actor.world.pos.z == 400 && hook.unk_1E8.z == 400);
    assert(SwitchHook_GetCharges() == 4);
    assert(playerCollider.dim.pos.z == 400 && targetCollider.dim.pos.z == 0);
    assert(player.actor.prevPos.z == 400 && target.prevPos.z == 0 && target.home.pos.z == 0);
    ArmsHook_SwitchSwap(&hook, &play);
    assert(hook.actionFunc == ArmsHook_SwitchSwap);
    ArmsHook_SwitchSwap(&hook, &play);
    assert(hook.actionFunc == ArmsHook_Wait && player.heldActor == &hook.actor);
    // After the hold, colliders still follow a fall/push-out during settling.
    target.world.pos.y = -15;
    player.actor.world.pos.z = 410;
    ArmsHook_Update(&hook.actor, &play);
    assert(targetCollider.dim.pos.y == -15 && playerCollider.dim.pos.z == 410);
    // Despawning during the hold must release without dereferencing freed memory.
    reset();
    ArmsHook_Wait(&hook, &play);
    play.actorCtx.actorLists[ACTORCAT_PROP].head = NULL;
    sSwapTarget = (Actor*)1;
    ArmsHook_SwitchSwap(&hook, &play);
    assert(hook.actionFunc == ArmsHook_Wait && player.heldActor == &hook.actor);
    // A previous highlighted actor that is no longer in a live list is never read.
    reset();
    play.actorCtx.actorLists[ACTORCAT_PROP].head = NULL;
    sSwitchSelection = (Actor*)1;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.z == 0 && hook.actionFunc == ArmsHook_Wait);
    assert(player.heldActor == &hook.actor && hook.actor.parent == &player.actor);
    // The highlight selector intentionally ignores height. Launch must still
    // reject a distant vertical target without spending a charge.
    reset();
    target.world.pos = (Vec3f){ 0, 2000, 100 };
    target.focus.pos = target.world.pos;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.y == 0 && target.world.pos.y == 2000);
    assert(SwitchHook_GetCharges() == 5 && player.heldActor == &hook.actor);
    assert(hook.actionFunc == ArmsHook_Wait && hook.actor.parent == &player.actor);
    // Range is measured from the launched tip, including its vertical origin.
    reset();
    hook.actor.world.pos.y = 100;
    target.world.pos = (Vec3f){ 0, 500, 300 };
    target.focus.pos = target.world.pos;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.y == 500 && SwitchHook_GetCharges() == 4);
    // Manual aim honors pitch, ignoring the forgiving yaw-only selection.
    reset();
    sShAimManual = 1;
    hook.actor.world.rot.x = -0x2000;
    target.world.pos = (Vec3f){ 0, 200, 200 };
    target.focus.pos = target.world.pos;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.y == 200 && player.actor.world.pos.z == 200);
    assert(!sShAimManual && hook.actionFunc == ArmsHook_SwitchSwap);
    reset();
    sShAimManual = 1;
    hook.actor.world.rot.y = 0x4000;
    target.world.pos = (Vec3f){ 200, 0, 0 };
    target.focus.pos = target.world.pos;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.x == 200 && player.actor.world.pos.z == 0);
    reset();
    sShAimManual = 1;
    target.world.pos.y = 200;
    target.focus.pos = target.world.pos;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.z == 0 && hook.actionFunc == ArmsHook_Wait);
    // Manual aim uses the closest live actor on the ray, not list order.
    reset();
    sShAimManual = 1;
    Actor nearer = target;
    nearer.world.pos.z = 150;
    nearer.focus.pos.z = 150;
    target.next = &nearer;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.z == 150 && target.world.pos.z == 400);
    // Manual aim rejects actors behind Link and beyond longshot reach.
    const float missedDistances[] = { -100, 600 };
    for (int i = 0; i < 2; i++) {
        reset();
        sShAimManual = 1;
        target.world.pos.z = missedDistances[i];
        ArmsHook_Wait(&hook, &play);
        assert(player.actor.world.pos.z == 0 && player.heldActor == &hook.actor);
    }
    reset();
    target.update = NULL;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.z == 0 && hook.actionFunc == ArmsHook_Wait);
    // Five fired attempts spend five charges; a sixth cannot swap.
    reset();
    play.actorCtx.actorLists[ACTORCAT_PROP].head = NULL;
    for (int i = 0; i < 5; i++) {
        hook.actor.parent = NULL;
        ArmsHook_Wait(&hook, &play);
    }
    assert(SwitchHook_GetCharges() == 0);
    play.actorCtx.actorLists[ACTORCAT_PROP].head = &target;
    hook.actor.parent = NULL;
    ArmsHook_Wait(&hook, &play);
    assert(player.actor.world.pos.z == 0 && player.heldActor == &hook.actor);
    // All travelling variants keep their existing launch speeds and lifetimes.
    const float speeds[] = { 20, 20, 40, 15 };
    const int timers[] = { 13, 26, 26, 35 };
    for (int i = 0; i < 4; i++) {
        reset();
        variant = i;
        ArmsHook_Wait(&hook, &play);
        assert(hook.actionFunc == ArmsHook_Shoot && hook.actor.speedXZ == speeds[i] && hook.timer == timers[i]);
        assert(SwitchHook_GetCharges() == 5 && player.actor.world.pos.z == 0);
    }
    puts("PASS: instant swap, manual aim, stale selection, misses, five charges, two-frame hold, other variants");
}
