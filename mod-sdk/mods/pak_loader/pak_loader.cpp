#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMenu/ModUI.h"
#include "soh/ResourceManagerHelpers.h"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
const char* CVarGetString(const char*,const char*);
void CVarSetString(const char*,const char*);
float CVarGetFloat(const char*,float);
void CVarSetFloat(const char*,float);
void CVarSave(void);
int32_t CVarGetInteger(const char*,int32_t);
void CVarSetInteger(const char*,int32_t);
}
#include "model.h"
#include <cstdio>

static const SOHModApi* sApi;
static std::vector<std::unique_ptr<pak::Model>> sModels;
static std::set<std::string> sImported;
static std::string sStatus="Choose a model independently for each appearance. Abilities stay with the current form.";
static bool sScanned=false, sOtherPlayer=false, sRestartNeeded=false;
static std::vector<std::string> sMenuKeys, sMenuLabels;
static const char* const sMenuCVars[]={"gMods.PakLoader.ChildModel", "gMods.PakLoader.AdultModel", "gMods.PakLoader.DeityModel"};
static const char* const sCVars[]={"gMods.PakLoader.Child", "gMods.PakLoader.Adult", "gMods.PakLoader.Deity"};
static const char* const sScaleCVars[]={"gMods.PakLoader.ChildRoot", "gMods.PakLoader.AdultRoot", "gMods.PakLoader.DeityRoot"};

