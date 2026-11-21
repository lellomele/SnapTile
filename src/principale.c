#include "affianca.h"
#include "avvio.h"
#include "../risorse/versione.h"
#include <windowsx.h>
#include <shellapi.h>
#include <limits.h>
#include <stdlib.h>

enum { ID_ICONA = 1, ID_SCORCIATOIA = 1, TIMER_MOUSE = 1, TIMER_SPOSTA,
       TIMER_ICONA, MSG_ICONA = WM_APP + 1, APRI = 10, AVVIO, GUIDA, INFO_APP, ESCI };
enum { MAX_SCHEDE = 6, LARGHEZZA = 276, MARGINE_X = 8, MARGINE_Y = 15,
       LARG_SCHEDA = 123, ALT_SCHEDA = 80, PASSO_X = 137, PASSO_Y = 95 };

typedef struct {
    enum Schema schema;
    BOOL proposta;
    RECT zone[MAX_ZONE];
    int app[MAX_ZONE];
} Scheda;

static HWND servizio, pannello;
static UINT msg_taskbar;
static NOTIFYICONDATAW icona;
static BOOL icona_presente, tasto_attivo, menu_tray;
static BOOL aperto, da_tastiera;
static HMONITOR monitor_pannello;
static RECT area_apertura, pulsante_apertura;
static RECT posizione_apertura;
static HWND focus_apertura, ultimo_primo;
static Finestra bersaglio, ultima, candidate[MAX_ZONE];
static HICON immagini[MAX_ZONE];  /* Icone condivise, di proprieta' delle app. */
static Scheda schede[MAX_SCHEDE];
static int n_app, n_schede, scelta_scheda, scelta_zona, dpi;
static ULONGLONG fuori_da;
static HWND osservata;
static RECT pulsante_osservato;
static ULONGLONG osservata_da;
static POINT ultimo_punto;
static BOOL punto_noto, attendi_uscita;
static Spostamento lavori[MAX_ZONE];
static int n_lavori, fallimenti;

static int scala(int n) { return MulDiv(n, dpi, 96); }

