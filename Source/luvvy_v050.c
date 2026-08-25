
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long DWORD;
typedef int BOOL;
typedef void *LPVOID;
typedef unsigned short wchar_t;
typedef long NTSTATUS;

#define TRUE 1
#define FALSE 0
#define DLL_PROCESS_ATTACH 1
#define PAGE_EXECUTE_READWRITE 0x40

typedef NTSTATUS (__stdcall *NtProtectVirtualMemoryFn)(void*, void**, u32*, u32, u32*);

static int StrEq(char *a, char *b){
    while(*a && *b){ if(*a!=*b)return 0; ++a; ++b; }
    return (*a==0 && *b==0);
}
static void *FindExportInModule(void *base, char *wanted){
    u8 *b=(u8*)base; u32 peOff, exportRva, namesRva, ordsRva, funcsRva, count, i;
    u32 *names,*funcs; unsigned short *ords;
    if(!b || b[0]!='M' || b[1]!='Z') return 0;
    peOff=*(u32*)(b+0x3C);
    if(*(u32*)(b+peOff)!=0x00004550) return 0;
    exportRva=*(u32*)(b+peOff+0x78);
    if(!exportRva) return 0;
    count=*(u32*)(b+exportRva+0x18);
    funcsRva=*(u32*)(b+exportRva+0x1C);
    namesRva=*(u32*)(b+exportRva+0x20);
    ordsRva=*(u32*)(b+exportRva+0x24);
    names=(u32*)(b+namesRva); funcs=(u32*)(b+funcsRva); ords=(unsigned short*)(b+ordsRva);
    for(i=0;i<count;++i){
        char *name=(char*)(b+names[i]);
        if(StrEq(name,wanted)) return (void*)(b+funcs[ords[i]]);
    }
    return 0;
}
static void *ResolveLoadedExport(char *wanted){
    u8 *peb,*ldr,*head,*link;
    __asm {
        mov eax, fs:[30h]
        mov peb, eax
    }
    if(!peb)return 0; ldr=*(u8**)(peb+0x0C); if(!ldr)return 0;
    head=ldr+0x14; link=*(u8**)head;
    while(link && link!=head){
        u8 *entry=link-0x08; void *base=*(void**)(entry+0x18); void *fn=FindExportInModule(base,wanted);
        if(fn)return fn; link=*(u8**)link;
    }
    return 0;
}
static NtProtectVirtualMemoryFn g_NtProtect=0;
static BOOL ProtectMemory(void *address,u32 len,u32 newProtect,u32 *oldProtect){
    void *base=address; u32 size=len,old=0;
    if(!g_NtProtect)g_NtProtect=(NtProtectVirtualMemoryFn)ResolveLoadedExport("NtProtectVirtualMemory");
    if(!g_NtProtect)return FALSE;
    if(g_NtProtect((void*)-1,&base,&size,newProtect,&old)<0)return FALSE;
    if(oldProtect)*oldProtect=old; return TRUE;
}

typedef struct GFxValue {
    void *valueInterface;
    u32 type;
    union { void *data; BOOL boolean; char *str; wchar_t *wstr; double number; } value;
} GFxValue;
typedef struct FunctionParams {
    GFxValue *retValue; void *movie; GFxValue *thisPtr; GFxValue *argsWithThisRef;
    GFxValue *args; int argsCount; void *userData;
} FunctionParams;
typedef struct FunctionHandler { void **vtable; u32 refCount; } FunctionHandler;

typedef void (__thiscall *GFxSetStringWFn)(GFxValue *, wchar_t *);
typedef void (__thiscall *GFxSetBooleanFn)(GFxValue *, BOOL);
typedef BOOL (__thiscall *VIGetMemberFn)(void *, void *, char *, GFxValue *, BOOL);
typedef BOOL (__thiscall *VISetMemberFn)(void *, void *, char *, GFxValue *, BOOL);
typedef BOOL (__thiscall *VIInvokeFn)(void *, void *, GFxValue *, char *, GFxValue *, int, BOOL);
typedef BOOL (__thiscall *VIPushBackFn)(void *, void *, GFxValue *);

