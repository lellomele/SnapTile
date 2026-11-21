#define COBJMACROS
#include "affianca.h"
#include <uiautomationclient.h>
#include <oleacc.h>

/* Il timer e le scorciatoie restano sul thread delle finestre. Solo una
   richiesta alla volta interroga il provider UI Automation dell'altra app. */
enum { LIBERA, IN_LETTURA, PRONTA };
/* Si combinano solo gli indizi ottenuti nel punto interrogato. */
enum { VISTA_FINESTRA = 1, VISTO_CONTROLLO = 2 };
static volatile LONG stato;
static struct {
    Finestra finestra;
    POINT punto;
    ULONGLONG sosta;
    RECT finestra_prima, pulsante;
} richiesta;

static BOOL nome_pulsante(HWND hwnd, const WCHAR *nome)
{
    if (!nome || !*nome) return FALSE;
    /* Un pulsante della toolbar puo' chiamarsi "Ripristina" anche in una
       finestra normale. Per un nome senza ID verifica il comando atteso. */
    static const WCHAR *const ingrandisci[] = {L"Maximize", L"Maximize Window", L"Ingrandisci", L"Massimizza"};
    static const WCHAR *const ripristina[] = {L"Restore", L"Restore Down", L"Restore Window", L"Ripristina"};
    BOOL ingrandita = IsZoomed(hwnd);
    const WCHAR *const *nomi = ingrandita ? ripristina : ingrandisci;
    unsigned n = ingrandita ? ARRAYSIZE(ripristina) : ARRAYSIZE(ingrandisci);
    for (unsigned i=0; i<n; ++i)
        if (!lstrcmpiW(nome, nomi[i])) return TRUE;
    /* Anche la lingua di Windows, indipendentemente dalla lingua di SnapTile. */
    HMENU menu = GetSystemMenu(hwnd, FALSE);
    WCHAR voce[128];
    if (!menu || !GetMenuStringW(menu, ingrandita ? SC_RESTORE : SC_MAXIMIZE, voce, ARRAYSIZE(voce), MF_BYCOMMAND))
        return FALSE;
    WCHAR *dest = voce;
    for (const WCHAR *s=voce; *s && *s!=L'\t'; ++s)
        if (*s!=L'&') *dest++ = *s;
    *dest = 0;
    return !lstrcmpiW(nome, voce);
}

static BOOL area_del_pulsante(RECT r)
{
    RECT comune;
    int dpi = dpi_monitor(MonitorFromWindow(richiesta.finestra.hwnd, MONITOR_DEFAULTTONEAREST));
    if (r.right-r.left > MulDiv(120, dpi, 96) || r.bottom-richiesta.finestra_prima.top > MulDiv(76, dpi, 96))
        return FALSE;
    return PtInRect(&r, richiesta.punto) && IntersectRect(&comune, &r, &richiesta.finestra_prima) &&
           EqualRect(&r, &comune);
}

static BOOL copre_finestra(RECT r)
{
    RECT interno = richiesta.finestra_prima, comune;
    /* Win32 e i provider possono includere bordi e ombre diversi. */
    InflateRect(&interno, -(interno.right - interno.left) / 10, -(interno.bottom - interno.top) / 10);
    return PtInRect(&r, richiesta.punto) && IntersectRect(&comune, &r, &interno) && EqualRect(&comune, &interno);
}

