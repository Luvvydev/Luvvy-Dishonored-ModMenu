typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long DWORD;
typedef int BOOL;
typedef void *LPVOID;
typedef unsigned short wchar_t;

#define TRUE 1
#define DLL_PROCESS_ATTACH 1
#define PAGE_EXECUTE_READWRITE 0x40

__declspec(dllimport) BOOL __stdcall VirtualProtect(LPVOID, unsigned long, DWORD, DWORD *);

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
typedef void (__thiscall *GFxMovieCreateObjectFn)(void *, GFxValue *, char *, GFxValue *, int);
typedef void (__thiscall *GFxMovieCreateArrayFn)(void *, GFxValue *);
typedef void (__thiscall *GFxMovieCreateFunctionFn)(void *, GFxValue *, FunctionHandler *, void *);
typedef BOOL (__thiscall *VIGetMemberFn)(void *, void *, char *, GFxValue *, BOOL);
typedef BOOL (__thiscall *VISetMemberFn)(void *, void *, char *, GFxValue *, BOOL);
typedef BOOL (__thiscall *VIInvokeFn)(void *, void *, GFxValue *, char *, GFxValue *, int, BOOL);
typedef BOOL (__thiscall *VIPushBackFn)(void *, void *, GFxValue *);

static void ZeroValue(GFxValue *v)
{
    u8 *p = (u8 *)v;
    u32 i;
    for (i = 0; i < sizeof(*v); ++i) p[i] = 0;
}

static BOOL IsDisplayObject(GFxValue *v)
{
    return (v->type & 0x08) ? TRUE : 0;
}

static BOOL ValueGetMember(GFxValue *v, char *name, GFxValue *out)
{
    if (!v || !v->valueInterface) return 0;
    return ((VIGetMemberFn)0x00DA8AD0)(v->valueInterface, v->value.data, name, out, IsDisplayObject(v));
}

static BOOL ValueSetMember(GFxValue *v, char *name, GFxValue *src)
{
    if (!v || !v->valueInterface) return 0;
    return ((VISetMemberFn)0x00DA58F0)(v->valueInterface, v->value.data, name, src, IsDisplayObject(v));
}

static BOOL ValueInvoke(GFxValue *v, char *name, GFxValue *args, int count)
{
    if (!v || !v->valueInterface) return 0;
    return ((VIInvokeFn)0x00DA8230)(v->valueInterface, v->value.data, 0, name, args, count, IsDisplayObject(v));
}