static void ZeroValue(GFxValue *v){u8 *p=(u8*)v;u32 i;for(i=0;i<sizeof(*v);++i)p[i]=0;}
static BOOL IsDisplayObject(GFxValue *v){return (v->type & 0x08)?TRUE:FALSE;}
static BOOL ValueGetMember(GFxValue *v,char *name,GFxValue *dest){if(!v||!v->valueInterface)return FALSE;ZeroValue(dest);return ((VIGetMemberFn)0x00DA8AD0)(v->valueInterface,v->value.data,name,dest,IsDisplayObject(v));}
static BOOL ValueSetMember(GFxValue *v,char *name,GFxValue *src){if(!v||!v->valueInterface)return FALSE;return ((VISetMemberFn)0x00DA58F0)(v->valueInterface,v->value.data,name,src,IsDisplayObject(v));}
static BOOL ValueInvoke(GFxValue *v,GFxValue *result,char *name,GFxValue *args,int count){if(!v||!v->valueInterface)return FALSE;return ((VIInvokeFn)0x00DA8230)(v->valueInterface,v->value.data,result,name,args,count,IsDisplayObject(v));}
static BOOL ValuePushBack(GFxValue *v,GFxValue *src){if(!v||!v->valueInterface)return FALSE;return ((VIPushBackFn)0x00DA5A20)(v->valueInterface,v->value.data,src);}
static void SetString(GFxValue *v,wchar_t *s){ZeroValue(v);((GFxSetStringWFn)0x0097B740)(v,s);}
static void SetBoolean(GFxValue *v,BOOL b){ZeroValue(v);((GFxSetBooleanFn)0x0097B6B0)(v,b);}

static BOOL MovieGetVariable(void*movie,GFxValue*out,char*path){
    void **vt; BOOL (__thiscall *fn)(void*,GFxValue*,char*);
    if(!movie)return FALSE; vt=*(void***)movie; if(!vt)return FALSE;
    fn=(void*)vt[0x44/4]; ZeroValue(out); return fn(movie,out,path);
}
static void MovieCreateArray(void*movie,GFxValue*out){
    void **vt=*(void***)movie; void (__thiscall *fn)(void*,GFxValue*)=(void*)vt[0x38/4];
    ZeroValue(out); fn(movie,out);
}
static void MovieCreateObject(void*movie,GFxValue*out){
    void **vt=*(void***)movie; void (__thiscall *fn)(void*,GFxValue*,char*,GFxValue*,int)=(void*)vt[0x34/4];
    ZeroValue(out); fn(movie,out,0,0,0);
}
static void MovieCreateFunction(void*movie,GFxValue*out,FunctionHandler*h,u32 action){
    void **vt=*(void***)movie; void (__thiscall *fn)(void*,GFxValue*,FunctionHandler*,void*)=(void*)vt[0x3C/4];
    ZeroValue(out); fn(movie,out,h,(void*)action);
}