static void avviso(const WCHAR *testo)
{
    NOTIFYICONDATAW n = {.cbSize = sizeof(n), .hWnd = servizio, .uID = ID_ICONA,
                         .uFlags = NIF_INFO, .dwInfoFlags = NIIF_INFO};
    lstrcpynW(n.szInfoTitle, L"SnapTile", ARRAYSIZE(n.szInfoTitle));
    lstrcpynW(n.szInfo, testo, ARRAYSIZE(n.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

static void registra_icona(void)
{
    icona_presente = Shell_NotifyIconW(NIM_ADD, &icona);
    if (!icona_presente) icona_presente = Shell_NotifyIconW(NIM_MODIFY, &icona);
    if (icona_presente) KillTimer(servizio, TIMER_ICONA);
    else SetTimer(servizio, TIMER_ICONA, 3000, NULL);
}

static void chiudi(BOOL restituisci_focus)
{
    if (!aperto) return;
    BOOL aveva_focus = GetForegroundWindow() == pannello;
    aperto = FALSE;
    ShowWindow(pannello, SW_HIDE);
    if (restituisci_focus && aveva_focus && ancora_valida(bersaglio))
        SetForegroundWindow(bersaglio.hwnd);
    attendi_uscita = TRUE;
    osservata = NULL;
    punto_noto = FALSE;
}

static BOOL CALLBACK elenca(HWND hwnd, LPARAM unused)
{
    (void)unused;
    if (hwnd == bersaglio.hwnd || MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) != monitor_pannello ||
        (GetWindow(hwnd, GW_OWNER) && !(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW)) ||
        !idonea(hwnd)) return TRUE;
    candidate[n_app++] = identifica(hwnd);
    return n_app < MAX_ZONE;
}

static HICON icona_app(HWND hwnd)
{
    DWORD_PTR h = 0;
    SendMessageTimeoutW(hwnd, WM_GETICON, ICON_SMALL2, 0,
                        SMTO_ABORTIFHUNG | SMTO_BLOCK, 30, &h);
    if (!h) h = (DWORD_PTR)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    if (!h) h = (DWORD_PTR)GetClassLongPtrW(hwnd, GCLP_HICON);
    return h ? (HICON)h : LoadIconW(NULL, IDI_APPLICATION);
}

static void aggiungi_scheda(enum Schema modello, BOOL proposta)
{
    int i = n_schede++;
    Scheda *s = &schede[i];
    s->schema = modello;
    s->proposta = proposta;
    int x = MARGINE_X + (i % 2) * PASSO_X, y = MARGINE_Y + (i / 2) * PASSO_Y;
    RECT base = {scala(x), scala(y), scala(x + LARG_SCHEDA), scala(y + ALT_SCHEDA)};
    int spazio = scala(3);
    for (int z = 0; z < numero_zone(modello); z++) {
        RECT r = rettangolo_zona(modello, z, base);
        if (r.left > base.left) r.left += spazio;
        if (r.right < base.right) r.right -= spazio;
        if (r.top > base.top) r.top += spazio;
        if (r.bottom < base.bottom) r.bottom -= spazio;
        s->zone[z] = r;
        s->app[z] = -1;
    }
    if (proposta) {
        s->app[0] = 1;
        s->app[1] = 0;
        if (modello == TRE_ZONE) s->app[2] = 2;
    }
}

static LONG limita(LONG v, LONG minimo, LONG massimo)
{
    if (massimo < minimo || v < minimo) return minimo;
    return v > massimo ? massimo : v;
}

static BOOL apri_pannello(HWND hwnd, RECT ancora, BOOL tastiera)
{
    if (!idonea(hwnd) || n_lavori) return FALSE;
    monitor_pannello = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!area_monitor(monitor_pannello, &area_apertura)) return FALSE;
    bersaglio = identifica(hwnd);
    if (!GetWindowRect(hwnd, &posizione_apertura)) return FALSE;
    focus_apertura = GetForegroundWindow();
    ultima = bersaglio;
    dpi = dpi_monitor(monitor_pannello);
    /* Il menu resta intero anche su un monitor piccolo con forte ingrandimento. */
    int max_x = MulDiv(area_apertura.right-area_apertura.left, 96, LARGHEZZA);
    int max_y = MulDiv(area_apertura.bottom-area_apertura.top, 96, 300);
    if (dpi > max_x) dpi = max_x;
    if (dpi > max_y) dpi = max_y;
    candidate[0] = bersaglio;
    n_app = 1;
    EnumWindows(elenca, 0);
    n_schede = 0;
    if (n_app >= 3) {
        for (int a = 0; a < 3; a++) immagini[a] = icona_app(candidate[a].hwnd);
        aggiungi_scheda(META, TRUE);
        aggiungi_scheda(TRE_ZONE, TRUE);
    }
    for (int i = 0; i < NUM_SCHEMI; i++) aggiungi_scheda((enum Schema)i, FALSE);

    if (tastiera) {
        RECT r;
        if (!GetWindowRect(hwnd, &r)) return FALSE;
        ancora.right = limita(r.right - scala(70), area_apertura.left, area_apertura.right);
        ancora.left = ancora.right;
        ancora.bottom = limita(r.top + scala(36), area_apertura.top, area_apertura.bottom);
        ancora.top = ancora.bottom;
    }
    int w = scala(LARGHEZZA), h = scala(2*MARGINE_Y + ALT_SCHEDA + (n_schede/2-1)*PASSO_Y);
    int x = limita((ancora.left + ancora.right - w)/2, area_apertura.left, area_apertura.right-w);
    int y = limita(ancora.bottom + scala(2), area_apertura.top, area_apertura.bottom-h);
    pulsante_apertura = ancora;
    da_tastiera = tastiera;
    scelta_scheda = tastiera ? 0 : -1;
    scelta_zona = tastiera ? 0 : -1;
    fuori_da = 0;
    aperto = TRUE;
    LONG_PTR ex = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | (tastiera ? 0 : WS_EX_NOACTIVATE);
    SetWindowLongPtrW(pannello, GWL_EXSTYLE, ex);
    HRGN angoli = CreateRoundRectRgn(0, 0, w+1, h+1, scala(12), scala(12));
    if (angoli && !SetWindowRgn(pannello, angoli, FALSE)) DeleteObject(angoli);
    SetWindowPos(pannello, HWND_TOPMOST, x, y, w, h,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW | (tastiera ? 0 : SWP_NOACTIVATE));
    if (tastiera) { SetForegroundWindow(pannello); SetFocus(pannello); }
    InvalidateRect(pannello, NULL, FALSE);
    return TRUE;
}

static void segna(int i, int z)
{
    if (i == scelta_scheda && z == scelta_zona) return;
    scelta_scheda = i;
    scelta_zona = z;
    InvalidateRect(pannello, NULL, FALSE);
}

static void selezione_mouse(POINT p)
{
    for (int i = 0; i < n_schede; i++)
        for (int z = 0; z < numero_zone(schede[i].schema); z++)
            if (PtInRect(&schede[i].zone[z], p)) { segna(i, z); return; }
    segna(-1, -1);
}

static void verifica_lavori(void)
{
    ULONGLONG ora = GetTickCount64();
    for (int i = n_lavori-1; i >= 0; i--) {
        enum Esito e = avanza_spostamento(&lavori[i], ora);
        if (e == IN_CORSO) continue;
        if (e == FALLITA) fallimenti++;
        lavori[i] = lavori[--n_lavori];
    }
    if (!n_lavori) {
        KillTimer(servizio, TIMER_SPOSTA);
        if (fallimenti) avviso(L"Some windows could not fit the selected zones. Check their minimum size and permissions.");
    }
}

static void applica(BOOL completa)
{
    if (!aperto || scelta_scheda < 0 || scelta_zona < 0 || n_lavori) return;
    RECT area;
    if (!ancora_valida(bersaglio) || !idonea(bersaglio.hwnd) ||
        MonitorFromWindow(bersaglio.hwnd, MONITOR_DEFAULTTONULL) != monitor_pannello ||
        !area_monitor(monitor_pannello, &area)) { chiudi(FALSE); return; }
    Scheda *s = &schede[scelta_scheda];
    int prossimo = 1;
    fallimenti = 0;
    for (int z = 0; z < numero_zone(s->schema); z++) {
        int a = -1;
        if (s->proposta) a = s->app[z];
        else if (z == scelta_zona) a = 0;
        else if (completa && prossimo < n_app) a = prossimo++;
        if (a < 0) continue;
        Finestra f = candidate[a];
        if (!ancora_valida(f) || MonitorFromWindow(f.hwnd, MONITOR_DEFAULTTONULL) != monitor_pannello) {
            fallimenti++;
            continue;
        }
        lavori[n_lavori++] = (Spostamento){.finestra = f,
            .zona = rettangolo_zona(s->schema, z, area), .scadenza = GetTickCount64()+1800};
    }
    chiudi(TRUE);
    SetForegroundWindow(bersaglio.hwnd);
    if (n_lavori && !SetTimer(servizio, TIMER_SPOSTA, 70, NULL)) {
        n_lavori = 0;
        avviso(L"The window placement timer is unavailable.");
        return;
    }
    verifica_lavori();
}

static void disegna(HDC dc, RECT area)
{
    HGDIOBJ vecchia_penna = SelectObject(dc, GetStockObject(DC_PEN));
    HGDIOBJ vecchio_pennello = SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, RGB(33,34,38));
    SetDCPenColor(dc, RGB(67,68,73));
    RoundRect(dc, 0, 0, area.right, area.bottom, scala(12), scala(12));
    for (int i = 0; i < n_schede; i++) {
        const Scheda *s = &schede[i];
        for (int z = 0; z < numero_zone(s->schema); z++) {
            RECT r = s->zone[z];
            BOOL evidenza = i == scelta_scheda && (z == scelta_zona || s->proposta);
            SetDCBrushColor(dc, evidenza ? RGB(54,111,152) : RGB(67,68,70));
            SetDCPenColor(dc, evidenza ? RGB(147,209,250) : RGB(129,130,133));
            RoundRect(dc, r.left, r.top, r.right, r.bottom, scala(4), scala(4));
            if (s->proposta && s->app[z] >= 0) {
                int lato = scala(23);
                DrawIconEx(dc, (r.left+r.right-lato)/2, (r.top+r.bottom-lato)/2,
                           immagini[s->app[z]], lato, lato, 0, NULL, DI_NORMAL);
            }
        }
    }
    SelectObject(dc, vecchio_pennello);
    SelectObject(dc, vecchia_penna);
}

