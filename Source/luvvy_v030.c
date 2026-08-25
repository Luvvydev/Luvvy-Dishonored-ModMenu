typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long DWORD;
typedef int BOOL;
typedef void *LPVOID;
typedef void *HANDLE;
typedef unsigned short wchar_t;

#define TRUE 1
#define FALSE 0
#define DLL_PROCESS_ATTACH 1
#define PAGE_EXECUTE_READWRITE 0x40
#define KEYEVENTF_KEYUP 0x0002
#define VK_ESCAPE 0x1B
#define VK_CONTROL 0x11
#define VK_SHIFT 0x10
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76

__declspec(dllimport) BOOL __stdcall VirtualProtect(LPVOID, unsigned long, DWORD, DWORD *);
__declspec(dllimport) HANDLE __stdcall CreateThread(LPVOID, unsigned long, DWORD (__stdcall *)(LPVOID), LPVOID, DWORD, DWORD *);
__declspec(dllimport) void __stdcall Sleep(DWORD);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) void __stdcall keybd_event(u8, u8, DWORD, unsigned long);

typedef struct GFxValue {
    void *valueInterface;
    u32 type;
    union {
        void *data;
        BOOL boolean;
        char *str;
        wchar_t *wstr;
        double number;
    } value;
} GFxValue;

typedef struct FunctionParams {
    GFxValue *retValue;
    void *movie;
    GFxValue *thisPtr;
    GFxValue *argsWithThisRef;
    GFxValue *args;
    int argsCount;
    void *userData;
} FunctionParams;

typedef struct FunctionHandler {
    void **vtable;
    u32 refCount;
} FunctionHandler;

typedef void (__thiscall *GFxSetStringWFn)(GFxValue *, wchar_t *);
typedef void (__thiscall *GFxSetBooleanFn)(GFxValue *, BOOL);
typedef BOOL (__thiscall *GFxMovieSetVariableFn)(void *, char *, GFxValue *, int);
typedef BOOL (__thiscall *GFxMovieGetVariableFn)(void *, GFxValue *, char *);
typedef void (__thiscall *GFxMovieCreateFunctionFn)(void *, GFxValue *, FunctionHandler *, void *);
typedef BOOL (__thiscall *VIGetMemberFn)(void *, void *, char *, GFxValue *, BOOL);
typedef BOOL (__thiscall *VISetMemberFn)(void *, void *, char *, GFxValue *, BOOL);

static void ZeroValue(GFxValue *v)
{
    u8 *p = (u8 *)v;
    u32 i;
    for (i = 0; i < sizeof(*v); ++i) p[i] = 0;
}

static BOOL IsDisplayObject(GFxValue *v)
{
    return (v->type & 0x08) ? TRUE : FALSE;
}

static BOOL ValueGetMember(GFxValue *v, char *name, GFxValue *out)
{
    if (!v || !v->valueInterface) return FALSE;
    ZeroValue(out);
    return ((VIGetMemberFn)0x00DA8AD0)(v->valueInterface, v->value.data, name, out, IsDisplayObject(v));
}

static BOOL ValueSetMember(GFxValue *v, char *name, GFxValue *src)
{
    if (!v || !v->valueInterface) return FALSE;
    return ((VISetMemberFn)0x00DA58F0)(v->valueInterface, v->value.data, name, src, IsDisplayObject(v));
}

static void SetString(GFxValue *v, wchar_t *s)
{
    ZeroValue(v);
    ((GFxSetStringWFn)0x0097B740)(v, s);
}

static void SetBool(GFxValue *v, BOOL b)
{
    ZeroValue(v);
    ((GFxSetBooleanFn)0x0097B6B0)(v, b);
}

static void * __thiscall HandlerDeletingDtor(FunctionHandler *self, u32 flags)
{
    (void)flags;
    return self;
}

static void __thiscall HandlerCall(FunctionHandler *self, FunctionParams *params);

static void *g_HandlerVTable[2] = {
    (void *)&HandlerDeletingDtor,
    (void *)&HandlerCall
};

#define HANDLER_COUNT 32
static FunctionHandler g_Handlers[HANDLER_COUNT];