#define MAX_PATCH_HITS 16
typedef struct PatchDef{const u8 *findBytes;u32 findLen;const u8 *patchBytes;u32 patchLen;void *hits[MAX_PATCH_HITS];u32 hitCount;BOOL discovered;BOOL enabled;}PatchDef;
static const u8 P_HEALTH_FIND[]={0x29,0x93,0x44,0x03,0x00,0x00};
static const u8 P_HEALTH_ON[]={0x90,0x90,0x90,0x90,0x90,0x90};
static const u8 P_DETECT_FIND[]={0x0F,0xB6,0x81,0xAC,0x00,0x00,0x00};
static const u8 P_DETECT_ON[]={0x31,0xC0,0x90,0x90,0x90,0x90,0x90};
static const u8 P_MANA_FIND[]={0x8B,0x8E,0x60,0x0A,0x00,0x00};
static const u8 P_MANA_ON[]={0x8B,0x8E,0x64,0x0A,0x00,0x00};
static const u8 P_AMMO_FIND[]={0x89,0x06,0x5F,0xB8,0x01,0x00,0x00,0x00};
static const u8 P_AMMO_ON[]={0x90,0x90};
static const u8 P_POTION_FIND[]={0xFF,0x8C,0x86,0xD4,0x00,0x00,0x00};
static const u8 P_POTION_ON[]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
static PatchDef g_Patches[5]={
 {P_HEALTH_FIND,6,P_HEALTH_ON,6,{0},0,FALSE,FALSE},
 {P_DETECT_FIND,7,P_DETECT_ON,7,{0},0,FALSE,FALSE},
 {P_MANA_FIND,6,P_MANA_ON,6,{0},0,FALSE,FALSE},
 {P_AMMO_FIND,8,P_AMMO_ON,2,{0},0,FALSE,FALSE},
 {P_POTION_FIND,7,P_POTION_ON,7,{0},0,FALSE,FALSE}
};
static BOOL BytesEqual(const u8*a,const u8*b,u32 n){u32 i;for(i=0;i<n;++i)if(a[i]!=b[i])return FALSE;return TRUE;}
static void DiscoverPatch(PatchDef*p){
    u8*begin=(u8*)0x00401000;u32 size=0x00B918EF,i;
    if(p->discovered)return;p->hitCount=0;
    for(i=0;i+p->findLen<=size;++i)if(BytesEqual(begin+i,p->findBytes,p->findLen)){if(p->hitCount<MAX_PATCH_HITS)p->hits[p->hitCount++]=begin+i;i+=p->findLen-1;}
    p->discovered=TRUE;
}
static void WriteBytes(void*address,const u8*bytes,u32 len){
    u32 oldProtect=0,ignored=0,i;if(!address||!len)return;
    if(!ProtectMemory(address,len,PAGE_EXECUTE_READWRITE,&oldProtect))return;
    for(i=0;i<len;++i)((u8*)address)[i]=bytes[i];
    ProtectMemory(address,len,oldProtect,&ignored);
}
static void TogglePatch(u32 index){
    PatchDef*p;u32 i;if(index>=5)return;p=&g_Patches[index];DiscoverPatch(p);if(!p->hitCount)return;
    if(!p->enabled){for(i=0;i<p->hitCount;++i)WriteBytes(p->hits[i],p->patchBytes,p->patchLen);p->enabled=TRUE;}
    else{for(i=0;i<p->hitCount;++i)WriteBytes(p->hits[i],p->findBytes,p->patchLen);p->enabled=FALSE;}
}

enum { ACT_SETMENU=0, ACT_OPEN=1, ACT_HEALTH=2, ACT_DETECT=3, ACT_MANA=4, ACT_AMMO=5, ACT_POTION=6, ACT_BACK=7 };
#define HANDLER_COUNT 8
static void * __thiscall HandlerDeletingDtor(FunctionHandler *self,u32 flags){(void)flags;return self;}
static void __thiscall HandlerCall(FunctionHandler*self,FunctionParams*params);
static void *g_HandlerVTable[2]={(void*)&HandlerDeletingDtor,(void*)&HandlerCall};
static FunctionHandler g_Handlers[HANDLER_COUNT];
static BOOL g_CheatPage=FALSE;
static void *g_FunctionMovie=0;
static BOOL g_FunctionsReady=FALSE;

static char *g_HiddenFns[HANDLER_COUNT]={"_LuvvySetMenu","_LuvvyOpen","_LuvvyHealth","_LuvvyDetect","_LuvvyMana","_LuvvyAmmo","_LuvvyPotion","_LuvvyBack"};

static void InitHandlers(void){u32 i;for(i=0;i<HANDLER_COUNT;++i){g_Handlers[i].vtable=g_HandlerVTable;g_Handlers[i].refCount=0;}}