static void verifica_elemento(IUIAutomationElement *elemento)
{
    BOOL attivo = FALSE, nascosto = TRUE;
    RECT r;
    if (FAILED(IUIAutomationElement_get_CachedIsEnabled(elemento, &attivo)) || !attivo ||
        FAILED(IUIAutomationElement_get_CachedIsOffscreen(elemento, &nascosto)) || nascosto ||
        FAILED(IUIAutomationElement_get_CachedBoundingRectangle(elemento, &r)) ||
        !area_del_pulsante(r)) return;

    BSTR classe = NULL, id = NULL, nome = NULL;
    IUIAutomationElement_get_CachedClassName(elemento, &classe);
    IUIAutomationElement_get_CachedAutomationId(elemento, &id);
    IUIAutomationElement_get_CachedName(elemento, &nome);
    BOOL valido = (classe && (!lstrcmpW(classe, L"window-maximize") || !lstrcmpW(classe, L"window-restore"))) ||
                  (id && (!lstrcmpiW(id, L"Maximize") || !lstrcmpiW(id, L"MaximizeButton") ||
                          !lstrcmpiW(id, L"Restore") || !lstrcmpiW(id, L"RestoreButton"))) ||
                  nome_pulsante(richiesta.finestra.hwnd, nome);
    SysFreeString(classe);
    SysFreeString(id);
    SysFreeString(nome);
    if (valido) richiesta.pulsante = r;
}

static void cerca_pulsante(IUIAutomation *uia, unsigned *indizi)
{
    IUIAutomationCacheRequest *cache = NULL;
    IUIAutomationTreeWalker *albero = NULL;
    IUIAutomationElement *elemento = NULL;
    if (FAILED(IUIAutomation_CreateCacheRequest(uia, &cache))) return;
    const PROPERTYID proprieta[] = { UIA_ProcessIdPropertyId, UIA_ControlTypePropertyId,
        UIA_IsEnabledPropertyId, UIA_IsOffscreenPropertyId, UIA_BoundingRectanglePropertyId,
        UIA_ClassNamePropertyId, UIA_AutomationIdPropertyId, UIA_NamePropertyId };
    for (unsigned i=0; i<ARRAYSIZE(proprieta); ++i)
        if (FAILED(IUIAutomationCacheRequest_AddProperty(cache, proprieta[i]))) goto fine;
    if (FAILED(IUIAutomation_ElementFromPointBuildCache(uia, richiesta.punto, cache, &elemento))) goto fine;

    /* Un glifo SVG puo' essere il figlio del vero pulsante. Ferma la risalita
       al primo pulsante, anche se e' Chiudi o Riduci a icona. */
    for (int livello=0; elemento && livello<4; ++livello) {
        int pid = 0;
        CONTROLTYPEID tipo = 0;
        if (FAILED(IUIAutomationElement_get_CachedProcessId(elemento, &pid)) ||
            FAILED(IUIAutomationElement_get_CachedControlType(elemento, &tipo))) break;
        if ((DWORD)pid != richiesta.finestra.processo) { *indizi |= VISTO_CONTROLLO; break; }
        if (livello == 0) {
            RECT r = {0};
            BOOL attivo = FALSE, nascosto = TRUE;
            if (tipo == UIA_WindowControlTypeId &&
                SUCCEEDED(IUIAutomationElement_get_CachedIsEnabled(elemento, &attivo)) && attivo &&
                SUCCEEDED(IUIAutomationElement_get_CachedIsOffscreen(elemento, &nascosto)) && !nascosto &&
                SUCCEEDED(IUIAutomationElement_get_CachedBoundingRectangle(elemento, &r)) && copre_finestra(r))
                *indizi |= VISTA_FINESTRA;
            else
                *indizi |= VISTO_CONTROLLO;
        }
        if (tipo == UIA_ButtonControlTypeId) {
            verifica_elemento(elemento);
            break;
        }
        if (tipo==UIA_WindowControlTypeId || tipo==UIA_DocumentControlTypeId) break;
        if (!albero && FAILED(IUIAutomation_get_ControlViewWalker(uia, &albero))) break;
        IUIAutomationElement *padre = NULL;
        HRESULT hr = IUIAutomationTreeWalker_GetParentElementBuildCache(albero, elemento, cache, &padre);
        IUIAutomationElement_Release(elemento);
        elemento = padre;
        if (FAILED(hr)) break;
    }
fine:
    if (elemento) IUIAutomationElement_Release(elemento);
    if (albero) IUIAutomationTreeWalker_Release(albero);
    IUIAutomationCacheRequest_Release(cache);
}

