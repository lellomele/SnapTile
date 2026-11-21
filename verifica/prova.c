/* Il processo figlio gestisce vere finestre Win32. Il filtro limita tutti
   gli spostamenti a quel processo, anche nei sandbox privi di altri desktop. */
#include <windows.h>
static DWORD pid_prova;
typedef struct { WNDENUMPROC callback; LPARAM param; } Filtro;
static BOOL CALLBACK solo_figlio(HWND h, LPARAM l)
{
    Filtro *f=(Filtro *)l;
    DWORD pid=0;
    GetWindowThreadProcessId(h,&pid);
    return pid==pid_prova ? f->callback(h,f->param) : TRUE;
}
static BOOL elenca_prova(WNDENUMPROC callback, LPARAM l)
{
    Filtro f={callback,l};
    return EnumWindows(solo_figlio,(LPARAM)&f);
}
static HWND sotto_mouse;
static HWND primo_simulato;
static HWND primo_prova(void)
{
    return primo_simulato ? primo_simulato : GetForegroundWindow();
}
static POINT mouse_simulato;
static BOOL puntatore_prova(POINT *p)
{
    if (!sotto_mouse) return GetCursorPos(p);
    *p=mouse_simulato;
    return TRUE;
}
static HWND finestra_prova(POINT p)
{
    return sotto_mouse ? sotto_mouse : WindowFromPoint(p);
}
#define EnumWindows elenca_prova
#define GetCursorPos puntatore_prova
#define WindowFromPoint finestra_prova
#define GetForegroundWindow primo_prova
#include "../src/principale.c"
#undef EnumWindows
#undef GetCursorPos
#undef WindowFromPoint
#undef GetForegroundWindow
#include <stdio.h>
#include <shellscalingapi.h>
#include <dwmapi.h>

static int errori;

static void controlla(BOOL ok, const char *nome)
{
    printf("%s  %s\n", ok ? "OK" : "FAIL", nome);
    if (!ok) errori++;
}

static LRESULT CALLBACK prova_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_APP + 20) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static int crea_finestre(void)
{
    WNDCLASSW wc = {.lpfnWndProc=prova_proc, .hInstance=GetModuleHandleW(NULL),
                    .lpszClassName=L"SnapTile.Test.App"};
    if (!RegisterClassW(&wc)) return 10;
    const WCHAR *nomi[] = {L"Test Alfa", L"Test Beta", L"Test Gamma", L"Test Delta"};
    for (int i=0; i<4; i++) {
        HWND h = CreateWindowW(wc.lpszClassName, nomi[i], WS_OVERLAPPEDWINDOW,
                               80+15*i, 60+15*i, 420, 320, NULL,NULL,wc.hInstance,NULL);
        if (!h) return 11;
        if (i < 2) ShowWindow(h, SW_SHOWNOACTIVATE);
    }
    MSG m;
    while (GetMessageW(&m,NULL,0,0)>0) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}