static BOOL EnsureFunctions(void*movie,GFxValue*pauseMenuMc){
    u32 i; GFxValue fn;
    if(g_FunctionsReady && g_FunctionMovie==movie)return TRUE;
    g_FunctionMovie=movie; g_FunctionsReady=FALSE;
    for(i=0;i<HANDLER_COUNT;++i){
        MovieCreateFunction(movie,&fn,&g_Handlers[i],i);
        if(!fn.valueInterface)return FALSE;
        if(!ValueSetMember(pauseMenuMc,g_HiddenFns[i],&fn))return FALSE;
    }
    g_FunctionsReady=TRUE; return TRUE;
}
static BOOL GetFn(GFxValue*pauseMenuMc,u32 action,GFxValue*out){
    if(action>=HANDLER_COUNT)return FALSE;
    return ValueGetMember(pauseMenuMc,g_HiddenFns[action],out);
}
static void AddButton(void*movie,GFxValue*array,wchar_t*label,GFxValue*callback,BOOL locked){
    GFxValue button,text,lockState;
    MovieCreateObject(movie,&button);
    SetString(&text,label); SetBoolean(&lockState,locked);
    ValueSetMember(&button,"txt",&text);
    ValueSetMember(&button,"callback",callback);
    ValueSetMember(&button,"lockState",&lockState);
    ValuePushBack(array,&button);
}
static BOOL GetOriginalCallback(GFxValue*pauseMenuMc,char*name,GFxValue*out){return ValueGetMember(pauseMenuMc,name,out);}

static void RenderMenu(FunctionParams*params){
    GFxValue array,cb,menuMc,delayed;
    void *movie=params->movie; GFxValue *pause=params->thisPtr;
    if(!movie||!pause)return;
    if(!EnsureFunctions(movie,pause))return;
    MovieCreateArray(movie,&array);

    if(!g_CheatPage){
        if(GetOriginalCallback(pause,"OnResumeClicked",&cb)) AddButton(movie,&array,L"RESUME GAME",&cb,FALSE);
        if(GetOriginalCallback(pause,"OnSaveGameClicked",&cb)) AddButton(movie,&array,L"SAVE GAME",&cb,FALSE);
        if(GetOriginalCallback(pause,"OnLoadGameClicked",&cb)) AddButton(movie,&array,L"LOAD GAME",&cb,FALSE);
        if(GetOriginalCallback(pause,"OnOptionsClicked",&cb)) AddButton(movie,&array,L"OPTIONS",&cb,FALSE);
        if(GetOriginalCallback(pause,"OnMissionStatsClicked",&cb)) AddButton(movie,&array,L"MISSION STATS",&cb,FALSE);
        if(GetFn(pause,ACT_OPEN,&cb)) AddButton(movie,&array,L"LUVVY CHEATS",&cb,FALSE);
        if(GetOriginalCallback(pause,"OnQuitGameClicked",&cb)) AddButton(movie,&array,L"BACK TO MAIN MENU",&cb,FALSE);
        if(GetOriginalCallback(pause,"OnBackToWindowsClicked",&cb)) AddButton(movie,&array,L"EXIT TO WINDOWS",&cb,FALSE);
    } else {
        if(GetFn(pause,ACT_HEALTH,&cb)) AddButton(movie,&array,g_Patches[0].enabled?L"IMMORTAL: ON":L"IMMORTAL: OFF",&cb,FALSE);
        if(GetFn(pause,ACT_DETECT,&cb)) AddButton(movie,&array,g_Patches[1].enabled?L"NEVER DETECTED: ON":L"NEVER DETECTED: OFF",&cb,FALSE);
        if(GetFn(pause,ACT_MANA,&cb)) AddButton(movie,&array,g_Patches[2].enabled?L"INFINITE MANA: ON":L"INFINITE MANA: OFF",&cb,FALSE);
        if(GetFn(pause,ACT_AMMO,&cb)) AddButton(movie,&array,g_Patches[3].enabled?L"INFINITE AMMO: ON":L"INFINITE AMMO: OFF",&cb,FALSE);
        if(GetFn(pause,ACT_POTION,&cb)) AddButton(movie,&array,g_Patches[4].enabled?L"INFINITE POTIONS: ON":L"INFINITE POTIONS: OFF",&cb,FALSE);
        if(GetFn(pause,ACT_BACK,&cb)) AddButton(movie,&array,L"BACK TO NORMAL MENU",&cb,FALSE);
        if(GetOriginalCallback(pause,"OnResumeClicked",&cb)) AddButton(movie,&array,L"RESUME GAME",&cb,FALSE);
    }

    if(ValueGetMember(pause,"_menu_mc",&menuMc)){
        SetBoolean(&delayed,TRUE);
        ValueSetMember(&menuMc,"_bDelayedOpeningAnimation",&delayed);
        ValueInvoke(&menuMc,0,"SetMenu",&array,1);
    }
}

