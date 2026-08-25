
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long DWORD;
typedef long LONG;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned short WORD;
typedef unsigned long ULONG_PTR;
typedef long LPARAM;
typedef unsigned int WPARAM;
typedef long LRESULT;
typedef void *HANDLE;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HANDLE HDC;
typedef HANDLE HBRUSH;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef const unsigned short *LPCWSTR;
typedef unsigned short WCHAR;
typedef long NTSTATUS;

#define TRUE 1
#define FALSE 0
#define DLL_PROCESS_ATTACH 1
#define PAGE_EXECUTE_READWRITE 0x40

#define WS_POPUP        0x80000000u
#define WS_BORDER       0x00800000u
#define WS_EX_TOPMOST   0x00000008u
#define WS_EX_TOOLWINDOW 0x00000080u
#define WS_EX_NOACTIVATE 0x08000000u

#define SW_HIDE 0
#define SW_SHOWNOACTIVATE 4
#define HWND_TOPMOST ((HWND)-1)

#define SWP_NOACTIVATE 0x0010
#define SWP_SHOWWINDOW 0x0040

#define WM_PAINT   0x000F
#define WM_ERASEBKGND 0x0014

#define TRANSPARENT 1

#define VK_ESCAPE 0x1B
#define VK_RETURN 0x0D
#define VK_UP     0x26
#define VK_DOWN   0x28
#define VK_F10    0x79
#define VK_1      0x31
#define VK_2      0x32
#define VK_3      0x33
#define VK_4      0x34
#define VK_5      0x35

typedef struct RECT { LONG left,top,right,bottom; } RECT;
typedef struct POINT { LONG x,y; } POINT;
typedef struct PAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    u8 rgbReserved[32];
} PAINTSTRUCT;

typedef LRESULT (__stdcall *WNDPROC)(HWND,UINT,WPARAM,LPARAM);
typedef struct WNDCLASSW {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
} WNDCLASSW;

typedef NTSTATUS (__stdcall *NtProtectVirtualMemoryFn)(void*, void**, u32*, u32, u32*);
typedef unsigned short (__stdcall *RegisterClassWFn)(const WNDCLASSW*);
typedef HWND (__stdcall *CreateWindowExWFn)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HANDLE,HINSTANCE,void*);
typedef BOOL (__stdcall *DestroyWindowFn)(HWND);
typedef BOOL (__stdcall *ShowWindowFn)(HWND,int);
typedef BOOL (__stdcall *SetWindowPosFn)(HWND,HWND,int,int,int,int,UINT);
typedef HWND (__stdcall *GetForegroundWindowFn)(void);
typedef BOOL (__stdcall *GetWindowRectFn)(HWND,RECT*);
typedef short (__stdcall *GetAsyncKeyStateFn)(int);
typedef BOOL (__stdcall *InvalidateRectFn)(HWND,const RECT*,BOOL);
typedef BOOL (__stdcall *UpdateWindowFn)(HWND);
typedef HDC (__stdcall *BeginPaintFn)(HWND,PAINTSTRUCT*);
typedef BOOL (__stdcall *EndPaintFn)(HWND,const PAINTSTRUCT*);
typedef LRESULT (__stdcall *DefWindowProcWFn)(HWND,UINT,WPARAM,LPARAM);
typedef int (__stdcall *FillRectFn)(HDC,const RECT*,HBRUSH);
typedef HBRUSH (__stdcall *CreateSolidBrushFn)(u32);
typedef BOOL (__stdcall *DeleteObjectFn)(HANDLE);
typedef u32 (__stdcall *SetTextColorFn)(HDC,u32);
typedef int (__stdcall *SetBkModeFn)(HDC,int);
typedef BOOL (__stdcall *TextOutWFn)(HDC,int,int,LPCWSTR,int);

static HINSTANCE g_Instance=0;