static void InitHandlers(void)
{
    u32 i;
    for (i = 0; i < HANDLER_COUNT; ++i) {
        g_Handlers[i].vtable = g_HandlerVTable;
        g_Handlers[i].refCount = 0;
    }
}

static void CreateFunction(void *movie, GFxValue *out, u32 action)
{
    void **vt;
    GFxMovieCreateFunctionFn fn;
    FunctionHandler *handler;
    ZeroValue(out);
    if (!movie || action >= HANDLER_COUNT) return;
    vt = *(void ***)movie;
    if (!vt) return;
    fn = (GFxMovieCreateFunctionFn)vt[0x3C / 4];
    if (!fn) return;
    handler = &g_Handlers[action];
    handler->refCount = 0;
    fn(movie, out, handler, (void *)action);
}

/* ---------------- exact patches copied from the known working loader format ---------------- */
#define MAX_PATCH_HITS 16

typedef struct PatchDef {
    const u8 *findBytes;
    u32 findLen;
    const u8 *patchBytes;
    u32 patchLen;
    void *hits[MAX_PATCH_HITS];
    u32 hitCount;
    BOOL enabled;
} PatchDef;

static const u8 P_HEALTH_FIND[] = {0x29,0x93,0x44,0x03,0x00,0x00};
static const u8 P_HEALTH_ON[]   = {0x90,0x90,0x90,0x90,0x90,0x90};
static const u8 P_DETECT_FIND[] = {0x0F,0xB6,0x81,0xAC,0x00,0x00,0x00};
static const u8 P_DETECT_ON[]   = {0x31,0xC0,0x90,0x90,0x90,0x90,0x90};
static const u8 P_AMMO_FIND[]   = {0x89,0x06,0x5F,0xB8,0x01,0x00,0x00,0x00};
static const u8 P_AMMO_ON[]     = {0x90,0x90};
static const u8 P_POTION_FIND[] = {0xFF,0x8C,0x86,0xD4,0x00,0x00,0x00};
static const u8 P_POTION_ON[]   = {0x90,0x90,0x90,0x90,0x90,0x90,0x90};
static const u8 P_MANA_FIND[]   = {0x8B,0x8E,0x60,0x0A,0x00,0x00};
static const u8 P_MANA_ON[]     = {0x8B,0x8E,0x64,0x0A,0x00,0x00};

static PatchDef g_Patches[5] = {
    {P_HEALTH_FIND,6,P_HEALTH_ON,6,{0},0,FALSE},
    {P_DETECT_FIND,7,P_DETECT_ON,7,{0},0,FALSE},
    {P_AMMO_FIND,8,P_AMMO_ON,2,{0},0,FALSE},
    {P_MANA_FIND,6,P_MANA_ON,6,{0},0,FALSE},
    {P_POTION_FIND,7,P_POTION_ON,7,{0},0,FALSE}
};

static BOOL BytesEqual(const u8 *a, const u8 *b, u32 n)
{
    u32 i;
    for (i = 0; i < n; ++i) if (a[i] != b[i]) return FALSE;
    return TRUE;
}

static void DiscoverPatch(PatchDef *p)
{
    u8 *begin = (u8 *)0x00401000;
    u32 size = 0x00B918EF;
    u32 i;
    p->hitCount = 0;
    for (i = 0; i + p->findLen <= size; ++i) {
        if (BytesEqual(begin + i, p->findBytes, p->findLen)) {
            if (p->hitCount < MAX_PATCH_HITS) p->hits[p->hitCount++] = begin + i;
            i += p->findLen - 1;
        }
    }
}

static void InitPatches(void)
{
    u32 i;
    for (i = 0; i < 5; ++i) DiscoverPatch(&g_Patches[i]);
}

static void WriteBytes(void *address, const u8 *bytes, u32 len)
{
    DWORD oldProtect = 0, ignored = 0;
    u32 i;
    if (!address || !len) return;
    if (!VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldProtect)) return;
    for (i = 0; i < len; ++i) ((u8 *)address)[i] = bytes[i];
    VirtualProtect(address, len, oldProtect, &ignored);
}

static void SetPatch(PatchDef *p, BOOL enabled)
{
    u32 i;
    if (!p || p->hitCount == 0) return;
    if (enabled) {
        for (i = 0; i < p->hitCount; ++i) WriteBytes(p->hits[i], p->patchBytes, p->patchLen);
    } else {
        for (i = 0; i < p->hitCount; ++i) WriteBytes(p->hits[i], p->findBytes, p->patchLen);
    }
    p->enabled = enabled;
}

