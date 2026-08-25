typedef unsigned char u8;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned long uptr;
typedef void *LPVOID;
typedef unsigned short wchar_t;

#define TRUE 1
#define DLL_PROCESS_ATTACH 1
#define PAGE_EXECUTE_READWRITE 0x40

__declspec(dllimport) BOOL __stdcall VirtualProtect(LPVOID, unsigned long, DWORD, DWORD *);

typedef struct GFxValue {
    void *valueInterface;
    unsigned int type;
    union {
        void *data;
        BOOL boolean;
        char *str;
        wchar_t *wstr;
        double number;
    } value;
} GFxValue;

typedef void (__thiscall *GFxSetStringWFn)(GFxValue *, wchar_t *);
typedef BOOL (__thiscall *GFxMovieSetVariableFn)(void *, char *, GFxValue *, int);

static void __cdecl Luvvy_OnPauseMenu(void *pauseMenu)
{
    if (!pauseMenu) return;

    /* DisGFxMoviePlayerPauseMenu::mode */
    if (*((u8 *)pauseMenu + 0x1F8) == 2) return;

    /* DisGFxMoviePlayer::disMovie @ +0x38 */
    void *disMovie = *(void **)((u8 *)pauseMenu + 0x38);
    if (!disMovie) return;

    /* DisGFxMovie::movie @ +0x34 */
    void *movie = *(void **)((u8 *)disMovie + 0x34);
    if (!movie) return;

    GFxValue text;
    u8 *p = (u8 *)&text;
    unsigned int i;
    for (i = 0; i < sizeof(text); ++i) p[i] = 0;

    ((GFxSetStringWFn)0x0097B740)(&text, L"LUVVY TEST");

    void **vtable = *(void ***)movie;
    if (!vtable) return;

    GFxMovieSetVariableFn SetVariable = (GFxMovieSetVariableFn)vtable[0x40 / 4];
    if (!SetVariable) return;

    SetVariable(movie, "_root.texts.t_Help", &text, 0);
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
    unsigned int i;
    for (i = 0; i < 5; ++i) {
        if (target[i] != expected[i]) return 0;
    }

    DWORD oldProtect = 0;
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
    if (fdwReason == DLL_PROCESS_ATTACH) {
        InstallHook();
    }
    return TRUE;
}