static void pompa(DWORD ms)
{
    ULONGLONG fine = GetTickCount64()+ms;
    do {
        MSG m;
        while (PeekMessageW(&m,NULL,0,0,PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(8);
    } while (GetTickCount64()<fine);
}

static BOOL attendi_lavori(void)
{
    ULONGLONG fine=GetTickCount64()+2400;
    while (n_lavori && GetTickCount64()<fine) pompa(20);
    return n_lavori==0 && fallimenti==0;
}

static BOOL stessa_zona(HWND h, enum Schema s, int z)
{
    RECT r, work, prevista;
    if (!area_monitor(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &work)) return FALSE;
    prevista = rettangolo_zona(s,z,work);
    if (FAILED(DwmGetWindowAttribute(h,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))) {
        if (!GetWindowRect(h,&r)) return FALSE;
    }
    return abs(r.left-prevista.left)<=1 && abs(r.top-prevista.top)<=1 &&
           abs(r.right-prevista.right)<=1 && abs(r.bottom-prevista.bottom)<=1;
}

static BOOL avvia_figlio(WCHAR *programma, WCHAR *argomenti, WCHAR *desktop, PROCESS_INFORMATION *pi)
{
    STARTUPINFOW si = {.cb=sizeof(si), .lpDesktop=desktop};
    return CreateProcessW(programma,argomenti,NULL,NULL,FALSE,0,NULL,NULL,&si,pi);
}

static void geometrie(void)
{
    const RECT prove[] = {{0,0,1919,1039},{-1919,-301,0,738},{-500,40,1,879}};
    BOOL ok=TRUE;
    for (unsigned a=0; a<ARRAYSIZE(prove); a++)
        for (int s=0; s<NUM_SCHEMI; s++) {
            RECT work=prove[a], usate[MAX_ZONE];
            LONG somma=0;
            for (int z=0; z<numero_zone(s); z++) {
                RECT r=rettangolo_zona(s,z,work);
                if (r.left<work.left || r.top<work.top || r.right>work.right ||
                    r.bottom>work.bottom || IsRectEmpty(&r)) ok=FALSE;
                for (int j=0; j<z; j++) { RECT comune; if (IntersectRect(&comune,&r,&usate[j])) ok=FALSE; }
                usate[z]=r;
                somma+=(r.right-r.left)*(r.bottom-r.top);
            }
            if (somma!=(work.right-work.left)*(work.bottom-work.top)) ok=FALSE;
        }
    controlla(ok,"Four layouts, negative coordinates and odd resolutions");
}

static void prova_avvio(void)
{
    WCHAR nome[100];
    wsprintfW(nome,L"Software\\SnapTile.Prova.%lu.%lu",GetCurrentProcessId(),GetTickCount());
    HKEY radice, run;
    DWORD creata;
    LONG e=RegCreateKeyExW(HKEY_CURRENT_USER,nome,0,NULL,REG_OPTION_VOLATILE,
                           KEY_ALL_ACCESS,NULL,&radice,&creata);
    if (e!=ERROR_SUCCESS) {
        printf("SKIPPED: volatile test registry unavailable (%ld)\n",e);
        return;
    }
    if (creata!=REG_CREATED_NEW_KEY) { RegCloseKey(radice); return; }
    e=RegCreateKeyExW(radice,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0,NULL,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,NULL,&run,NULL);
    if (e==ERROR_SUCCESS) {
        RegCloseKey(run);
        e=RegOverridePredefKey(HKEY_CURRENT_USER,radice);
    }
    if (e==ERROR_SUCCESS) {
        BOOL ok=!avvio_attivo() && imposta_avvio(TRUE)==ERROR_SUCCESS && avvio_attivo();
        ok=ok && imposta_avvio(FALSE)==ERROR_SUCCESS && !avvio_attivo();
        RegOverridePredefKey(HKEY_CURRENT_USER,NULL);
        controlla(ok,"Startup registration and removal in a test registry");
    } else printf("SKIPPED: isolated registry unavailable (%ld)\n",e);
    RegDeleteTreeW(radice,NULL);
    RegCloseKey(radice);
    RegDeleteKeyW(HKEY_CURRENT_USER,nome);
}

int main(int argc, char **argv)
{
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if (argc>1 && !lstrcmpA(argv[1],"--finestre")) return crea_finestre();
    geometrie();
    prova_avvio();
    WCHAR nome_desktop[80], exe[MAX_PATH], arg[]=L"prova.exe --finestre";
    wsprintfW(nome_desktop,L"SnapTileTest%lu",GetCurrentProcessId());
    HDESK precedente=GetThreadDesktop(GetCurrentThreadId());
    DWORD diritti=DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_ENUMERATE |
                  DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS;
    HDESK desk=CreateDesktopW(nome_desktop,NULL,NULL,0,diritti,NULL);
    if (!desk || !SetThreadDesktop(desk)) {
        if (desk) CloseDesktop(desk);
        desk=NULL;
        puts("Using current desktop with test windows isolated by process ID.");
    }
    GetModuleFileNameW(NULL,exe,MAX_PATH);
    PROCESS_INFORMATION pi={0};
    if (!avvia_figlio(exe,arg,desk ? nome_desktop : NULL,&pi)) {
        printf("Test process failed to start: %lu\n",GetLastError());
        return 3;
    }
    pid_prova=pi.dwProcessId;
    ULONGLONG limite=GetTickCount64()+4000;
    while (!FindWindowW(L"SnapTile.Test.App",L"Test Delta") &&
           GetTickCount64()<limite && WaitForSingleObject(pi.hProcess,15)==WAIT_TIMEOUT) { }
    HWND a=FindWindowW(L"SnapTile.Test.App",L"Test Alfa");
    HWND b=FindWindowW(L"SnapTile.Test.App",L"Test Beta");
    HWND c=FindWindowW(L"SnapTile.Test.App",L"Test Gamma");
    HWND d=FindWindowW(L"SnapTile.Test.App",L"Test Delta");
    if (!a || !b || !c || !d) {
        DWORD codice=0;
        GetExitCodeProcess(pi.hProcess,&codice);
        printf("Test windows unavailable. Child status %lu\n",codice);
        TerminateProcess(pi.hProcess,2);
        return 4;
    }
    WNDCLASSW wc={.hInstance=GetModuleHandleW(NULL),.lpfnWndProc=procedura_servizio,
                  .lpszClassName=L"SnapTile.Test.Servizio"};
    RegisterClassW(&wc);
    servizio=CreateWindowW(wc.lpszClassName,L"",WS_POPUP,0,0,0,0,NULL,NULL,wc.hInstance,NULL);
    wc.lpszClassName=L"SnapTile.Test.Pannello";
    wc.lpfnWndProc=procedura_pannello;
    RegisterClassW(&wc);
    pannello=CreateWindowW(wc.lpszClassName,L"",WS_POPUP,0,0,0,0,servizio,NULL,wc.hInstance,NULL);
    RECT ancora={400,60,445,90};
    BOOL ok=apri_pannello(a,ancora,FALSE);
    controlla(ok && n_schede==4 && n_app==2, "Two windows: four layouts");
    TITLEBARINFOEX tb={.cbSize=sizeof(tb)};
    SendMessageW(a,WM_GETTITLEBARINFOEX,0,(LPARAM)&tb);
    RECT t=tb.rgrect[3], rilevato;
    POINT p={(t.left+t.right)/2,(t.top+t.bottom)/2};
    controlla(!IsRectEmpty(&t) && pulsante_ingrandisci(a,p,&rilevato), "Maximize and Restore button detection");
    chiudi(FALSE);
    attendi_uscita=FALSE;
    sotto_mouse=a; mouse_simulato=p;
    controlla_mouse();
    BOOL chiuso_inizialmente=!aperto;
    pompa(470);
    controlla_mouse();
    controlla(chiuso_inizialmente && aperto && !da_tastiera && bersaglio.hwnd==a,
              "Hover delay opens the panel");
    sotto_mouse=NULL;
    if (ok) {
        RECT z=schede[1].zone[1];
        LPARAM punto=MAKELPARAM((z.left+z.right)/2,(z.top+z.bottom)/2);
        SendMessageW(pannello,WM_LBUTTONUP,0,punto);
        controlla(attendi_lavori() && stessa_zona(a,ASIMMETRICO,1), "Click on the right 40% zone");
    }
    ShowWindowAsync(a,SW_MAXIMIZE);
    pompa(300);
    controlla(IsZoomed(a), "Window initially maximized");
    ultima=identifica(a);
    primo_simulato=a;
    SendMessageW(servizio,WM_HOTKEY,ID_SCORCIATOIA,0);
    primo_simulato=NULL;
    ok=aperto && da_tastiera && bersaglio.hwnd==a;
    if (ok) {
        SendMessageW(pannello,WM_KEYDOWN,VK_TAB,0);
        SendMessageW(pannello,WM_KEYDOWN,VK_RETURN,0);
    }
    BOOL completata=attendi_lavori();
    if (!ok || !completata || IsZoomed(a) || !stessa_zona(a,META,1))
        printf("Details: panel %d, complete %d, maximized %d, zone %d\n",
               ok,completata,IsZoomed(a),stessa_zona(a,META,1));
    controlla(ok && completata && !IsZoomed(a) && stessa_zona(a,META,1),
              "Tab and Enter restore and tile a maximized window");
    ShowWindowAsync(c,SW_SHOWNOACTIVATE); ShowWindowAsync(d,SW_SHOWNOACTIVATE);
    pompa(200);
    ok=apri_pannello(a,ancora,FALSE);
    controlla(ok && n_schede==6 && n_app==4, "Four windows: four layouts and two suggestions");
    if (ok) {
        HWND prevista[3]={candidate[1].hwnd,candidate[0].hwnd,candidate[2].hwnd};
        segna(1,1); applica(FALSE);
        ok=attendi_lavori();
        for (int z=0; z<3; z++) if (!stessa_zona(prevista[z],TRE_ZONE,z)) ok=FALSE;
        controlla(ok,"Automatic placement of three external windows");
    }
    ok=apri_pannello(a,ancora,FALSE);
    if (ok) {
        HWND prevista[4]={candidate[1].hwnd,candidate[2].hwnd,candidate[3].hwnd,a};
        segna(n_schede-1,3); applica(TRUE);
        ok=attendi_lavori();
        for (int z=0; z<4; z++) if (!stessa_zona(prevista[z],QUATTRO_ZONE,z)) ok=FALSE;
        controlla(ok,"Shift fills four quadrants");
    }
    /* Una finestra scompare dopo l'apertura del menu. */
    ok=apri_pannello(d,ancora,FALSE);
    SendMessageW(d,WM_CLOSE,0,0);
    segna(n_schede-1,0); applica(FALSE);
    controlla(ok && !aperto && !n_lavori,"Target closed during selection");
    PostMessageW(a,WM_APP+20,0,0);
    WaitForSingleObject(pi.hProcess,2000);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    DestroyWindow(servizio);
    if (desk) { SetThreadDesktop(precedente); CloseDesktop(desk); }
    return errori ? 1 : 0;
}