static void RefreshCurrent(FunctionParams*params){
    /* Call our installed SetPauseMenu function exactly like the game does.
       No Close/Open, no Resume, no submenu transition. */
    if(params && params->thisPtr) ValueInvoke(params->thisPtr,0,"SetPauseMenu",0,0);
}
static void __thiscall HandlerCall(FunctionHandler*self,FunctionParams*params){
    u32 action=(u32)params->userData;(void)self;
    if(action==ACT_SETMENU){RenderMenu(params);return;}
    if(action==ACT_OPEN){g_CheatPage=TRUE;RefreshCurrent(params);return;}
    if(action==ACT_BACK){g_CheatPage=FALSE;RefreshCurrent(params);return;}
    if(action>=ACT_HEALTH && action<=ACT_POTION){TogglePatch(action-ACT_HEALTH);RefreshCurrent(params);return;}
}

static void __cdecl Luvvy_OnPauseMenu(void*pauseMenu){
    void*disMovie;void*movie;GFxValue pauseMenuMc,setFn;
    if(!pauseMenu)return;if(*((u8*)pauseMenu+0x1F8)==2)return;
    disMovie=*(void**)((u8*)pauseMenu+0x38);if(!disMovie)return;
    movie=*(void**)((u8*)disMovie+0x34);if(!movie)return;
    if(!MovieGetVariable(movie,&pauseMenuMc,"_root.pauseMenu_mc"))return;
    if(!EnsureFunctions(movie,&pauseMenuMc))return;
    if(GetFn(&pauseMenuMc,ACT_SETMENU,&setFn)) ValueSetMember(&pauseMenuMc,"SetPauseMenu",&setFn);
}
__declspec(naked) static void Luvvy_ShowPauseMenuHook(void){
    __asm {
        push ebp
        mov ebp, esp
        push 0FFFFFFFFh
        push ecx
        call Luvvy_OnPauseMenu
        pop ecx
        mov eax, 00BCCCA5h
        jmp eax
    }
}
static BOOL InstallHook(void){
    u8*target=(u8*)0x00BCCCA0;const u8 expected[5]={0x55,0x8B,0xEC,0x6A,0xFF};u32 i,oldProtect=0,ignored=0;
    for(i=0;i<5;++i)if(target[i]!=expected[i])return FALSE;
    if(!ProtectMemory(target,5,PAGE_EXECUTE_READWRITE,&oldProtect))return FALSE;
    target[0]=0xE9;*(long*)(target+1)=(long)((u8*)&Luvvy_ShowPauseMenuHook-(target+5));
    ProtectMemory(target,5,oldProtect,&ignored);return TRUE;
}
BOOL __stdcall DllMainCRTStartup(void*hinstDLL,DWORD fdwReason,void*lpvReserved){
    (void)hinstDLL;(void)lpvReserved;if(fdwReason==DLL_PROCESS_ATTACH){InitHandlers();InstallHook();}return TRUE;
}