static int StrEq(char *a,char *b){
    while(*a&&*b){if(*a!=*b)return 0;++a;++b;}
    return (*a==0&&*b==0);
}
static void *FindExportInModule(void *base,char *wanted){
    u8 *b=(u8*)base;u32 peOff,exportRva,namesRva,ordsRva,funcsRva,count,i;
    u32 *names,*funcs;u16 *ords;
    if(!b||b[0]!='M'||b[1]!='Z')return 0;
    peOff=*(u32*)(b+0x3C);
    if(*(u32*)(b+peOff)!=0x00004550)return 0;
    exportRva=*(u32*)(b+peOff+0x78);
    if(!exportRva)return 0;
    count=*(u32*)(b+exportRva+0x18);
    funcsRva=*(u32*)(b+exportRva+0x1C);
    namesRva=*(u32*)(b+exportRva+0x20);
    ordsRva=*(u32*)(b+exportRva+0x24);
    names=(u32*)(b+namesRva);funcs=(u32*)(b+funcsRva);ords=(u16*)(b+ordsRva);
    for(i=0;i<count;++i){
        char *name=(char*)(b+names[i]);
        if(StrEq(name,wanted))return (void*)(b+funcs[ords[i]]);
    }
    return 0;
}
static void *ResolveLoadedExport(char *wanted){
    u8 *peb,*ldr,*head,*link;
    __asm {
        mov eax, fs:[30h]
        mov peb, eax
    }
    if(!peb)return 0;
    ldr=*(u8**)(peb+0x0C);
    if(!ldr)return 0;
    head=ldr+0x14;
    link=*(u8**)head;
    while(link&&link!=head){
        u8 *entry=link-0x08;
        void *base=*(void**)(entry+0x18);
        void *fn=FindExportInModule(base,wanted);
        if(fn)return fn;
        link=*(u8**)link;
    }
    return 0;
}

static NtProtectVirtualMemoryFn pNtProtectVirtualMemory=0;
static RegisterClassWFn pRegisterClassW=0;
static CreateWindowExWFn pCreateWindowExW=0;
static DestroyWindowFn pDestroyWindow=0;
static ShowWindowFn pShowWindow=0;
static SetWindowPosFn pSetWindowPos=0;
static GetForegroundWindowFn pGetForegroundWindow=0;
static GetWindowRectFn pGetWindowRect=0;
static GetAsyncKeyStateFn pGetAsyncKeyState=0;
static InvalidateRectFn pInvalidateRect=0;
static UpdateWindowFn pUpdateWindow=0;
static BeginPaintFn pBeginPaint=0;
static EndPaintFn pEndPaint=0;
static DefWindowProcWFn pDefWindowProcW=0;
static FillRectFn pFillRect=0;
static CreateSolidBrushFn pCreateSolidBrush=0;
static DeleteObjectFn pDeleteObject=0;
static SetTextColorFn pSetTextColor=0;
static SetBkModeFn pSetBkMode=0;
static TextOutWFn pTextOutW=0;

static BOOL ResolveAPIs(void){
    if(pGetAsyncKeyState)return TRUE;
    pNtProtectVirtualMemory=(NtProtectVirtualMemoryFn)ResolveLoadedExport("NtProtectVirtualMemory");
    pRegisterClassW=(RegisterClassWFn)ResolveLoadedExport("RegisterClassW");
    pCreateWindowExW=(CreateWindowExWFn)ResolveLoadedExport("CreateWindowExW");
    pDestroyWindow=(DestroyWindowFn)ResolveLoadedExport("DestroyWindow");
    pShowWindow=(ShowWindowFn)ResolveLoadedExport("ShowWindow");
    pSetWindowPos=(SetWindowPosFn)ResolveLoadedExport("SetWindowPos");
    pGetForegroundWindow=(GetForegroundWindowFn)ResolveLoadedExport("GetForegroundWindow");
    pGetWindowRect=(GetWindowRectFn)ResolveLoadedExport("GetWindowRect");
    pGetAsyncKeyState=(GetAsyncKeyStateFn)ResolveLoadedExport("GetAsyncKeyState");
    pInvalidateRect=(InvalidateRectFn)ResolveLoadedExport("InvalidateRect");
    pUpdateWindow=(UpdateWindowFn)ResolveLoadedExport("UpdateWindow");
    pBeginPaint=(BeginPaintFn)ResolveLoadedExport("BeginPaint");
    pEndPaint=(EndPaintFn)ResolveLoadedExport("EndPaint");
    pDefWindowProcW=(DefWindowProcWFn)ResolveLoadedExport("DefWindowProcW");
    pFillRect=(FillRectFn)ResolveLoadedExport("FillRect");
    pCreateSolidBrush=(CreateSolidBrushFn)ResolveLoadedExport("CreateSolidBrush");
    pDeleteObject=(DeleteObjectFn)ResolveLoadedExport("DeleteObject");
    pSetTextColor=(SetTextColorFn)ResolveLoadedExport("SetTextColor");
    pSetBkMode=(SetBkModeFn)ResolveLoadedExport("SetBkMode");
    pTextOutW=(TextOutWFn)ResolveLoadedExport("TextOutW");
    return pNtProtectVirtualMemory&&pRegisterClassW&&pCreateWindowExW&&pShowWindow&&
           pSetWindowPos&&pGetForegroundWindow&&pGetWindowRect&&pGetAsyncKeyState&&
           pInvalidateRect&&pUpdateWindow&&pBeginPaint&&pEndPaint&&pDefWindowProcW&&
           pFillRect&&pCreateSolidBrush&&pDeleteObject&&pSetTextColor&&pSetBkMode&&pTextOutW;
}
static BOOL ProtectMemory(void *address,u32 len,u32 newProtect,u32 *oldProtect){
    void *base=address;u32 size=len,old=0;
    if(!pNtProtectVirtualMemory&&!ResolveAPIs())return FALSE;
    if(pNtProtectVirtualMemory((void*)-1,&base,&size,newProtect,&old)<0)return FALSE;
    if(oldProtect)*oldProtect=old;
    return TRUE;
}

