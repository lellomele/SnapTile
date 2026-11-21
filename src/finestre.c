#include "affianca.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <oleacc.h>

BOOL area_monitor(HMONITOR monitor, RECT *area)
{
    MONITORINFO mi = {.cbSize = sizeof(mi)};
    if (!monitor || !GetMonitorInfoW(monitor, &mi)) return FALSE;
    *area = mi.rcWork;
    return TRUE;
}

int dpi_monitor(HMONITOR monitor)
{
    UINT x = 96, y = 96;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &x, &y))) return 96;
    return (int)x;
}

BOOL idonea(HWND hwnd)
{
    DWORD cloaked = 0;
    if (!hwnd || !IsWindowVisible(hwnd) || IsIconic(hwnd) ||
        GetAncestor(hwnd, GA_ROOT) != hwnd) return FALSE;
    LONG_PTR stile = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR esteso = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!(stile & WS_THICKFRAME) || (stile & WS_CHILD) ||
        (esteso & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))) return FALSE;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return !cloaked && !IsHungAppWindow(hwnd);
}

Finestra identifica(HWND hwnd)
{
    Finestra f = {.hwnd = hwnd};
    f.thread = GetWindowThreadProcessId(hwnd, &f.processo);
    return f;
}

BOOL ancora_valida(Finestra f)
{
    DWORD pid = 0;
    return f.hwnd && f.thread &&
           GetWindowThreadProcessId(f.hwnd, &pid) == f.thread && pid == f.processo;
}

BOOL pulsante_ingrandisci(HWND hwnd, POINT p, RECT *pulsante)
{
    RECT r;
    if (!(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_MAXIMIZEBOX) ||
        !GetWindowRect(hwnd, &r) || p.y < r.top || p.y > r.top + 200)
        return FALSE;
    int dpi = dpi_monitor(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
    if (p.y > r.top + MulDiv(76, dpi, 96)) return FALSE;

    DWORD_PTR hit = 0;
    UINT flags = SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT;
    if (!SendMessageTimeoutW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(p.x, p.y),
                             flags, 35, &hit)) return FALSE;
    TITLEBARINFOEX info = {.cbSize = sizeof(info)};
    if (SendMessageTimeoutW(hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&info,
                            flags, 35, NULL) &&
        !(info.rgstate[3] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_UNAVAILABLE |
                             STATE_SYSTEM_OFFSCREEN)) &&
        PtInRect(&info.rgrect[3], p) &&
        (hit == HTMAXBUTTON || hit == HTCAPTION || hit == HTCLIENT)) {
        *pulsante = info.rgrect[3];
        return TRUE;
    }
    if (hit != HTMAXBUTTON) return FALSE;
    int dx = MulDiv(16, dpi, 96), dy = MulDiv(12, dpi, 96);
    SetRect(pulsante, p.x - dx, p.y - dy, p.x + dx, p.y + dy);
    return TRUE;
}

/* DWM esclude i bordi invisibili. SetWindowPos li include. Dopo un ripristino
   l'animazione DWM puo' restituire per un istante il vecchio rettangolo. */
static LONG margine(LONG n)
{
    return n >= 0 && n <= 48 ? n : 0;
}

static BOOL rettangolo_richiesto(HWND hwnd, RECT zona, RECT *richiesta)
{
    RECT esterno, visibile;
    if (!GetWindowRect(hwnd, &esterno)) return FALSE;
    *richiesta = zona;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                        &visibile, sizeof(visibile)))) {
        richiesta->left -= margine(visibile.left - esterno.left);
        richiesta->top -= margine(visibile.top - esterno.top);
        richiesta->right += margine(esterno.right - visibile.right);
        richiesta->bottom += margine(esterno.bottom - visibile.bottom);
    }
    return TRUE;
}

enum Esito avanza_spostamento(Spostamento *s, ULONGLONG ora)
{
    HWND hwnd = s->finestra.hwnd;
    if (!ancora_valida(s->finestra) || !idonea(hwnd)) return FALLITA;
    if (IsZoomed(hwnd)) {
        if (ora >= s->scadenza) return FALLITA;
        if (!s->ripristino) {
            s->ripristino = TRUE;
            if (!ShowWindowAsync(hwnd, SW_RESTORE)) return FALLITA;
            s->verifica = ora + 90;
        }
        return IN_CORSO;
    }
    if (ora < s->verifica) return IN_CORSO;
    RECT attuale;
    if (!GetWindowRect(hwnd, &attuale)) return FALLITA;
    if (!s->inviata) {
        if (!rettangolo_richiesto(hwnd, s->zona, &s->richiesta)) return FALLITA;
        RECT r = s->richiesta;
        if (!SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right-r.left, r.bottom-r.top,
                          SWP_NOACTIVATE | SWP_NOOWNERZORDER |
                          SWP_ASYNCWINDOWPOS)) return FALLITA;
        s->inviata = TRUE;
        s->verifica = ora + 65;
        return IN_CORSO;
    }
    if (EqualRect(&attuale, &s->richiesta)) return SISTEMATA;
    return ora >= s->scadenza ? FALLITA : IN_CORSO;
}