static BOOL attivo_msaa(IAccessible *acc, VARIANT figlio)
{
    VARIANT condizioni;
    VariantInit(&condizioni);
    BOOL attivo = SUCCEEDED(IAccessible_get_accState(acc, figlio, &condizioni)) &&
        V_VT(&condizioni) == VT_I4 &&
        !(V_I4(&condizioni) & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN | STATE_SYSTEM_UNAVAILABLE));
    VariantClear(&condizioni);
    return attivo;
}

static BOOL rettangolo_msaa(IAccessible *acc, VARIANT figlio, RECT *r)
{
    LONG x, y, w, h;
    if (IAccessible_accLocation(acc, &x, &y, &w, &h, figlio) != S_OK || w <= 0 || h <= 0) return FALSE;
    return SetRect(r, x, y, x + w, y + h);
}

static void verifica_msaa(IAccessible *acc, VARIANT figlio, BOOL puntato, unsigned *indizi)
{
    VARIANT ruolo;
    VariantInit(&ruolo);
    LONG tipo = 0;
    if (SUCCEEDED(IAccessible_get_accRole(acc, figlio, &ruolo))) {
        if (V_VT(&ruolo) == VT_I4) tipo = V_I4(&ruolo);
        else if (puntato && V_VT(&ruolo) == VT_BSTR && V_BSTR(&ruolo) && *V_BSTR(&ruolo))
            *indizi |= VISTO_CONTROLLO;
    }
    VariantClear(&ruolo);
    RECT r;
    if (tipo == ROLE_SYSTEM_PUSHBUTTON) {
        *indizi |= VISTO_CONTROLLO;
        if (!attivo_msaa(acc, figlio) || !rettangolo_msaa(acc, figlio, &r) || !area_del_pulsante(r)) return;
        BSTR nome = NULL;
        if (SUCCEEDED(IAccessible_get_accName(acc, figlio, &nome)) && nome_pulsante(richiesta.finestra.hwnd, nome))
            richiesta.pulsante = r;
        SysFreeString(nome);
    } else if (tipo && puntato) {
        if ((tipo == ROLE_SYSTEM_WINDOW || tipo == ROLE_SYSTEM_CLIENT || tipo == ROLE_SYSTEM_PANE) &&
            rettangolo_msaa(acc, figlio, &r) && copre_finestra(r)) {
            /* Il contenitore client puo' avvolgere l'intera finestra. Da solo
               non identifica la finestra e neppure un controllo distinto. */
            if (tipo == ROLE_SYSTEM_WINDOW)
                *indizi |= attivo_msaa(acc, figlio) ? VISTA_FINESTRA : VISTO_CONTROLLO;
        } else {
            *indizi |= VISTO_CONTROLLO;
        }
    }
}

/* Interroga il controllo sotto il mouse: una barra disegnata dall'app puo'
   esporre i suoi pulsanti come figli MSAA di un unico HWND. */
static void cerca_msaa(unsigned *indizi)
{
    POINT p = richiesta.punto;
    HWND sotto = WindowFromPoint(p);
    if (GetAncestor(sotto, GA_ROOT) != richiesta.finestra.hwnd) return;
    IAccessible *acc = NULL;
    if (SUCCEEDED(AccessibleObjectFromWindow(sotto, OBJID_CLIENT, &IID_IAccessible, (void **)&acc)) && acc) {
        VARIANT se_stesso;
        VariantInit(&se_stesso);
        V_VT(&se_stesso) = VT_I4;
        V_I4(&se_stesso) = CHILDID_SELF;
        for (int livello=0; livello<6; ++livello) {
            verifica_msaa(acc, se_stesso, FALSE, indizi);
            if (!IsRectEmpty(&richiesta.pulsante)) break;
            VARIANT figlio;
            VariantInit(&figlio);
            HRESULT hr = IAccessible_accHitTest(acc, p.x, p.y, &figlio);
            if (hr == S_OK && V_VT(&figlio) == VT_I4) verifica_msaa(acc, figlio, TRUE, indizi);
            IAccessible *interno = NULL;
            if (hr == S_OK && V_VT(&figlio)==VT_DISPATCH && V_DISPATCH(&figlio))
                IDispatch_QueryInterface(V_DISPATCH(&figlio), &IID_IAccessible, (void **)&interno);
            VariantClear(&figlio);
            if (!interno) break;
            IAccessible_Release(acc);
            acc = interno;
        }
        IAccessible_Release(acc);
    }
    if (!IsRectEmpty(&richiesta.pulsante)) return;
    VARIANT figlio;
    VariantInit(&figlio);
    acc = NULL;
    if (SUCCEEDED(AccessibleObjectFromPoint(p, &acc, &figlio)) && acc) {
        HWND hwnd = NULL;
        if (SUCCEEDED(WindowFromAccessibleObject(acc, &hwnd)) && GetAncestor(hwnd, GA_ROOT)==richiesta.finestra.hwnd)
            verifica_msaa(acc, figlio, TRUE, indizi);
        IAccessible_Release(acc);
    }
    VariantClear(&figlio);
}