static void TogglePatch(u32 index)
{
    if (index >= 5) return;
    SetPatch(&g_Patches[index], !g_Patches[index].enabled);
}

/* ---------------- menu state / key execution ---------------- */
#define ACTION_OPEN_LUVVY 1
#define ACTION_HEALTH 2
#define ACTION_NODETECT 3
#define ACTION_AMMO 4
#define ACTION_MANA 5
#define ACTION_POTIONS 6
#define ACTION_PAGE2 7
#define ACTION_NORMAL 8
#define ACTION_CLOSE 9
#define ACTION_COINS 10
#define ACTION_RUNES 11
#define ACTION_MAXITEMS 12
#define ACTION_MAXPOWERS 13
#define ACTION_MAXUPGRADES 14
#define ACTION_KILLRATS 15
#define ACTION_PAGE1 16

#define SEQ_REOPEN 1
#define SEQ_CLOSE_ONLY 2
#define SEQ_COMMAND 3

static volatile u32 g_LuvvyMode = 0;
static volatile u32 g_Page = 0;

static void TapKey(u8 vk)
{
    keybd_event(vk, 0, 0, 0);
    Sleep(18);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
}

static void TapCheatChord(u8 vk)
{
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event(VK_SHIFT, 0, 0, 0);
    Sleep(12);
    TapKey(vk);
    keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

static DWORD __stdcall SequenceThread(LPVOID arg)
{
    u32 packed = (u32)arg;
    u8 seq = (u8)(packed & 0xFF);
    u8 vk = (u8)((packed >> 8) & 0xFF);

    Sleep(70);
    TapKey(VK_ESCAPE);

    if (seq == SEQ_REOPEN) {
        Sleep(220);
        TapKey(VK_ESCAPE);
    } else if (seq == SEQ_COMMAND) {
        Sleep(150);
        TapCheatChord(vk);
        Sleep(180);
        TapKey(VK_ESCAPE);
    }
    return 0;
}

static void StartSequence(u8 seq, u8 vk)
{
    HANDLE h;
    u32 packed = ((u32)vk << 8) | seq;
    h = CreateThread(0, 0, SequenceThread, (void *)packed, 0, 0);
    if (h) CloseHandle(h);
}

static void __thiscall HandlerCall(FunctionHandler *self, FunctionParams *params)
{
    u32 action;
    (void)self;
    if (!params) return;
    action = (u32)params->userData;

    if (action == ACTION_OPEN_LUVVY) {
        g_LuvvyMode = 1;
        g_Page = 0;
        StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_HEALTH) {
        TogglePatch(0); StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_NODETECT) {
        TogglePatch(1); StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_AMMO) {
        TogglePatch(2); StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_MANA) {
        TogglePatch(3); StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_POTIONS) {
        TogglePatch(4); StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_PAGE2) {
        g_Page = 1; StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_PAGE1) {
        g_Page = 0; StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_NORMAL) {
        g_LuvvyMode = 0; g_Page = 0; StartSequence(SEQ_REOPEN, 0);
    } else if (action == ACTION_CLOSE) {
        StartSequence(SEQ_CLOSE_ONLY, 0);
    } else if (action == ACTION_COINS) {
        StartSequence(SEQ_COMMAND, VK_F2);
    } else if (action == ACTION_RUNES) {
        StartSequence(SEQ_COMMAND, VK_F3);
    } else if (action == ACTION_MAXITEMS) {
        StartSequence(SEQ_COMMAND, VK_F4);
    } else if (action == ACTION_MAXPOWERS) {
        StartSequence(SEQ_COMMAND, VK_F5);
    } else if (action == ACTION_MAXUPGRADES) {
        StartSequence(SEQ_COMMAND, VK_F6);
    } else if (action == ACTION_KILLRATS) {
        StartSequence(SEQ_COMMAND, VK_F7);
    }
}

static char *g_TextPaths[8] = {
    "_root.texts.t_ResumeGame",
    "_root.texts.t_SaveGame",
    "_root.texts.t_LoadGamePauseMenu",
    "_root.texts.t_Options",
    "_root.texts.t_MissionStats",
    "_root.texts.t_Help",
    "_root.texts.t_BackToMainMenu",
    "_root.texts.t_BackToWindows"
};

static char *g_Callbacks[8] = {
    "OnResumeClicked",
    "OnSaveGameClicked",
    "OnLoadGameClicked",
    "OnOptionsClicked",
    "OnMissionStatsClicked",
    "OnTutorialsClicked",
    "OnQuitGameClicked",
    "OnBackToWindowsClicked"
};

static char *g_SavedCallbacks[8] = {
    "__LuvvyOrigCb0","__LuvvyOrigCb1","__LuvvyOrigCb2","__LuvvyOrigCb3",
    "__LuvvyOrigCb4","__LuvvyOrigCb5","__LuvvyOrigCb6","__LuvvyOrigCb7"
};

static char *g_SavedTexts[8] = {
    "__LuvvyOrigTx0","__LuvvyOrigTx1","__LuvvyOrigTx2","__LuvvyOrigTx3",
    "__LuvvyOrigTx4","__LuvvyOrigTx5","__LuvvyOrigTx6","__LuvvyOrigTx7"
};

static BOOL GetPauseMenuMC(void *movie, GFxValue *pauseMenuMc)
{
    void **vt;
    GFxMovieGetVariableFn getVariable;
    if (!movie || !pauseMenuMc) return FALSE;
    vt = *(void ***)movie;
    if (!vt) return FALSE;
    getVariable = (GFxMovieGetVariableFn)vt[0x44/4];
    if (!getVariable) return FALSE;
    ZeroValue(pauseMenuMc);
    return getVariable(movie, pauseMenuMc, "_root.pauseMenu_mc");
}

static BOOL EnsureOriginalsSaved(void *movie, GFxValue *pauseMenuMc)
{
    GFxValue marker, value;
    void **vt;
    GFxMovieGetVariableFn getVariable;
    u32 i;
    if (ValueGetMember(pauseMenuMc, "__LuvvySaved", &marker) && marker.value.boolean) return TRUE;

    vt = *(void ***)movie;
    if (!vt) return FALSE;
    getVariable = (GFxMovieGetVariableFn)vt[0x44/4];
    if (!getVariable) return FALSE;

    for (i = 0; i < 8; ++i) {
        if (ValueGetMember(pauseMenuMc, g_Callbacks[i], &value))
            ValueSetMember(pauseMenuMc, g_SavedCallbacks[i], &value);
        ZeroValue(&value);
        if (getVariable(movie, &value, g_TextPaths[i]))
            ValueSetMember(pauseMenuMc, g_SavedTexts[i], &value);
    }
    SetBool(&marker, TRUE);
    ValueSetMember(pauseMenuMc, "__LuvvySaved", &marker);
    return TRUE;
}

static void RestoreOriginals(void *movie, GFxValue *pauseMenuMc)
{
    GFxValue value;
    void **vt;
    GFxMovieSetVariableFn setVariable;
    u32 i;
    vt = *(void ***)movie;
    if (!vt) return;
    setVariable = (GFxMovieSetVariableFn)vt[0x40/4];
    if (!setVariable) return;

    for (i = 0; i < 8; ++i) {
        if (ValueGetMember(pauseMenuMc, g_SavedCallbacks[i], &value))
            ValueSetMember(pauseMenuMc, g_Callbacks[i], &value);
        if (ValueGetMember(pauseMenuMc, g_SavedTexts[i], &value))
            setVariable(movie, g_TextPaths[i], &value, 0);
    }
}

static void SetRow(void *movie, GFxValue *pauseMenuMc, u32 row, wchar_t *label, u32 action)
{
    void **vt;
    GFxMovieSetVariableFn setVariable;
    GFxValue text, callback;
    if (row >= 8) return;
    vt = *(void ***)movie;
    if (!vt) return;
    setVariable = (GFxMovieSetVariableFn)vt[0x40/4];
    if (!setVariable) return;

    SetString(&text, label);
    setVariable(movie, g_TextPaths[row], &text, 0);
    CreateFunction(movie, &callback, action);
    ValueSetMember(pauseMenuMc, g_Callbacks[row], &callback);
}

static void BuildPage1(void *movie, GFxValue *pauseMenuMc)
{
    SetRow(movie,pauseMenuMc,0,g_Patches[0].enabled ? L"IMMORTAL: ON" : L"IMMORTAL: OFF",ACTION_HEALTH);
    SetRow(movie,pauseMenuMc,1,g_Patches[1].enabled ? L"NEVER DETECTED: ON" : L"NEVER DETECTED: OFF",ACTION_NODETECT);
    SetRow(movie,pauseMenuMc,2,g_Patches[2].enabled ? L"INFINITE AMMO: ON" : L"INFINITE AMMO: OFF",ACTION_AMMO);
    SetRow(movie,pauseMenuMc,3,g_Patches[3].enabled ? L"INFINITE MANA: ON" : L"INFINITE MANA: OFF",ACTION_MANA);
    SetRow(movie,pauseMenuMc,4,g_Patches[4].enabled ? L"INFINITE POTIONS: ON" : L"INFINITE POTIONS: OFF",ACTION_POTIONS);
    SetRow(movie,pauseMenuMc,5,L"RESOURCES  >",ACTION_PAGE2);
    SetRow(movie,pauseMenuMc,6,L"BACK TO NORMAL MENU",ACTION_NORMAL);
    SetRow(movie,pauseMenuMc,7,L"CLOSE MENU",ACTION_CLOSE);
}

static void BuildPage2(void *movie, GFxValue *pauseMenuMc)
{
    SetRow(movie,pauseMenuMc,0,L"GIVE 1000 COINS",ACTION_COINS);
    SetRow(movie,pauseMenuMc,1,L"GIVE 5 RUNES",ACTION_RUNES);
    SetRow(movie,pauseMenuMc,2,L"MAX ITEMS",ACTION_MAXITEMS);
    SetRow(movie,pauseMenuMc,3,L"MAX POWERS",ACTION_MAXPOWERS);
    SetRow(movie,pauseMenuMc,4,L"MAX UPGRADES",ACTION_MAXUPGRADES);
    SetRow(movie,pauseMenuMc,5,L"KILL ALL RATS",ACTION_KILLRATS);
    SetRow(movie,pauseMenuMc,6,L"<  PLAYER CHEATS",ACTION_PAGE1);
    SetRow(movie,pauseMenuMc,7,L"BACK TO NORMAL MENU",ACTION_NORMAL);
}

static void __cdecl Luvvy_OnPauseMenu(void *pauseMenu)
{
    void *disMovie;
    void *movie;
    GFxValue pauseMenuMc;

    if (!pauseMenu) return;
    if (*((u8 *)pauseMenu + 0x1F8) == 2) return;

    disMovie = *(void **)((u8 *)pauseMenu + 0x38);
    if (!disMovie) return;
    movie = *(void **)((u8 *)disMovie + 0x34);
    if (!movie) return;
    if (!GetPauseMenuMC(movie, &pauseMenuMc)) return;
    if (!EnsureOriginalsSaved(movie, &pauseMenuMc)) return;

    if (!g_LuvvyMode) {
        RestoreOriginals(movie, &pauseMenuMc);
        SetRow(movie, &pauseMenuMc, 5, L"LUVVY CHEATS", ACTION_OPEN_LUVVY);
    } else if (g_Page == 0) {
        BuildPage1(movie, &pauseMenuMc);
    } else {
        BuildPage2(movie, &pauseMenuMc);
    }
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

static BOOL InstallHook(void)
{
    u8 *target = (u8 *)0x00BCCCA0;
    const u8 expected[5] = {0x55,0x8B,0xEC,0x6A,0xFF};
    u32 i;
    DWORD oldProtect = 0, ignored = 0;

    for (i = 0; i < 5; ++i) if (target[i] != expected[i]) return FALSE;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return FALSE;
    target[0] = 0xE9;
    *(long *)(target + 1) = (long)((u8 *)&Luvvy_ShowPauseMenuHook - (target + 5));
    VirtualProtect(target, 5, oldProtect, &ignored);
    return TRUE;
}

BOOL __stdcall DllMainCRTStartup(void *hinstDLL, DWORD fdwReason, void *lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        InitHandlers();
        InitPatches();
        InstallHook();
    }
    return TRUE;
}