static void naviga(WPARAM tasto)
{
    int i = scelta_scheda < 0 ? 0 : scelta_scheda, z = scelta_zona < 0 ? 0 : scelta_zona;
    if (tasto == VK_TAB) {
        int passo = (GetKeyState(VK_SHIFT) < 0) ? -1 : 1;
        z += passo;
        if (z >= numero_zone(schede[i].schema)) { i = (i+1) % n_schede; z = 0; }
        if (z < 0) { i = (i+n_schede-1) % n_schede; z = numero_zone(schede[i].schema)-1; }
        segna(i,z);
        return;
    }
    RECT partenza = schede[i].zone[z];
    int x = (partenza.left+partenza.right)/2, y = (partenza.top+partenza.bottom)/2;
    int migliore = INT_MAX, mi = i, mz = z;
    for (int j = 0; j < n_schede; j++) {
        for (int k = 0; k < numero_zone(schede[j].schema); k++) {
            RECT r = schede[j].zone[k];
            int dx = (r.left+r.right)/2-x, dy = (r.top+r.bottom)/2-y;
            int avanti = tasto == VK_RIGHT ? dx : tasto == VK_LEFT ? -dx : tasto == VK_DOWN ? dy : -dy;
            int lato = (tasto == VK_LEFT || tasto == VK_RIGHT) ? abs(dy) : abs(dx);
            if (avanti <= 0 || lato > avanti) continue;
            int punteggio = avanti + 3*lato;
            if (punteggio < migliore) { migliore = punteggio; mi = j; mz = k; }
        }
    }
    segna(mi,mz);
}

