typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long DWORD;
typedef int BOOL;
typedef void *LPVOID;
typedef unsigned short wchar_t;

#define TRUE 1
#define FALSE 0
#define DLL_PROCESS_ATTACH 1
#define PAGE_EXECUTE_READWRITE 0x40


typedef long NTSTATUS;
typedef NTSTATUS (__stdcall *NtProtectVirtualMemoryFn)(void*, void**, u32*, u32, u32*);

static int StrEq(char *a, char *b){
    while(*a && *b){ if(*a!=*b)return 0; ++a; ++b; }
    return (*a==0 && *b==0);
}

static void *FindExportInModule(void *base, char *wanted){
    u8 *b=(u8*)base;
    u32 peOff, exportRva, namesRva, ordsRva, funcsRva, count, i;
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
    names=(u32*)(b+namesRva);
    funcs=(u32*)(b+funcsRva);
    ords=(unsigned short*)(b+ordsRva);
    for(i=0;i<count;++i){
        char *name=(char*)(b+names[i]);
        if(StrEq(name,wanted)){
            u32 rva=funcs[ords[i]];
            return (void*)(b+rva);
        }
    }
    return 0;
}

static void *ResolveLoadedExport(char *wanted){
    u8 *peb; u8 *ldr; u8 *head; u8 *link;
    __asm {
        mov eax, fs:[30h]
        mov peb, eax
    }
    if(!peb)return 0;
    ldr=*(u8**)(peb+0x0C);
    if(!ldr)return 0;
    head=ldr+0x14;
    link=*(u8**)head;
    while(link && link!=head){
        u8 *entry=link-0x08;
        void *base=*(void**)(entry+0x18);
        void *fn=FindExportInModule(base,wanted);
        if(fn)return fn;
        link=*(u8**)link;
    }
    return 0;
}