/* -------------------- Cheat patches -------------------- */
#define MAX_PATCH_HITS 16
typedef struct PatchDef{
    const u8 *findBytes;u32 findLen;
    const u8 *patchBytes;u32 patchLen;
    void *hits[MAX_PATCH_HITS];u32 hitCount;
    BOOL discovered;BOOL enabled;
} PatchDef;

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
static BOOL BytesEqual(const u8*a,const u8*b,u32 n){
    u32 i;for(i=0;i<n;++i)if(a[i]!=b[i])return FALSE;return TRUE;
}
static void DiscoverPatch(PatchDef*p){
    u8*begin=(u8*)0x00401000;u32 size=0x00B918EF,i;
    if(p->discovered)return;
    p->hitCount=0;
    for(i=0;i+p->findLen<=size;++i){
        if(BytesEqual(begin+i,p->findBytes,p->findLen)){
            if(p->hitCount<MAX_PATCH_HITS)p->hits[p->hitCount++]=begin+i;
            i+=p->findLen-1;
        }
    }
    p->discovered=TRUE;
}
static void WriteBytes(void*address,const u8*bytes,u32 len){
    u32 oldProtect=0,ignored=0,i;
    if(!address||!len)return;
    if(!ProtectMemory(address,len,PAGE_EXECUTE_READWRITE,&oldProtect))return;
    for(i=0;i<len;++i)((u8*)address)[i]=bytes[i];
    ProtectMemory(address,len,oldProtect,&ignored);
}
static void TogglePatch(u32 index){
    PatchDef*p;u32 i;
    if(index>=5)return;
    p=&g_Patches[index];DiscoverPatch(p);
    if(!p->hitCount)return;
    if(!p->enabled){
        for(i=0;i<p->hitCount;++i)WriteBytes(p->hits[i],p->patchBytes,p->patchLen);
        p->enabled=TRUE;
    }else{
        for(i=0;i<p->hitCount;++i)WriteBytes(p->hits[i],p->findBytes,p->patchLen);
        p->enabled=FALSE;
    }
}

/* -------------------- Independent Win32 overlay -------------------- */
static const WCHAR CLASS_NAME[]={ 'L','u','v','v','y','O','v','e','r','l','a','y',0 };
static const WCHAR TITLE_TEXT[]={ 'L','U','V','V','Y',' ','C','H','E','A','T','S',0 };
static const WCHAR HELP_TEXT[]={ 'F','1','0',':',' ','C','l','o','s','e',' ',' ',' ','A','r','r','o','w','s',':',' ','M','o','v','e',' ',' ',' ','E','n','t','e','r',':',' ','T','o','g','g','l','e',0 };