static LRESULT CALLBACK procedura_pannello(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg) {
    case WM_MOUSEACTIVATE: return da_tastiera ? MA_ACTIVATE : MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        HDC memoria = CreateCompatibleDC(dc);
        HBITMAP bmp = memoria ? CreateCompatibleBitmap(dc, r.right, r.bottom) : NULL;
        if (bmp) {
            HGDIOBJ prima = SelectObject(memoria, bmp);
            disegna(memoria, r);
            BitBlt(dc, 0, 0, r.right, r.bottom, memoria, 0, 0, SRCCOPY);
            SelectObject(memoria, prima);
            DeleteObject(bmp);
        } else disegna(dc, r);
        if (memoria) DeleteDC(memoria);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP: {
        POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        selezione_mouse(p);
        if (msg == WM_LBUTTONUP) applica((w & MK_SHIFT) != 0);
        return 0;
    }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) chiudi(TRUE);
        else if (w == VK_RETURN || w == VK_SPACE) applica(GetKeyState(VK_SHIFT) < 0);
        else if (w == VK_TAB || (w >= VK_LEFT && w <= VK_DOWN)) naviga(w);
        return 0;
    case WM_ACTIVATE:
        if (aperto && da_tastiera && LOWORD(w) == WA_INACTIVE) chiudi(FALSE);
        return 0;
    case WM_CLOSE: chiudi(TRUE); return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

