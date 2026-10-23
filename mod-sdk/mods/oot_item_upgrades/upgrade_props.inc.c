
// Function-pointer comparisons must use the game's address, not a DLL import
// thunk. Ordinary calls work either way, but actor actionFunc holds the former.
#ifdef _WIN32
__declspec(dllimport) void EnIshi_Wait(EnIshi*,PlayState*);
__declspec(dllimport) void BgHeavyBlock_Wait(BgHeavyBlock*,PlayState*);
#else
void EnIshi_Wait(EnIshi*,PlayState*);
void BgHeavyBlock_Wait(BgHeavyBlock*,PlayState*);
#endif
void EnIshi_SpawnFragmentsLarge(EnIshi*,PlayState*);
void EnIshi_SpawnDustLarge(EnIshi*,PlayState*);
void EnIshi_DropCollectible(EnIshi*,PlayState*);
void BgHeavyBlock_SpawnPieces(BgHeavyBlock*,PlayState*);
void EnGoroiwa_SpawnFragments(EnGoroiwa*,PlayState*);
typedef struct {Actor* actor;u32 swing;u8 hits;} AxeProp;
static AxeProp sProps[32];
static void ResetProps(void){memset(sProps,0,sizeof(sProps));}
static bool PropHit(Actor* actor,int hits,float range){
 Player* p=GET_PLAYER(gPlayState);AdvanceSwing(p,gPlayState);
 if(Weapon(p)!=CH_HAMMER||!p->meleeWeaponState||Actor_WorldDistXYZToActor(&p->actor,actor)>range||
    ABS((s16)(Actor_WorldYawTowardActor(&p->actor,actor)-p->actor.shape.rot.y))>0x3800)return false;
 AxeProp* slot=NULL;
 for(int i=0;i<ARRAY_COUNT(sProps);i++)if(sProps[i].actor==actor){slot=&sProps[i];break;}
 if(!slot)for(int i=0;i<ARRAY_COUNT(sProps);i++)if(!sProps[i].actor||!IsActorAlive(gPlayState,sProps[i].actor)){slot=&sProps[i];*slot=(AxeProp){actor,UINT32_MAX,0};break;}
 if(!slot||slot->swing==sSwingId)return false;
 slot->swing=sSwingId;slot->hits++;Audio_PlayActorSound2(actor,NA_SE_IT_HAMMER_HIT);
 CollisionCheck_SpawnShieldParticlesMetal(gPlayState,&actor->world.pos);
 if(slot->hits<hits)return false;slot->actor=NULL;return true;
}
static void SmashProp(Actor* a){
 PlayState* play=gPlayState;if(!a->update)return;
 if(a->id==ACTOR_EN_ISHI){
  EnIshi* rock=(EnIshi*)a;if((a->params&1)!=1||rock->actionFunc!=EnIshi_Wait||!PropHit(a,3,90))return;
  // The native drop runs VB_ROCK_DROP_ITEM, which owns shuffled boulder
  // rewards and clears their check identity. A random drop bypasses it.
  EnIshi_DropCollectible(rock,play);
  EnIshi_SpawnFragmentsLarge(rock,play);EnIshi_SpawnDustLarge(rock,play);
  Flags_SetSwitch(play,((a->params>>10)&0x3C)|((a->params>>6)&3));
 }else if(a->id==ACTOR_BG_HEAVY_BLOCK){
  BgHeavyBlock* block=(BgHeavyBlock*)a;int type=a->params&255;
  if(type==HEAVYBLOCK_BIG_PIECE||type==HEAVYBLOCK_SMALL_PIECE||block->actionFunc!=BgHeavyBlock_Wait||!PropHit(a,5,130))return;
  BgHeavyBlock_SpawnPieces(block,play);if(type==HEAVYBLOCK_BREAKABLE)Flags_SetSwitch(play,(a->params>>8)&63);
  for(Actor* other=play->actorCtx.actorLists[ACTORCAT_BG].head;other;other=other->next){int t=other->params&255;
   if(other!=a&&other->id==ACTOR_BG_HEAVY_BLOCK&&other->update&&t!=HEAVYBLOCK_BIG_PIECE&&t!=HEAVYBLOCK_SMALL_PIECE&&Actor_WorldDistXYZToActor(a,other)<=600)Actor_Kill(other);
  }
 }else if(a->id==ACTOR_BG_JYA_BLOCK){if(!PropHit(a,5,130))return;
 }else if(a->id==ACTOR_BG_JYA_GOROIWA){if(!PropHit(a,1,120))return;
 }else if(a->id==ACTOR_EN_GOROIWA){if(!PropHit(a,1,120))return;EnGoroiwa_SpawnFragments((EnGoroiwa*)a,play);
 }else if(a->id==ACTOR_BG_JYA_HAHENIRON){if(a->params!=0||!PropHit(a,1,60))return;
 }else return;
 if(a->id!=ACTOR_EN_ISHI)Item_DropCollectibleRandom(play,a,&a->world.pos,0);
 Audio_PlayActorSound2(a,NA_SE_EV_ROCK_BROKEN);Actor_Kill(a);
}