static NtProtectVirtualMemoryFn g_NtProtect=0;
static BOOL ProtectMemory(void *address,u32 len,u32 newProtect,u32 *oldProtect){
    void *base=address; u32 size=len; u32 old=0;
    if(!g_NtProtect)g_NtProtect=(NtProtectVirtualMemoryFn)ResolveLoadedExport("NtProtectVirtualMemory");
    if(!g_NtProtect)return FALSE;
    if(g_NtProtect((void*)-1,&base,&size,newProtect,&old)<0)return FALSE;
    if(oldProtect)*oldProtect=old;
    return TRUE;
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
typedef BOOL (__thiscall *GFxMovieSetVariableFn)(void *, char *, GFxValue *, int);
typedef BOOL (__thiscall *GFxMovieGetVariableFn)(void *, GFxValue *, char *);
typedef void (__thiscall *GFxMovieCreateFunctionFn)(void *, GFxValue *, FunctionHandler *, void *);
typedef BOOL (__thiscall *VIGetMemberFn)(void *, void *, char *, GFxValue *, BOOL);
typedef BOOL (__thiscall *VISetMemberFn)(void *, void *, char *, GFxValue *, BOOL);
typedef BOOL (__thiscall *VIInvokeFn)(void *, void *, GFxValue *, char *, GFxValue *, int, BOOL);

static void ZeroValue(GFxValue *v){u8 *p=(u8*)v;u32 i;for(i=0;i<sizeof(*v);++i)p[i]=0;}
static BOOL IsDisplayObject(GFxValue *v){return (v->type & 0x08)?TRUE:FALSE;}
static BOOL ValueGetMember(GFxValue *v,char *name,GFxValue *dest){if(!v||!v->valueInterface)return FALSE;ZeroValue(dest);return ((VIGetMemberFn)0x00DA8AD0)(v->valueInterface,v->value.data,name,dest,IsDisplayObject(v));}
static BOOL ValueSetMember(GFxValue *v,char *name,GFxValue *src){if(!v||!v->valueInterface)return FALSE;return ((VISetMemberFn)0x00DA58F0)(v->valueInterface,v->value.data,name,src,IsDisplayObject(v));}
static BOOL ValueInvoke(GFxValue *v,GFxValue *result,char *name,GFxValue *args,int count){if(!v||!v->valueInterface)return FALSE;return ((VIInvokeFn)0x00DA8230)(v->valueInterface,v->value.data,result,name,args,count,IsDisplayObject(v));}
static void SetString(GFxValue *v,wchar_t *s){ZeroValue(v);((GFxSetStringWFn)0x0097B740)(v,s);}

static void * __thiscall HandlerDeletingDtor(FunctionHandler *self,u32 flags){(void)flags;return self;}
static void __thiscall HandlerCall(FunctionHandler *self,FunctionParams *params);
static void *g_HandlerVTable[2]={(void*)&HandlerDeletingDtor,(void*)&HandlerCall};
#define HANDLER_COUNT 16
static FunctionHandler g_Handlers[HANDLER_COUNT];
static void InitHandlers(void){u32 i;for(i=0;i<HANDLER_COUNT;++i){g_Handlers[i].vtable=g_HandlerVTable;g_Handlers[i].refCount=0;}}
static void CreateFunction(void *movie,GFxValue *out,u32 action){void **vt;GFxMovieCreateFunctionFn fn;FunctionHandler *handler;ZeroValue(out);if(!movie||action>=HANDLER_COUNT)return;vt=*(void***)movie;if(!vt)return;fn=(GFxMovieCreateFunctionFn)vt[0x3C/4];if(!fn)return;handler=&g_Handlers[action];fn(movie,out,handler,(void*)action);}

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
static void DiscoverPatch(PatchDef*p){u8*begin=(u8*)0x00401000;u32 size=0x00B918EF,i;if(p->discovered)return;p->hitCount=0;for(i=0;i+p->findLen<=size;++i){if(BytesEqual(begin+i,p->findBytes,p->findLen)){if(p->hitCount<MAX_PATCH_HITS)p->hits[p->hitCount++]=begin+i;i+=p->findLen-1;}}p->discovered=TRUE;}
static void WriteBytes(void*address,const u8*bytes,u32 len){u32 oldProtect=0,ignored=0;u32 i;if(!address||!len)return;if(!ProtectMemory(address,len,PAGE_EXECUTE_READWRITE,&oldProtect))return;for(i=0;i<len;++i)((u8*)address)[i]=bytes[i];ProtectMemory(address,len,oldProtect,&ignored);}
static void TogglePatch(u32 index){PatchDef*p;u32 i;if(index>=5)return;p=&g_Patches[index];DiscoverPatch(p);if(p->hitCount==0)return;if(!p->enabled){for(i=0;i<p->hitCount;++i)WriteBytes(p->hits[i],p->patchBytes,p->patchLen);p->enabled=TRUE;}else{for(i=0;i<p->hitCount;++i)WriteBytes(p->hits[i],p->findBytes,p->patchLen);p->enabled=FALSE;}}

#define ACTION_HEALTH 1
#define ACTION_NODETECT 2
#define ACTION_MANA 3
#define ACTION_AMMO 4
#define ACTION_POTIONS 5
#define ACTION_OPEN_CHEATS 6
#define ACTION_BACK_NORMAL 7
#define ACTION_RESUME 8

static char *g_TextPaths[8]={"_root.texts.t_ResumeGame","_root.texts.t_SaveGame","_root.texts.t_LoadGamePauseMenu","_root.texts.t_Options","_root.texts.t_MissionStats","_root.texts.t_Help","_root.texts.t_BackToMainMenu","_root.texts.t_BackToWindows"};
static char *g_Callbacks[8]={"OnResumeClicked","OnSaveGameClicked","OnLoadGameClicked","OnOptionsClicked","OnMissionStatsClicked","OnTutorialsClicked","OnQuitGameClicked","OnBackToWindowsClicked"};
static char *g_HiddenText[8]={"_LuvvyText0","_LuvvyText1","_LuvvyText2","_LuvvyText3","_LuvvyText4","_LuvvyText5","_LuvvyText6","_LuvvyText7"};
static char *g_HiddenCb[8]={"_LuvvyCb0","_LuvvyCb1","_LuvvyCb2","_LuvvyCb3","_LuvvyCb4","_LuvvyCb5","_LuvvyCb6","_LuvvyCb7"};
static char *g_HiddenFn[9]={0,"_LuvvyFn1","_LuvvyFn2","_LuvvyFn3","_LuvvyFn4","_LuvvyFn5","_LuvvyFn6","_LuvvyFn7","_LuvvyFn8"};
static void *g_CustomMovie=0;
static BOOL g_CustomReady=FALSE;
static void *g_SavedMovie=0;
static BOOL g_HaveSaved=FALSE;
static BOOL g_CheatPage=FALSE;

static BOOL GetPauseMenuMC(void*movie,GFxValue*pauseMenuMc){void**vt;GFxMovieGetVariableFn getVariable;if(!movie||!pauseMenuMc)return FALSE;vt=*(void***)movie;if(!vt)return FALSE;getVariable=(GFxMovieGetVariableFn)vt[0x44/4];if(!getVariable)return FALSE;ZeroValue(pauseMenuMc);return getVariable(movie,pauseMenuMc,"_root.pauseMenu_mc");}
static BOOL GetMovieVariable(void*movie,GFxValue*out,char*path){void**vt;GFxMovieGetVariableFn fn;if(!movie||!out)return FALSE;vt=*(void***)movie;if(!vt)return FALSE;fn=(GFxMovieGetVariableFn)vt[0x44/4];if(!fn)return FALSE;ZeroValue(out);return fn(movie,out,path);}
static BOOL SetMovieVariable(void*movie,char*path,GFxValue*v){void**vt;GFxMovieSetVariableFn fn;if(!movie||!v)return FALSE;vt=*(void***)movie;if(!vt)return FALSE;fn=(GFxMovieSetVariableFn)vt[0x40/4];if(!fn)return FALSE;return fn(movie,path,v,0);}

/* Keep managed ActionScript strings/functions alive on pauseMenu_mc itself.
   Do not keep raw GFxValue function objects across menu transitions. */
static void SaveOriginals(void*movie,GFxValue*pauseMenuMc){u32 i;GFxValue v;if(!movie||!pauseMenuMc)return;if(g_HaveSaved&&g_SavedMovie==movie)return;if(g_SavedMovie!=movie){g_CustomMovie=0;g_CustomReady=FALSE;}g_SavedMovie=movie;g_HaveSaved=TRUE;for(i=0;i<8;++i){if(GetMovieVariable(movie,&v,g_TextPaths[i]))ValueSetMember(pauseMenuMc,g_HiddenText[i],&v);if(ValueGetMember(pauseMenuMc,g_Callbacks[i],&v))ValueSetMember(pauseMenuMc,g_HiddenCb[i],&v);}}
static void RestoreOriginals(void*movie,GFxValue*pauseMenuMc){u32 i;GFxValue v;if(!g_HaveSaved||g_SavedMovie!=movie)return;for(i=0;i<8;++i){if(ValueGetMember(pauseMenuMc,g_HiddenText[i],&v))SetMovieVariable(movie,g_TextPaths[i],&v);if(ValueGetMember(pauseMenuMc,g_HiddenCb[i],&v))ValueSetMember(pauseMenuMc,g_Callbacks[i],&v);}}

/* Resume through the live ActionScript member. Row 0 is never replaced by
   Luvvy, so this never depends on a cached managed callback. */
static void ResumeLive(FunctionParams*params){GFxValue pauseMenuMc;if(!params||!params->movie)return;if(!GetPauseMenuMC(params->movie,&pauseMenuMc))return;ValueInvoke(&pauseMenuMc,0,"OnResumeClicked",0,0);}
static BOOL EnsureCustomFunctions(void*movie,GFxValue*pauseMenuMc){
 u32 action;GFxValue fn;
 if(!movie||!pauseMenuMc)return FALSE;
 if(g_CustomReady&&g_CustomMovie==movie)return TRUE;
 g_CustomMovie=movie;g_CustomReady=FALSE;
 for(action=1;action<=8;++action){
   CreateFunction(movie,&fn,action);
   if(!fn.valueInterface)return FALSE;
   if(!ValueSetMember(pauseMenuMc,g_HiddenFn[action],&fn))return FALSE;
 }
 g_CustomReady=TRUE;
 return TRUE;
}
static void SetRow(void*movie,GFxValue*pauseMenuMc,u32 row,wchar_t*label,u32 action){
 GFxValue text,callback;
 if(row>=8||action<1||action>8)return;
 SetString(&text,label);
 SetMovieVariable(movie,g_TextPaths[row],&text);
 if(!EnsureCustomFunctions(movie,pauseMenuMc))return;
 if(!ValueGetMember(pauseMenuMc,g_HiddenFn[action],&callback))return;
 ValueSetMember(pauseMenuMc,g_Callbacks[row],&callback);
}

static void __thiscall HandlerCall(FunctionHandler*self,FunctionParams*params){u32 action;(void)self;if(!params)return;action=(u32)params->userData;
 if(action>=ACTION_HEALTH&&action<=ACTION_POTIONS){TogglePatch(action-1);ResumeLive(params);return;}
 if(action==ACTION_OPEN_CHEATS){g_CheatPage=TRUE;ResumeLive(params);return;}
 if(action==ACTION_BACK_NORMAL){g_CheatPage=FALSE;ResumeLive(params);return;}
 if(action==ACTION_RESUME){ResumeLive(params);return;}
}

static void __cdecl Luvvy_OnPauseMenu(void*pauseMenu){void*disMovie;void*movie;GFxValue pauseMenuMc;if(!pauseMenu)return;if(*((u8*)pauseMenu+0x1F8)==2)return;disMovie=*(void**)((u8*)pauseMenu+0x38);if(!disMovie)return;movie=*(void**)((u8*)disMovie+0x34);if(!movie)return;if(!GetPauseMenuMC(movie,&pauseMenuMc))return;
 SaveOriginals(movie,&pauseMenuMc);
 RestoreOriginals(movie,&pauseMenuMc);
 if(!g_CheatPage){
   SetRow(movie,&pauseMenuMc,5,L"LUVVY CHEATS",ACTION_OPEN_CHEATS);
   return;
 }
 /* Keep row 0 (Resume) completely original so all Luvvy callbacks can exit
    through the game's current live OnResumeClicked implementation. */
 SetRow(movie,&pauseMenuMc,1,g_Patches[0].enabled?L"IMMORTAL: ON":L"IMMORTAL: OFF",ACTION_HEALTH);
 SetRow(movie,&pauseMenuMc,2,g_Patches[1].enabled?L"NEVER DETECTED: ON":L"NEVER DETECTED: OFF",ACTION_NODETECT);
 SetRow(movie,&pauseMenuMc,3,g_Patches[2].enabled?L"INFINITE MANA: ON":L"INFINITE MANA: OFF",ACTION_MANA);
 SetRow(movie,&pauseMenuMc,4,g_Patches[3].enabled?L"INFINITE AMMO: ON":L"INFINITE AMMO: OFF",ACTION_AMMO);
 SetRow(movie,&pauseMenuMc,5,g_Patches[4].enabled?L"INFINITE POTIONS: ON":L"INFINITE POTIONS: OFF",ACTION_POTIONS);
 SetRow(movie,&pauseMenuMc,6,L"BACK TO NORMAL MENU",ACTION_BACK_NORMAL);
 SetRow(movie,&pauseMenuMc,7,L"RESUME GAME",ACTION_RESUME);
}

__declspec(naked) static void Luvvy_ShowPauseMenuHook(void)
{
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
static BOOL InstallHook(void){u8*target=(u8*)0x00BCCCA0;const u8 expected[5]={0x55,0x8B,0xEC,0x6A,0xFF};u32 i;u32 oldProtect=0,ignored=0;for(i=0;i<5;++i)if(target[i]!=expected[i])return FALSE;if(!ProtectMemory(target,5,PAGE_EXECUTE_READWRITE,&oldProtect))return FALSE;target[0]=0xE9;*(long*)(target+1)=(long)((u8*)&Luvvy_ShowPauseMenuHook-(target+5));ProtectMemory(target,5,oldProtect,&ignored);return TRUE;}
BOOL __stdcall DllMainCRTStartup(void*hinstDLL,DWORD fdwReason,void*lpvReserved){(void)hinstDLL;(void)lpvReserved;if(fdwReason==DLL_PROCESS_ATTACH){InitHandlers();InstallHook();}return TRUE;}