static const WCHAR L_HEALTH_OFF[]={ 'I','m','m','o','r','t','a','l',':',' ','O','F','F',0 };
static const WCHAR L_HEALTH_ON[] ={ 'I','m','m','o','r','t','a','l',':',' ','O','N',0 };
static const WCHAR L_DETECT_OFF[]={ 'N','e','v','e','r',' ','D','e','t','e','c','t','e','d',':',' ','O','F','F',0 };
static const WCHAR L_DETECT_ON[] ={ 'N','e','v','e','r',' ','D','e','t','e','c','t','e','d',':',' ','O','N',0 };
static const WCHAR L_MANA_OFF[]  ={ 'I','n','f','i','n','i','t','e',' ','M','a','n','a',':',' ','O','F','F',0 };
static const WCHAR L_MANA_ON[]   ={ 'I','n','f','i','n','i','t','e',' ','M','a','n','a',':',' ','O','N',0 };
static const WCHAR L_AMMO_OFF[]  ={ 'I','n','f','i','n','i','t','e',' ','A','m','m','o',':',' ','O','F','F',0 };
static const WCHAR L_AMMO_ON[]   ={ 'I','n','f','i','n','i','t','e',' ','A','m','m','o',':',' ','O','N',0 };
static const WCHAR L_POT_OFF[]   ={ 'I','n','f','i','n','i','t','e',' ','P','o','t','i','o','n','s',':',' ','O','F','F',0 };
static const WCHAR L_POT_ON[]    ={ 'I','n','f','i','n','i','t','e',' ','P','o','t','i','o','n','s',':',' ','O','N',0 };

static HWND g_Overlay=0;
static BOOL g_Open=FALSE;
static int g_Selected=0;
static BOOL g_ClassRegistered=FALSE;
static BOOL g_KeyPrev[256];

static int WLen(const WCHAR*s){int n=0;while(s[n])++n;return n;}
static u32 RGBc(u8 r,u8 g,u8 b){return ((u32)r)|((u32)g<<8)|((u32)b<<16);}

static const WCHAR *GetLabel(int i){
    if(i==0)return g_Patches[0].enabled?L_HEALTH_ON:L_HEALTH_OFF;
    if(i==1)return g_Patches[1].enabled?L_DETECT_ON:L_DETECT_OFF;
    if(i==2)return g_Patches[2].enabled?L_MANA_ON:L_MANA_OFF;
    if(i==3)return g_Patches[3].enabled?L_AMMO_ON:L_AMMO_OFF;
    return g_Patches[4].enabled?L_POT_ON:L_POT_OFF;
}