static void controlla_mouse(void)
{
    if (menu_tray || n_lavori) return;
    POINT p;
    if (!GetCursorPos(&p)) { chiudi(FALSE); osservata = NULL; punto_noto = FALSE; return; }
    ULONGLONG ora = GetTickCount64();
    if (aperto) {
        if (!ancora_valida(bersaglio) || !IsWindowVisible(bersaglio.hwnd) || IsIconic(bersaglio.hwnd)) {
            chiudi(FALSE);
            return;
        }
        if (da_tastiera) return;
        RECT posizione;
        if (GetForegroundWindow() != focus_apertura ||
            !GetWindowRect(bersaglio.hwnd, &posizione) || !EqualRect(&posizione, &posizione_apertura)) {
            chiudi(FALSE);
            return;
        }
        RECT r;
        GetWindowRect(pannello, &r);
        if (GetAsyncKeyState(VK_ESCAPE) < 0 ||
            (!PtInRect(&r, p) && (GetAsyncKeyState(VK_LBUTTON) < 0 ||
                                   GetAsyncKeyState(VK_RBUTTON) < 0))) { chiudi(FALSE); return; }
        if (PtInRect(&r, p) || PtInRect(&pulsante_apertura, p)) fuori_da = 0;
        else if (!fuori_da) fuori_da = ora;
        else if (ora - fuori_da >= 350) chiudi(FALSE);
        return;
    }
    HWND primo = GetForegroundWindow();
    if (primo != ultimo_primo) {
        ultimo_primo = primo;
        if (idonea(primo)) ultima = identifica(primo);
        punto_noto = FALSE;
    }
    if (GetAsyncKeyState(VK_LBUTTON) < 0 || GetAsyncKeyState(VK_RBUTTON) < 0) {
        osservata = NULL;
        punto_noto = FALSE;
        return;
    }
    BOOL fermo = punto_noto && p.x == ultimo_punto.x && p.y == ultimo_punto.y;
    if (fermo && (!osservata || ora - osservata_da < 400)) return;
    ultimo_punto = p;
    punto_noto = TRUE;
    if (attendi_uscita && PtInRect(&pulsante_apertura, p)) return;
    attendi_uscita = FALSE;
    HWND hwnd = GetAncestor(WindowFromPoint(p), GA_ROOT);
    RECT button;
    enum TipoPulsante tipo = idonea(hwnd) ? esamina_pulsante(hwnd, p, &button) : NESSUN_PULSANTE;
    if (tipo == NESSUN_PULSANTE) {
        osservata = NULL;
        return;
    }
    if (osservata != hwnd || !PtInRect(&pulsante_osservato, p)) {
        osservata = hwnd;
        pulsante_osservato = button;
        osservata_da = ora;
    } else if (ora - osservata_da >= 400) {
        if (tipo == DA_VERIFICARE && !verifica_pulsante_pronta(identifica(hwnd), p, osservata_da, &button))
            return;
        osservata = NULL;
        if (PtInRect(&button, p)) apri_pannello(hwnd, button, FALSE);
    }
}

static HWND destinazione(void)
{
    HWND hwnd = GetForegroundWindow();
    if (idonea(hwnd)) return hwnd;
    return ancora_valida(ultima) && idonea(ultima.hwnd) ? ultima.hwnd : NULL;
}

static void apri_con_tastiera(HWND hwnd)
{
    RECT vuoto = {0};
    if (!apri_pannello(hwnd, vuoto, TRUE) && !n_lavori)
        avviso(L"Select a resizable window and open the layouts again.");
}

static void comandi_tray(void)
{
    HWND prima = destinazione();
    chiudi(FALSE);
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING | (prima ? 0 : MF_GRAYED), APRI,
                tasto_attivo ? L"Open layouts\tCtrl+Alt+Z" : L"Open layouts");
    AppendMenuW(menu, MF_STRING | (avvio_attivo() ? MF_CHECKED : 0), AVVIO, L"Start with Windows");
    AppendMenuW(menu, MF_STRING, GUIDA, L"Quick guide");
    AppendMenuW(menu, MF_STRING, INFO_APP, L"About SnapTile");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ESCI, L"Exit");
    POINT p;
    GetCursorPos(&p);
    menu_tray = TRUE;
    SetForegroundWindow(servizio);
    UINT comando = TrackPopupMenuEx(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                    p.x, p.y, servizio, NULL);
    PostMessageW(servizio, WM_NULL, 0, 0);
    DestroyMenu(menu);
    menu_tray = FALSE;
    if (comando == APRI) apri_con_tastiera(prima);
    else if (comando == AVVIO) {
        LONG e = imposta_avvio(!avvio_attivo());
        if (e != ERROR_SUCCESS) {
            WCHAR motivo[256];
            if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, e, 0x0409, motivo, ARRAYSIZE(motivo), NULL))
                lstrcpyW(motivo, L"Could not update the startup setting.");
            MessageBoxW(NULL, motivo, L"SnapTile - Startup", MB_OK | MB_ICONWARNING);
        }
    } else if (comando == GUIDA) {
        MessageBoxW(NULL,
            L"Hover over Maximize or Restore, then click a zone.\n\n"
            L"Shift-click to fill the other zones with available windows. "
            L"With three or more eligible windows on this monitor, the top row shows two suggested groups with app icons.\n\n"
            L"Press Ctrl+Alt+Z to open layouts. Use the arrow keys or Tab to select a zone, Enter to apply, or Esc to close.",
            L"SnapTile", MB_OK | MB_ICONINFORMATION);
    } else if (comando == INFO_APP) {
        MessageBoxW(NULL, L"SnapTile " SNAPTILE_VERSIONE L"\n\nCopyright \u00A9 2026 Raffaele Mele",
                    L"About SnapTile", MB_OK | MB_ICONINFORMATION);
    } else if (comando == ESCI) DestroyWindow(servizio);
}