static BOOL ValuePushBack(GFxValue *v, GFxValue *item)
{
    if (!v || !v->valueInterface) return 0;
    return ((VIPushBackFn)0x00DA5A20)(v->valueInterface, v->value.data, item);
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

static FunctionHandler g_OpenHandler = { g_HandlerVTable, 0 };
static FunctionHandler g_NoopHandler = { g_HandlerVTable, 0 };
static FunctionHandler g_BackHandler = { g_HandlerVTable, 0 };

#define ACTION_OPEN 1
#define ACTION_NOOP 2
#define ACTION_BACK 3

static void CreateFunction(void *movie, GFxValue *out, FunctionHandler *handler, u32 action)
{
    void **vt;
    GFxMovieCreateFunctionFn fn;
    ZeroValue(out);
    if (!movie) return;
    vt = *(void ***)movie;
    if (!vt) return;
    fn = (GFxMovieCreateFunctionFn)vt[0x3C / 4];
    if (!fn) return;
    handler->refCount = 0;
    fn(movie, out, handler, (void *)action);
}

static void AddButton(void *movie, GFxValue *array, wchar_t *label, FunctionHandler *handler, u32 action)
{
    void **vt;
    GFxMovieCreateObjectFn createObject;
    GFxValue button, text, callback, lockState;

    if (!movie || !array) return;
    vt = *(void ***)movie;
    if (!vt) return;
    createObject = (GFxMovieCreateObjectFn)vt[0x34 / 4];
    if (!createObject) return;

    ZeroValue(&button);
    createObject(movie, &button, 0, 0, 0);
    SetString(&text, label);
    CreateFunction(movie, &callback, handler, action);
    SetBool(&lockState, 0);

    ValueSetMember(&button, "txt", &text);
    ValueSetMember(&button, "callback", &callback);
    ValueSetMember(&button, "lockState", &lockState);
    ValuePushBack(array, &button);
}

static BOOL GetPauseAndMenu(void *movie, GFxValue *pauseMenuMc, GFxValue *menuMc)
{
    void **vt;
    GFxMovieGetVariableFn getVariable;

    if (!movie || !pauseMenuMc || !menuMc) return 0;
    vt = *(void ***)movie;
    if (!vt) return 0;
    getVariable = (GFxMovieGetVariableFn)vt[0x44 / 4];
    if (!getVariable) return 0;

    ZeroValue(pauseMenuMc);
    ZeroValue(menuMc);
    if (!getVariable(movie, pauseMenuMc, "_root.pauseMenu_mc")) return 0;
    if (!ValueGetMember(pauseMenuMc, "_menu_mc", menuMc)) return 0;
    return 1;
}

static void RefreshMenu(GFxValue *menu, GFxValue *buttons)
{
    GFxValue delayed;
    if (!menu) return;

    /* SetMenu alone updates the data but does not redraw an already-open
       pause menu in this build. Close -> SetMenu -> Open forces the native
       Scaleform menu to rebuild its visible rows. */
    ValueInvoke(menu, "Close", 0, 0);

    SetBool(&delayed, 1);
    ValueSetMember(menu, "_bDelayedOpeningAnimation", &delayed);

    if (buttons) ValueInvoke(menu, "SetMenu", buttons, 1);
    ValueInvoke(menu, "Open", 0, 0);
}

static void OpenLuvvyMenu(FunctionParams *params)
{
    void *movie;
    void **vt;
    GFxMovieCreateArrayFn createArray;
    GFxValue buttons, pauseMenuMc, menu;

    if (!params || !params->movie) return;
    movie = params->movie;
    vt = *(void ***)movie;
    if (!vt) return;
    createArray = (GFxMovieCreateArrayFn)vt[0x38 / 4];
    if (!createArray) return;

    ZeroValue(&buttons);
    createArray(movie, &buttons);

    AddButton(movie, &buttons, L"LUVVY MENU WORKS", &g_NoopHandler, ACTION_NOOP);
    AddButton(movie, &buttons, L"CHEAT ACTIONS NEXT", &g_NoopHandler, ACTION_NOOP);
    AddButton(movie, &buttons, L"BACK", &g_BackHandler, ACTION_BACK);

    if (!GetPauseAndMenu(movie, &pauseMenuMc, &menu)) return;
    RefreshMenu(&menu, &buttons);
}

static void BackToPause(FunctionParams *params)
{
    GFxValue pauseMenuMc, menu;
    if (!params || !params->movie) return;
    if (!GetPauseAndMenu(params->movie, &pauseMenuMc, &menu)) return;

    /* Ask the game's own ActionScript to rebuild the normal pause entries,
       then reopen the visible menu so BACK is guaranteed to escape. */
    ValueInvoke(&menu, "Close", 0, 0);
    ValueInvoke(&pauseMenuMc, "SetPauseMenu", 0, 0);
    ValueInvoke(&menu, "Open", 0, 0);
}

static void __thiscall HandlerCall(FunctionHandler *self, FunctionParams *params)
{
    u32 action;
    (void)self;
    if (!params) return;
    action = (u32)params->userData;
    if (action == ACTION_OPEN) OpenLuvvyMenu(params);
    else if (action == ACTION_BACK) BackToPause(params);
}

static void __cdecl Luvvy_OnPauseMenu(void *pauseMenu)
{
    void *disMovie;
    void *movie;
    void **vtable;
    GFxMovieSetVariableFn setVariable;
    GFxMovieGetVariableFn getVariable;
    GFxValue text, pauseMenuMc, callback;

    if (!pauseMenu) return;
    if (*((u8 *)pauseMenu + 0x1F8) == 2) return;

    disMovie = *(void **)((u8 *)pauseMenu + 0x38);
    if (!disMovie) return;
    movie = *(void **)((u8 *)disMovie + 0x34);
    if (!movie) return;

    vtable = *(void ***)movie;
    if (!vtable) return;
    setVariable = (GFxMovieSetVariableFn)vtable[0x40 / 4];
    getVariable = (GFxMovieGetVariableFn)vtable[0x44 / 4];
    if (!setVariable || !getVariable) return;

    SetString(&text, L"LUVVY CHEATS");
    setVariable(movie, "_root.texts.t_Help", &text, 0);

    ZeroValue(&pauseMenuMc);
    if (!getVariable(movie, &pauseMenuMc, "_root.pauseMenu_mc")) return;

    CreateFunction(movie, &callback, &g_OpenHandler, ACTION_OPEN);
    ValueSetMember(&pauseMenuMc, "OnTutorialsClicked", &callback);
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
    const u8 expected[5] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF};
    u32 i;
    DWORD oldProtect = 0;

    for (i = 0; i < 5; ++i) {
        if (target[i] != expected[i]) return 0;
    }

    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return 0;
    target[0] = 0xE9;
    *(long *)(target + 1) = (long)((u8 *)&Luvvy_ShowPauseMenuHook - (target + 5));
    {
        DWORD ignored = 0;
        VirtualProtect(target, 5, oldProtect, &ignored);
    }
    return 1;
}

BOOL __stdcall DllMainCRTStartup(void *hinstDLL, DWORD fdwReason, void *lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) InstallHook();
    return TRUE;
}