static std::string Lower(std::string s) { for(char& c:s) if(c>='A' && c<='Z') c+=32; return s; }
static std::string Hex(uint32_t n) { char buf[9];snprintf(buf,sizeof(buf),"%08x",n);return buf; }
static pak::Model* Find(const std::string& key) {
    for(auto& m:sModels) if(m->key==key) return m.get(); return nullptr;
}
static bool IsLocal(Player* p) { return gPlayState && p==GET_PLAYER(gPlayState) && !sOtherPlayer; }
static int Slot() {
    if(sOtherPlayer || !gPlayState) return -1;
    const char* form=sApi->GetActiveForm();
    if(form && !strcmp(form,"nei.fierce_deity")) return 2;
    if(form && *form) return -1;
    return LINK_IS_ADULT?1:0;
}
static pak::Model* Selected(int slot) {
    if(slot<0 || slot>2 || !CVarGetInteger("gMods.PakLoader.Enabled",1)) return nullptr;
    auto* model=Find(CVarGetString(sCVars[slot],""));
    if(!model) return nullptr;
    if(!model->ready) {
        try { model->LoadNative(); }
        catch(const std::exception& e) {
            sStatus=model->label+": "+e.what();
            CVarSetString(sCVars[slot],"");
            CVarSetInteger(sMenuCVars[slot],0);
            if(SOH_MOD_API_HAS(sApi,Log)) sApi->Log(sStatus.c_str());
            return nullptr;
        }
    }
    return model;
}
static bool NativePath(const char* path,void*) {
    std::string p(path), lower=Lower(p);
    if(lower.find("alt/")==0 || lower.find("link")==std::string::npos || lower.find("skel")==std::string::npos ||
       lower.find("limb")!=std::string::npos) return true;
    bool deity=lower.find("fiercedeity")!=std::string::npos || lower.find("fierce_deity")!=std::string::npos;
    bool human=lower.find("linkhuman")!=std::string::npos;
    bool child=human || lower.find("linkchild")!=std::string::npos;
    bool adult=lower.find("linkadult")!=std::string::npos;
    if(!deity && !child && !adult) return true;
    std::string key="otr:"+p;
    if(Find(key)) return true;
    auto m=std::make_unique<pak::Model>();m->key=key;m->path=p;m->native=true;m->child=child;m->deity=deity;
    m->label=std::string(deity?"[MM FD] ":human?"[MM Child] ":child?"[OoT Child] ":"[OoT Adult] ")+p;
    size_t end=p.rfind('/');m->prefix=end==std::string::npos?"":p.substr(0,end+1);
    sModels.push_back(std::move(m));return true;
}
static std::string JsonString(const std::string& text,const std::string& key) {
    size_t p=text.find('"'+key+'"');if(p==std::string::npos) return "";
    p=text.find(':',p+key.size()+2);if(p==std::string::npos) return "";
    ++p;while(p<text.size() && (text[p]==' ' || text[p]=='\n' || text[p]=='\r' || text[p]=='\t')) ++p;
    if(p==text.size()) return "";
    if(text[p]=='[') { size_t end=text.find(']',p);return end==std::string::npos?"":JsonString(text.substr(p,end-p),"file"); }
    if(text[p]!='"') return "";
    std::string out;
    for(++p;p<text.size();++p) {
        if(text[p]=='"') return out;
        if(text[p]=='\\') { if(++p==text.size()) return ""; }
        out+=text[p];
    }
    return "";
}
static size_t Import(const std::string& name,const uint8_t* data,size_t size) {
    const std::string hash=Hex(pak::Crc(data,size));
    if(sImported.count(hash)) return 0;
    std::vector<pak::File> files;
    bool raw=Lower(name).ends_with(".zobj");
    const std::string sourceName=name.substr(name.find_last_of('/')+1);
    if(raw) files.push_back({sourceName,pak::Bytes(data,data+size)}); else files=pak::Read(data,size);
    std::string json;
    for(auto& f:files) if(Lower(f.name).ends_with("package.json")) json.assign(f.data.begin(),f.data.end());
    size_t count=0;std::string failure;
    for(auto& f:files) {
        auto lower=Lower(f.name);if(!lower.ends_with(".zobj")) continue;
        // These are body imports. Equipment-only and nonhuman forms have different contracts.
        bool child=lower.find("child")!=std::string::npos || lower.find("human")!=std::string::npos;
        bool deity=lower.find("deity")!=std::string::npos || lower.find("fierce")!=std::string::npos;
        for(const char* key:{"child_model","human_model","adult_model","fierce_deity_model","deity_model"}) {
            auto referenced=JsonString(json,key);
            if(!referenced.empty() && (f.name==referenced || f.name.ends_with("/"+referenced))) {
                child=!strcmp(key,"child_model") || !strcmp(key,"human_model");
                deity=!strcmp(key,"fierce_deity_model") || !strcmp(key,"deity_model");break;
            }
        }
        auto m=std::make_unique<pak::Model>();m->key="pak:"+hash+":"+f.name;
        m->label="[PAK "+std::string(deity?"MM FD":child?"Child":"Adult")+"] "+sourceName+" / "+f.name;
        m->child=child;m->deity=deity;m->bytes=std::move(f.data);
        try {m->LoadZobj();sModels.push_back(std::move(m));++count;}
        catch(const std::exception& e) {failure=f.name+": "+e.what();}
    }
    if(!count) throw std::runtime_error(failure.empty()?"No supported Link ZOBJ found in this PAK":failure);
    sImported.insert(hash);return count;
}
static bool StoredPak(const char* path,const uint8_t* data,uint64_t size,void*) {
    try { if(size<=pak::Limit) Import(path,data,size_t(size)); }
    catch(const std::exception& e) {sStatus=std::string(path)+": "+e.what();}
    return true;
}
static void Scan() {
    sApi->ListArchiveFiles("*Skel*",NativePath,nullptr);
    sApi->ReadArchiveFiles("pak_loader/imports/*",StoredPak,nullptr);
    sScanned=true;
}
static void PickPak() {
    SOHUserFile file{};file.structSize=sizeof(file);
    if(!sApi->PickUserFile("Import a Link body model","ModLoader64 models","*.pak *.zobj",&file)) return;
    try {
        pak::Require(file.size<=pak::Limit,"Model exceeds 32 MiB");
        size_t added=Import(file.name,file.data,size_t(file.size));
        const std::string hash=Hex(pak::Crc(file.data,size_t(file.size)));
        const std::string path="pak_loader/imports/"+hash+"/"+file.name;
        auto archive=pak::Wrap(path,file.data,size_t(file.size));
        bool saved=sApi->SaveToModsFolder(("pakloader_import_"+hash+".o2r").c_str(),archive.data(),archive.size());
        sRestartNeeded |= saved;
        sStatus=std::to_string(added)+" models imported. "+(saved?"Saved. Restart Unbound to update the model lists.":"Not saved. Import again and allow saving to add these models to the lists.");
    } catch(const std::exception& e) {sStatus=e.what();}
    sApi->FreeUserFile(&file);
}
static Gfx* Alias(pak::Model& m,uint32_t key) {
    auto it=m.aliases.find(key);return it==m.aliases.end()?nullptr:it->second;
}
static bool Has(const std::string& s,const char* part) { return s.find(part)!=std::string::npos; }
static Gfx* Equipment(pak::Model& m,Player* player,int limb,const std::string& original) {
    if(limb==PLAYER_LIMB_L_HAND) {
        uint32_t key=Has(original,"Bottle")?0x50a8:Has(original,"Bgs")?0x5458:Has(original,"GiantsKnife")?0x54f0:
            Has(original,"HoldingKokiriSword")?0x5440:Has(original,"HoldingMasterSword")?0x5448:
            Has(original,"Hammer")?0x5460:Has(original,"Closed")?0x50a0:0x5098;
        return Alias(m,key);
    }
    if(limb==PLAYER_LIMB_R_HAND) {
        uint32_t key=Has(original,"Mirror")?0x5478:Has(original,"DekuShield")?0x5468:Has(original,"Hylian")?0x5470:
            Has(original,"Bow")?0x5480:Has(original,"Hookshot")?0x5488:Has(original,"Ocarina")?0x5490:
            Has(original,"Closed")?0x50b8:0x50b0;
        return Alias(m,key);
    }
    if(limb==PLAYER_LIMB_SHEATH) {
        int sword=CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD),shield=player->currentShield;
        bool sheathed=player->sheathType==PLAYER_MODELTYPE_SHEATH_16 || player->sheathType==PLAYER_MODELTYPE_SHEATH_18;
        bool back=player->sheathType==PLAYER_MODELTYPE_SHEATH_18 || player->sheathType==PLAYER_MODELTYPE_SHEATH_19;
        if(back && shield>=1 && shield<=3 && sheathed && sword>=1 && sword<=3) {
            Gfx* dl=Alias(m,0x55c0+(sword-1)*24+(shield-1)*8);
            return dl?dl:Alias(m,0x5400+(sword-1)*24+(shield-1)*8);
        }
        if(back && shield>=1 && shield<=3) return Alias(m,0x53e8+(shield-1)*8);
        if(sword>=1 && sword<=3) return Alias(m,(sheathed?0x53d0:0x50c0)+(sword-1)*8);
    }
    return nullptr;
}
static std::string PathFor(pak::Model& m,const std::string& original) {
    size_t leaf=original.rfind('/');std::string name=leaf==std::string::npos?original:original.substr(leaf+1);
    const std::string from=Has(name,"gLinkChild")?"gLinkChild":Has(name,"gLinkAdult")?"gLinkAdult":"";
    if(from.empty()) return "";
    const std::string to=m.deity?"gLinkFierceDeity":Has(m.path,"gLinkHuman")?"gLinkHuman":m.child?"gLinkChild":"gLinkAdult";
    name.replace(0,from.size(),to);
    if(to=="gLinkHuman" || m.deity) {
        size_t p=name.find("NearDL");if(p!=std::string::npos) name.replace(p,6,"DL");
        p=name.find("FarDL");if(p!=std::string::npos) name.replace(p,5,"DL");
    }
    return m.prefix+name;
}
static bool ResolveBody(Player* player,int32_t limb,Gfx** dl,Gfx*,Vec3f* pos) {
    if(!IsLocal(player) || !dl || limb<=0 || limb>21) return false;
    int slot=Slot();auto* model=Selected(slot);if(!model) return false;
    auto& m=*model;
    if(pos) {
        if(limb==PLAYER_LIMB_ROOT) {
            // The host's FD form contributes its own child root multiplier. Multiplication commutes
            // with FormModel's root hook, so callback ordering cannot change the final result.
            float correction=(slot==2 && !LINK_IS_ADULT)?(3377.f/2376.f):1.f;
            float target=m.rootRatio;
            if(m.child && LINK_IS_ADULT) target*=0.64f;
            if(!m.child && !LINK_IS_ADULT) target/=0.64f;
            float factor=target*CVarGetFloat(sScaleCVars[slot],1.f)/correction;
            pos->x*=factor;pos->y*=factor;pos->z*=factor;
        } else {auto p=m.limbs[limb-1].jointPos;*pos={float(p.x),float(p.y),float(p.z)};}
    }
    // Respect visibility, first-person limb suppression and custom items drawn by other mods.
    if(!*dl) return true;
    std::string original;
    if(ResourceMgr_OTRSigCheck(reinterpret_cast<char*>(*dl))) original=reinterpret_cast<const char*>(*dl);
    bool hand=limb==PLAYER_LIMB_L_HAND || limb==PLAYER_LIMB_R_HAND || limb==PLAYER_LIMB_SHEATH;
    if(m.native && !original.empty()) {
        std::string path=PathFor(m,original);
        if(!path.empty() && sApi->HasResource(path.c_str())) {*dl=ResourceMgr_LoadGfxByName(path.c_str());return true;}
    }
    if(hand) {
        if(Gfx* equip=Equipment(m,player,limb,original)) {*dl=equip;return true;}
        // An occupied hand/sheath keeps the correct OoT item when the pack has no matching variant.
        if(Has(original,"Holding") || Has(original,"Bgs") || Has(original,"GiantsKnife") ||
           Has(original,"Bottle") || Has(original,"Hammer") || Has(original,"Bow") || Has(original,"Hookshot") ||
           Has(original,"Slingshot") || Has(original,"Ocarina") || Has(original,"Boomerang") || limb==PLAYER_LIMB_SHEATH) return true;
    }
    *dl=m.limbs[limb-1].dLists[0]; return true;
}
static bool ResolveFace(const char** eyes,const char** mouth) {
    int slot=Slot();auto* model=Selected(slot);if(!model || !eyes || !mouth) return false;
    auto& m=*model;
    // Interned paths remain alive while queued display lists refer to them.
    static std::set<std::string> paths;
    if(m.native) {
        for(const char** tex:{eyes,mouth}) {
            std::string p=PathFor(m,*tex);
            if(m.deity) p=m.prefix+std::string(tex==eyes?"gLinkFierceDeityEyesTex":"gLinkFierceDeityMouthTex");
            if(!p.empty() && sApi->HasResource(p.c_str())) *tex=paths.insert("__OTR__"+p).first->c_str();
        }
    } else {
        int eye=0,mouthIndex=0;
        static constexpr const char* eyeNames[]={"Open","Half","Closed","Right","Left","Up","Down","Wincing"};
        static constexpr const char* mouthNames[]={"Closed","Half","Open","Smile"};
        for(int i=0;i<8;++i) if(strstr(*eyes,eyeNames[i])) eye=i;
        for(int i=0;i<4;++i) if(strstr(*mouth,mouthNames[i])) mouthIndex=i;
        // zzplayas reserves 0x0000..0x4fff for 8 CI8 eyes and 4 CI8 mouths.
        if(m.bytes.size()>=0x5000) {
            *eyes=reinterpret_cast<const char*>(m.bytes.data()+eye*0x800);
            *mouth=reinterpret_cast<const char*>(m.bytes.data()+0x4000+mouthIndex*0x400);
        }
    }
    return true;
}
static bool FdActive() {return Slot()==2 && Selected(2)!=nullptr;}
static bool FdBody(Player* p,int32_t i,Gfx** d,Gfx* original,Vec3f* pos) {
    return Slot()==2 && ResolveBody(p,i,d,original,pos);
}
static bool FdFace(const char** eyes,const char** mouth) {return Slot()==2 && ResolveFace(eyes,mouth);}
static void Body(Player* p,int32_t i,Gfx** d,Gfx* original,Vec3f* pos) {
    if(Slot()!=2) ResolveBody(p,i,d,original,pos);
}
static void Face(const char** eyes,const char** mouth) {if(Slot()!=2) ResolveFace(eyes,mouth);}
static void Draw(Actor* actor,PlayState* play,bool*) {
    if(actor->id==ACTOR_PLAYER || actor->id==ACTOR_EN_OE2) sOtherPlayer=actor!=&GET_PLAYER(play)->actor;
}
static void DrawEnd(Actor* actor,PlayState*) {
    if(actor->id==ACTOR_PLAYER || actor->id==ACTOR_EN_OE2) sOtherPlayer=false;
}
static void Tint(bool* should,va_list) {if(Selected(Slot())) *should=false;}
// Native SohMenu widgets own model selection, checkbox and slider rendering.
// The ModApi copies combo values at registration; stable string keys remain the persisted source of truth.
static void SyncMenuSelection(int slot) {
    const std::string key=CVarGetString(sCVars[slot],"");
    auto it=std::find(sMenuKeys.begin(),sMenuKeys.end(),key);
    CVarSetInteger(sMenuCVars[slot],it==sMenuKeys.end()?0:int32_t(it-sMenuKeys.begin()));
}
static void ChangeSelection(int slot) {
    int index=CVarGetInteger(sMenuCVars[slot],0);
    if(index<0 || size_t(index)>=sMenuKeys.size()) index=0;
    CVarSetString(sCVars[slot],sMenuKeys[index].c_str());
    Selected(slot);SyncMenuSelection(slot);CVarSave();
}
static void ChildChanged() {ChangeSelection(0);}
static void AdultChanged() {ChangeSelection(1);}
static void DeityChanged() {ChangeSelection(2);}
static void ApplyPreset() {
    const char* keys[]={"otr:objects/object_link_child/gLinkHumanSkel",
                        "otr:objects/object_link_boy/gLinkFierceDeitySkel",
                        "otr:objects/object_link_boy/gLinkAdultSkel"};
    for(const char* key:keys) if(std::find(sMenuKeys.begin(),sMenuKeys.end(),key)==sMenuKeys.end()) {
        sStatus="MM models unavailable. Install mm.o2r through MM Assets and restart Unbound.";return;
    }
    CVarSetInteger("gMods.PakLoader.Enabled",1);
    for(int slot=0;slot<3;++slot) {
        CVarSetString(sCVars[slot],keys[slot]);CVarSetFloat(sScaleCVars[slot],1.f);SyncMenuSelection(slot);
    }
    CVarSave();sStatus="MM Child / MM Fierce Deity / OoT Adult selected. Fierce Deity keeps its abilities.";
}
static void ResetModels() {
    for(int slot=0;slot<3;++slot) {
        CVarSetString(sCVars[slot],"");CVarSetFloat(sScaleCVars[slot],1.f);SyncMenuSelection(slot);
    }
    CVarSave();sStatus="Default appearances and root adjustments restored.";
}
static void InstallArchive() {
    SOHUserFile file{};file.structSize=sizeof(file);
    if(!sApi->PickUserFile("Install a player model archive","Ship archives","*.o2r *.otr *.zip",&file)) return;
    const std::string ext=Lower(file.name).ends_with(".otr")?".otr":".o2r";
    const std::string name="pakloader_models_"+Hex(pak::Crc(file.data,size_t(file.size)))+ext;
    bool saved=sApi->SaveToModsFolder(name.c_str(),file.data,file.size);
    sApi->FreeUserFile(&file);sRestartNeeded |= saved;
    sStatus=saved?"Archive installed. Restart Unbound to update the model lists.":"Archive was not installed.";
}
// ModMenu has no native button type. Only these individual action rows use the host's themed buttons.
static void PresetAction() {if(ModUI_Button("Apply MM Child / MM FD / OoT Adult",-1,0)) ApplyPreset();}
static void ResetAction() {if(ModUI_Button("Restore Default Models",-1,0)) ResetModels();}
static void ImportAction() {if(ModUI_Button("Import PAK / ZOBJ",-1,0)) PickPak();}
static void ArchiveAction() {if(ModUI_Button("Install O2R / OTR",-1,0)) InstallArchive();}
static void StatusRow() {
    ModUI_TextWrapped(sStatus.c_str());
    if(sRestartNeeded && SOH_MOD_API_HAS(sApi,RequestRestart) && ModUI_Button("Restart to Update Model Lists",-1,0))
        sApi->RequestRestart("Load the newly installed player models.");
}
static SOHModMenuWidget Widget(SOHModMenuWidgetType type,const char* label,uint8_t column=0) {
    SOHModMenuWidget w{};w.structSize=sizeof(w);w.section="Mods";w.sidebar="PAK Loader";
    w.type=type;w.label=label;w.column=column;return w;
}
static void RegisterMenu() {
    Scan();sMenuKeys={""};sMenuLabels={"Default Link"};
    for(const auto& m:sModels) {
        sMenuKeys.push_back(m->key);
        const char* base=m->deity?"MM Fierce Deity":Has(m->path,"gLinkHuman")?"MM Child Link":m->child?"OoT Child Link":"OoT Adult Link";
        bool original=m->path.starts_with("objects/object_link_");
        sMenuLabels.push_back(m->native && original?base:m->label);
    }
    std::vector<const char*> values;
    for(const auto& label:sMenuLabels) values.push_back(label.c_str());
    sApi->RegisterMenuSidebar("Mods","PAK Loader","gMods.PakLoader.Page",2);
    auto w=Widget(SOH_MOD_MENU_TEXT,"Custom Player Models");sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_SEPARATOR,"Player Models Separator");sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_CHECKBOX,"Enable Custom Player Model");
    w.cvar="gMods.PakLoader.Enabled";w.defaultInt=1;
    w.tooltip="Replace Link's appearance. Turn off to restore every original model without losing your selections.";
    sApi->RegisterMenuWidget(&w);
    static const char* const labels[]={"Child Link Model","Adult Link Model","Fierce Deity Model"};
    static const char* const tips[]={
        "Choose Child Link's appearance. MM Child Link and adult models can be used here.",
        "Choose Adult Link's appearance, including MM Fierce Deity models. Abilities remain those of Adult Link.",
        "Choose the Fierce Deity form's appearance, including OoT Adult Link models. Requires the Fierce Deity mod; its abilities remain active."};
    static const SOHModMenuCallback changed[]={ChildChanged,AdultChanged,DeityChanged};
    for(int slot=0;slot<3;++slot) {
        SyncMenuSelection(slot);
        w=Widget(SOH_MOD_MENU_COMBOBOX,labels[slot]);w.cvar=sMenuCVars[slot];w.tooltip=tips[slot];
        w.values=values.data();w.valueCount=uint32_t(values.size());w.defaultInt=0;w.onChange=changed[slot];
        sApi->RegisterMenuWidget(&w);
    }
    w=Widget(SOH_MOD_MENU_CUSTOM,"MM Model Preset");w.onChange=PresetAction;sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_CUSTOM,"Restore Models");w.onChange=ResetAction;sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_TEXT,"Model Library",1);sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_SEPARATOR,"Model Library Separator",1);sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_CUSTOM,"Import Model File",1);w.onChange=ImportAction;sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_CUSTOM,"Install Model Archive",1);w.onChange=ArchiveAction;sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_TEXT,"New files appear in the model lists after restarting Unbound.",1);sApi->RegisterMenuWidget(&w);
    w=Widget(SOH_MOD_MENU_SEPARATOR,"Root Adjustment Separator",1);sApi->RegisterMenuWidget(&w);
    static const char* const scaleLabels[]={"Child Link Root Height","Adult Link Root Height","Fierce Deity Root Height"};
    for(int slot=0;slot<3;++slot) {
        w=Widget(SOH_MOD_MENU_FLOAT_SLIDER,scaleLabels[slot],1);w.cvar=sScaleCVars[slot];
        w.defaultFloat=1.f;w.minFloat=0.25f;w.maxFloat=2.5f;
        w.tooltip="Adjust animation root height if the selected model floats or sinks. Default: 1. Does not change abilities or gameplay height.";
        sApi->RegisterMenuWidget(&w);
    }
    w=Widget(SOH_MOD_MENU_CUSTOM,"Model Library Status",1);w.onChange=StatusRow;sApi->RegisterMenuWidget(&w);
}
static void Tick() {if(!sScanned) Scan();}
static const char* const sHooks[]={"OnPlayerResolveLimbDraw","OnPlayerResolveFaceTextures","OnActorDraw","OnActorDrawEnd","OnPlayerUpdate"};
static const SOHModRequirements sRequirements={sizeof(SOHModRequirements),sHooks,5};
extern "C" SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {sApi=api;}
extern "C" SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements() {return &sRequirements;}
extern "C" SOH_MOD_EXPORT void ModInit() {
    if(!SOH_MOD_API_HAS(sApi,ListArchiveFiles) || !SOH_MOD_API_HAS(sApi,PublishService) ||
       !SOH_MOD_API_HAS(sApi,GetActiveForm) || !SOH_MOD_API_HAS(sApi,HasResource) ||
       !SOH_MOD_API_HAS(sApi,RegisterMenuSidebar)) return;
    sApi->PublishService("nei.pak_loader.fd_body.v1",reinterpret_cast<void*>(FdBody));
    sApi->PublishService("nei.pak_loader.fd_face.v1",reinterpret_cast<void*>(FdFace));
    sApi->PublishService("nei.pak_loader.fd_active.v1",reinterpret_cast<void*>(FdActive));
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveLimbDraw,Body);
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveFaceTextures,Face);
    SOH_REGISTER_HOOK(sApi,OnActorDraw,Draw);SOH_REGISTER_HOOK(sApi,OnActorDrawEnd,DrawEnd);
    SOH_REGISTER_HOOK(sApi,OnPlayerUpdate,Tick);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR,Tint);
    RegisterMenu();
}