static void CALLBACK leggi_pulsante(PTP_CALLBACK_INSTANCE istanza, void *param)
{
    (void)istanza;
    (void)param;
    if (ancora_valida(richiesta.finestra) &&
        GetAncestor(WindowFromPoint(richiesta.punto), GA_ROOT) == richiesta.finestra.hwnd &&
        SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        unsigned indizi = 0;
        IUIAutomation *uia = NULL;
        HRESULT hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
        if (FAILED(hr))
            hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
        if (SUCCEEDED(hr)) {
            IUIAutomation2 *uia2 = NULL;
            if (SUCCEEDED(IUIAutomation_QueryInterface(uia, &IID_IUIAutomation2, (void **)&uia2))) {
                IUIAutomation2_put_ConnectionTimeout(uia2, 1000);
                IUIAutomation2_put_TransactionTimeout(uia2, 1000);
                IUIAutomation2_Release(uia2);
            }
            cerca_pulsante(uia, &indizi);
            IUIAutomation_Release(uia);
        }
        if (IsRectEmpty(&richiesta.pulsante)) cerca_msaa(&indizi);
        if (IsRectEmpty(&richiesta.pulsante) && indizi == VISTA_FINESTRA)
            stima_pulsante_dwm(richiesta.finestra.hwnd, richiesta.finestra_prima, richiesta.punto, &richiesta.pulsante);
        CoUninitialize();
    }
    InterlockedExchange(&stato, PRONTA);
}

BOOL verifica_pulsante_pronta(Finestra f, POINT p, ULONGLONG sosta, RECT *pulsante)
{
    LONG attuale = InterlockedCompareExchange(&stato, LIBERA, LIBERA);
    if (attuale == IN_LETTURA) return FALSE;
    SetRectEmpty(pulsante);
    RECT posizione;
    if (!GetWindowRect(f.hwnd, &posizione)) return TRUE;
    if (attuale == PRONTA && richiesta.sosta == sosta && richiesta.finestra.hwnd == f.hwnd &&
        richiesta.finestra.processo == f.processo && richiesta.finestra.thread == f.thread &&
        EqualRect(&posizione, &richiesta.finestra_prima) &&
        ((richiesta.punto.x==p.x && richiesta.punto.y==p.y) || PtInRect(&richiesta.pulsante, p))) {
        if (PtInRect(&richiesta.pulsante, p)) *pulsante = richiesta.pulsante;
        InterlockedExchange(&stato, LIBERA);
        return TRUE;
    }
    richiesta.finestra = f;
    richiesta.punto = p;
    richiesta.sosta = sosta;
    richiesta.finestra_prima = posizione;
    SetRectEmpty(&richiesta.pulsante);
    InterlockedExchange(&stato, IN_LETTURA);
    if (TrySubmitThreadpoolCallback(leggi_pulsante, NULL, NULL)) return FALSE;
    InterlockedExchange(&stato, LIBERA);
    return TRUE;
}
