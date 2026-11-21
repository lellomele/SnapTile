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

static LRESULT CALLBACK barra_proc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    LONG_PTR hit = GetWindowLongPtrW(h, GWLP_USERDATA);
    if (msg == WM_NCHITTEST) return hit;
    if (msg == WM_GETTITLEBARINFOEX) {
        TITLEBARINFOEX *info = (TITLEBARINFOEX *)l;
        SetRectEmpty(&info->rgrect[3]);
        info->rgstate[3] = hit == HTCAPTION ? STATE_SYSTEM_INVISIBLE : 0;
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

static void prova_barra(int hit)
{
    HINSTANCE istanza = GetModuleHandleW(NULL);
    WNDCLASSW wc = {.lpfnWndProc=barra_proc, .hInstance=istanza, .lpszClassName=L"SnapTile.Test.Barra"};
    ATOM frame = RegisterClassW(&wc);
    HWND h = frame ? CreateWindowW(MAKEINTATOM(frame), L"", WS_OVERLAPPEDWINDOW, 20, 20, 720, 500,
                                   NULL, NULL, istanza, NULL) : NULL;
    BOOL ok = FALSE;
    RECT r, ancora;
    if (h && GetWindowRect(h, &r)) {
        int dpi = dpi_monitor(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST));
        POINT p = {r.right-MulDiv(72, dpi, 96), r.top+MulDiv(18, dpi, 96)};
        SetWindowLongPtrW(h, GWLP_USERDATA, hit);
        ok = esamina_pulsante(h, p, &ancora)==DA_VERIFICARE && PtInRect(&ancora, p);
        POINT contenuto = {p.x, r.top+MulDiv(90, dpi, 96)};
        POINT fuori_angolo = {r.right-MulDiv(220, dpi, 96), p.y};
        ok = ok && esamina_pulsante(h, contenuto, &ancora)==NESSUN_PULSANTE &&
             esamina_pulsante(h, fuori_angolo, &ancora)==NESSUN_PULSANTE;
        SetWindowLongPtrW(h, GWLP_USERDATA, HTCLOSE);
        ok = ok && esamina_pulsante(h, p, &ancora)==NESSUN_PULSANTE;
        SetWindowLongPtrW(h, GWLP_USERDATA, HTMAXBUTTON);
        ok = ok && esamina_pulsante(h, p, &ancora)==PULSANTE_NATIVO;
    }
    if (h) DestroyWindow(h);
    if (frame) UnregisterClassW(MAKEINTATOM(frame), istanza);
    controlla(ok, hit == HTCLIENT ? "Custom frame: client hit and empty maximize bounds" :
                                    "Custom frame: caption hit and invisible native maximize button");
}

/* La radice espone i vecchi rettangoli, mentre la barra figlia occupa
   realmente i pixel dei pulsanti e risponde a WM_NCHITTEST. */
static LRESULT CALLBACK cornice_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_NCCALCSIZE) return 0;
    if (m == WM_NCHITTEST) return GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_GETTITLEBARINFOEX) {
        TITLEBARINFOEX *t = (TITLEBARINFOEX *)l;
        RECT r;
        GetWindowRect(h, &r);
        SetRect(&t->rgrect[2], r.right-130, r.top+11, r.right-89, r.top+52);
        SetRect(&t->rgrect[3], r.right-91, r.top+11, r.right-50, r.top+52);
        return TRUE;
    }
    return DefWindowProcW(h, m, w, l);
}