static LRESULT __stdcall OverlayWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){
        PAINTSTRUCT ps;HDC dc;RECT r;HBRUSH bg;int i;
        dc=pBeginPaint(hwnd,&ps);
        r.left=0;r.top=0;r.right=440;r.bottom=310;
        bg=pCreateSolidBrush(RGBc(15,15,18));
        pFillRect(dc,&r,bg);
        pDeleteObject(bg);
        pSetBkMode(dc,TRANSPARENT);
        pSetTextColor(dc,RGBc(235,235,240));
        pTextOutW(dc,22,18,TITLE_TEXT,WLen(TITLE_TEXT));
        for(i=0;i<5;++i){
            const WCHAR*label=GetLabel(i);
            if(i==g_Selected)pSetTextColor(dc,RGBc(255,215,90));
            else pSetTextColor(dc,RGBc(235,235,240));
            if(i==g_Selected){
                static const WCHAR ARROW[]={'>',' ',0};
                pTextOutW(dc,22,68+i*34,ARROW,2);
            }
            pTextOutW(dc,46,68+i*34,label,WLen(label));
        }
        pSetTextColor(dc,RGBc(160,160,170));
        pTextOutW(dc,22,255,HELP_TEXT,WLen(HELP_TEXT));
        pEndPaint(hwnd,&ps);
        return 0;
    }
    return pDefWindowProcW(hwnd,msg,wp,lp);
}
static BOOL EnsureOverlay(void){
    WNDCLASSW wc;
    if(g_Overlay)return TRUE;
    if(!ResolveAPIs())return FALSE;
    if(!g_ClassRegistered){
        wc.style=0;wc.lpfnWndProc=OverlayWndProc;wc.cbClsExtra=0;wc.cbWndExtra=0;
        wc.hInstance=g_Instance;wc.hIcon=0;wc.hCursor=0;wc.hbrBackground=0;
        wc.lpszMenuName=0;wc.lpszClassName=CLASS_NAME;
        pRegisterClassW(&wc);
        g_ClassRegistered=TRUE;
    }
    g_Overlay=pCreateWindowExW(
        WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
        CLASS_NAME,TITLE_TEXT,WS_POPUP|WS_BORDER,
        100,100,440,310,0,0,g_Instance,0
    );
    return g_Overlay?TRUE:FALSE;
}
static void PositionOverlay(void){
    HWND fg;RECT rr;int x,y;
    if(!g_Overlay)return;
    fg=pGetForegroundWindow();
    if(fg&&pGetWindowRect(fg,&rr)){
        x=rr.left+((rr.right-rr.left)-440)/2;
        y=rr.top+((rr.bottom-rr.top)-310)/2;
        pSetWindowPos(g_Overlay,HWND_TOPMOST,x,y,440,310,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    }
}
static BOOL KeyPressed(int vk){
    BOOL down=(pGetAsyncKeyState(vk)&0x8000)?TRUE:FALSE;
    BOOL pressed=down&&!g_KeyPrev[vk];
    g_KeyPrev[vk]=down;
    return pressed;
}
static void Redraw(void){
    if(g_Overlay){pInvalidateRect(g_Overlay,0,TRUE);pUpdateWindow(g_Overlay);}
}
static void SetOpen(BOOL open){
    g_Open=open;
    if(open){
        if(EnsureOverlay()){PositionOverlay();pShowWindow(g_Overlay,SW_SHOWNOACTIVATE);Redraw();}
    }else if(g_Overlay)pShowWindow(g_Overlay,SW_HIDE);
}
static void OverlayTick(void *gfx){
    (void)gfx;
    if(!ResolveAPIs())return;

    if(KeyPressed(VK_F10)){SetOpen(!g_Open);return;}
    if(!g_Open)return;

    PositionOverlay();

    if(KeyPressed(VK_ESCAPE)){SetOpen(FALSE);return;}
    if(KeyPressed(VK_UP)){g_Selected=(g_Selected+4)%5;Redraw();}
    if(KeyPressed(VK_DOWN)){g_Selected=(g_Selected+1)%5;Redraw();}
    if(KeyPressed(VK_RETURN)){TogglePatch((u32)g_Selected);Redraw();}

    if(KeyPressed(VK_1)){TogglePatch(0);g_Selected=0;Redraw();}
    if(KeyPressed(VK_2)){TogglePatch(1);g_Selected=1;Redraw();}
    if(KeyPressed(VK_3)){TogglePatch(2);g_Selected=2;Redraw();}
    if(KeyPressed(VK_4)){TogglePatch(3);g_Selected=3;Redraw();}
    if(KeyPressed(VK_5)){TogglePatch(4);g_Selected=4;Redraw();}
}

/* Hook the already-verified per-frame Dishonored D3D path.
   Original bytes at 0x009C0710:
   55 8B EC 8B 45 08
*/
__declspec(naked) static void OverlayFrameHook(void){
    __asm {
        mov eax, [esp+04h]
        push eax
        call OverlayTick
        add esp, 04h

        push ebp
        mov ebp, esp
        mov eax, [ebp+08h]

        mov edx, 009C0716h
        jmp edx
    }
}
static BOOL InstallFrameHook(void){
    u8 *target=(u8*)0x009C0710;
    const u8 expected[6]={0x55,0x8B,0xEC,0x8B,0x45,0x08};
    u32 oldProtect=0,ignored=0,i;
    for(i=0;i<6;++i)if(target[i]!=expected[i])return FALSE;
    if(!ProtectMemory(target,6,PAGE_EXECUTE_READWRITE,&oldProtect))return FALSE;
    target[0]=0xE9;
    *(LONG*)(target+1)=(LONG)((u8*)&OverlayFrameHook-(target+5));
    target[5]=0x90;
    ProtectMemory(target,6,oldProtect,&ignored);
    return TRUE;
}

BOOL __stdcall DllMainCRTStartup(HINSTANCE hinstDLL,DWORD reason,void*reserved){
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH){
        g_Instance=hinstDLL;
        ResolveAPIs();
        InstallFrameHook();
    }
    return TRUE;
}