static LRESULT CALLBACK procedura_servizio(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg_taskbar && msg == msg_taskbar) { registra_icona(); return 0; }
    switch (msg) {
    case MSG_ICONA:
        if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) comandi_tray();
        else if (l == WM_LBUTTONUP) apri_con_tastiera(destinazione());
        return 0;
    case WM_HOTKEY:
        if (w == ID_SCORCIATOIA) {
            if (aperto && da_tastiera) chiudi(TRUE);
            else apri_con_tastiera(aperto ? bersaglio.hwnd : destinazione());
        }
        return 0;
    case WM_TIMER:
        if (w == TIMER_MOUSE) controlla_mouse();
        else if (w == TIMER_SPOSTA) verifica_lavori();
        else if (w == TIMER_ICONA) registra_icona();
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
    case WM_POWERBROADCAST:
        chiudi(FALSE);
        osservata = NULL;
        punto_noto = FALSE;
        return DefWindowProcW(hwnd, msg, w, l);
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION:
        if (w) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_MOUSE);
        KillTimer(hwnd, TIMER_SPOSTA);
        KillTimer(hwnd, TIMER_ICONA);
        if (tasto_attivo) UnregisterHotKey(hwnd, ID_SCORCIATOIA);
        if (icona_presente) Shell_NotifyIconW(NIM_DELETE, &icona);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

static int esegui(HINSTANCE istanza)
{
    HANDLE unica = CreateMutexW(NULL, FALSE, L"Local\\SnapTile.Istanza.1");
    if (!unica) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(unica); return 0; }
    HICON grafica = LoadIconW(istanza, MAKEINTRESOURCEW(101));
    WNDCLASSW classe = {.lpfnWndProc = procedura_servizio, .hInstance = istanza,
                        .hIcon = grafica, .hCursor = LoadCursorW(NULL, IDC_ARROW),
                        .lpszClassName = L"SnapTile.Servizio"};
    if (!RegisterClassW(&classe)) { CloseHandle(unica); return 1; }
    classe.lpfnWndProc = procedura_pannello;
    classe.lpszClassName = L"SnapTile.Pannello";
    if (!RegisterClassW(&classe)) { CloseHandle(unica); return 1; }
    servizio = CreateWindowExW(WS_EX_TOOLWINDOW, L"SnapTile.Servizio", L"SnapTile",
                               WS_POPUP, 0,0,0,0, NULL, NULL, istanza, NULL);
    pannello = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                               classe.lpszClassName, L"SnapTile - Layouts",
                               WS_POPUP, 0,0,0,0, servizio, NULL, istanza, NULL);
    if (!servizio || !pannello) { CloseHandle(unica); return 1; }
    msg_taskbar = RegisterWindowMessageW(L"TaskbarCreated");
    icona = (NOTIFYICONDATAW){.cbSize = sizeof(icona), .hWnd = servizio, .uID = ID_ICONA,
             .uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE, .uCallbackMessage = MSG_ICONA, .hIcon = grafica,
             .szTip = L"SnapTile " SNAPTILE_VERSIONE L" - Arrange windows"};
    registra_icona();
    tasto_attivo = RegisterHotKey(servizio, ID_SCORCIATOIA, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Z');
    if (!tasto_attivo) avviso(L"Ctrl+Alt+Z is already in use. Hover over Maximize or use the notification area icon.");
    if (!SetTimer(servizio, TIMER_MOUSE, 165, NULL)) {
        DestroyWindow(servizio);
        CloseHandle(unica);
        return 1;
    }
    MSG msg;
    int stato;
    while ((stato = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (IsWindow(servizio)) DestroyWindow(servizio);
    CloseHandle(unica);
    return stato < 0 ? 1 : 0;
}

/* Punto d'ingresso PE. Il programma usa l'inizializzazione del loader Win32
   e non richiede argv, costruttori o l'inizializzazione del runtime C. */
void avvia_affianca(void) __asm__("avvia_affianca");
void avvia_affianca(void)
{
    ExitProcess((UINT)esegui(GetModuleHandleW(NULL)));
}