static void prova_hit_figlio(void)
{
    HINSTANCE istanza = GetModuleHandleW(NULL);
    WNDCLASSW wc = {.lpfnWndProc=cornice_proc, .hInstance=istanza, .lpszClassName=L"SnapTile.Test.Cornice"};
    ATOM classe = RegisterClassW(&wc);
    HWND h = classe ? CreateWindowExW(WS_EX_TOPMOST, MAKEINTATOM(classe), L"",
        WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
        40, 40, 560, 240, NULL, NULL, istanza, NULL) : NULL;
    if (h) {
        SetWindowLongPtrW(h, GWLP_USERDATA, HTCAPTION);
        ShowWindow(h, SW_SHOWNOACTIVATE);
    }
    HWND figlio = h ? CreateWindowW(MAKEINTATOM(classe), L"", WS_CHILD | WS_VISIBLE,
        0, 0, 560, 64, h, NULL, istanza, NULL) : NULL;
    BOOL riduci = FALSE, fuori = FALSE, sinistra = FALSE, client = FALSE, altro = FALSE, mdi = FALSE;
    RECT r, ancora;
    if (figlio && idonea(h) && GetWindowRect(h, &r)) {
        POINT p = {r.right-129, r.top+44};
        SetWindowLongPtrW(figlio, GWLP_USERDATA, HTMAXBUTTON);
        riduci = WindowFromPoint(p) == figlio && esamina_pulsante(h, p, &ancora) == PULSANTE_NATIVO &&
                 PtInRect(&ancora, p);
        p = (POINT){r.right-132, r.top+43};
        fuori = WindowFromPoint(p) == figlio && esamina_pulsante(h, p, &ancora) == PULSANTE_NATIVO &&
                PtInRect(&ancora, p);
        p = (POINT){r.left+40, r.top+43};
        sinistra = WindowFromPoint(p) == figlio && esamina_pulsante(h, p, &ancora) == PULSANTE_NATIVO &&
                   PtInRect(&ancora, p);
        SetWindowLongPtrW(h, GWLP_USERDATA, HTCLIENT);
        p = (POINT){r.right-129, r.top+44};
        client = WindowFromPoint(p) == figlio && esamina_pulsante(h, p, &ancora) == PULSANTE_NATIVO &&
                 PtInRect(&ancora, p);
        SetWindowLongPtrW(h, GWLP_USERDATA, HTCAPTION);
        p = (POINT){r.right-70, r.top+43};
        SetWindowLongPtrW(figlio, GWLP_USERDATA, HTCLOSE);
        altro = WindowFromPoint(p) == figlio && esamina_pulsante(h, p, &ancora) == NESSUN_PULSANTE;
        SetWindowLongPtrW(figlio, GWLP_USERDATA, HTMINBUTTON);
        altro = altro && WindowFromPoint(p) == figlio && esamina_pulsante(h, p, &ancora) == NESSUN_PULSANTE;
        SetWindowLongPtrW(figlio, GWL_EXSTYLE, GetWindowLongPtrW(figlio, GWL_EXSTYLE) | WS_EX_MDICHILD);
        SetWindowLongPtrW(figlio, GWLP_USERDATA, HTMAXBUTTON);
        p = (POINT){r.right-132, r.top+43};
        mdi = WindowFromPoint(p) == figlio && esamina_pulsante(h, p, &ancora) == NESSUN_PULSANTE;
    }
    if (h) DestroyWindow(h);
    if (classe) UnregisterClassW(MAKEINTATOM(classe), istanza);
    controlla(riduci, "Child maximize hit inside the root's minimize rectangle");
    controlla(fuori, "Child maximize hit outside the root's caption buttons");
    controlla(sinistra, "Child maximize hit on the left of an LTR caption");
    controlla(client, "Child maximize hit in the expected corner with a root client hit");
    controlla(altro, "Child close and minimize hits inside the root's maximize rectangle");
    controlla(mdi, "Embedded MDI maximize hit does not target the root window");
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

static void prova_gruppo_dwm(void)
{
    RECT finestra = {76,81,1854,1195}, gruppo = {1508,0,1767,51};
    RECT centro = centro_pulsanti_dwm(finestra, gruppo, 168, FALSE);
    controlla(PtInRect(&centro, (POINT){1716,114}) &&
              !PtInRect(&centro, (POINT){1627,114}) && !PtInRect(&centro, (POINT){1800,114}) &&
              !PtInRect(&centro, (POINT){1716,145}), "Reported hover point and adjacent caption buttons");

    OffsetRect(&finestra, -2500, -100);
    centro = centro_pulsanti_dwm(finestra, gruppo, 168, FALSE);
    BOOL ok = PtInRect(&centro, (POINT){-784,14});
    RECT speculare = {11,0,270,51};
    centro = centro_pulsanti_dwm(finestra, speculare, 168, TRUE);
    ok = ok && PtInRect(&centro, (POINT){-2283,14});
    centro = centro_pulsanti_dwm(finestra, speculare, 168, FALSE);
    controlla(ok && IsRectEmpty(&centro), "Caption group on a negative-coordinate monitor and mirrored frame");

    RECT normale = {0,0,1200,800}, standard = {1055,0,1193,30};
    centro = centro_pulsanti_dwm(normale, standard, 96, FALSE);
    ok = PtInRect(&centro, (POINT){1124,16});
    const RECT anomali[] = {{0,0,0,0}, {900,0,1000,30}, {1055,70,1193,100},
                            {600,0,1193,30}, {1170,0,1193,30}, {1062,0,1201,30}};
    for (unsigned i = 0; i < ARRAYSIZE(anomali); ++i) {
        centro = centro_pulsanti_dwm(normale, anomali[i], 96, FALSE);
        ok = ok && IsRectEmpty(&centro);
    }
    controlla(ok, "96 DPI caption group and rejection of unusable bounds");
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
    prova_gruppo_dwm();
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
    prova_barra(HTCLIENT);
    prova_barra(HTCAPTION);
    prova_hit_figlio();
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
    controlla(!IsRectEmpty(&t) && esamina_pulsante(a,p,&rilevato) == PULSANTE_NATIVO,
              "Maximize and Restore button detection");
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
