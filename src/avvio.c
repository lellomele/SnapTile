#include "avvio.h"

static const WCHAR chiave[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const WCHAR nome[] = L"SnapTile";

static LONG comando_corrente(WCHAR *comando)
{
    WCHAR percorso[MAX_PATH + 1];
    DWORD n = GetModuleFileNameW(NULL, percorso, ARRAYSIZE(percorso));
    if (!n) return GetLastError();
    if (n + 2 > MAX_PATH) return ERROR_FILENAME_EXCED_RANGE;
    comando[0] = L'"';
    CopyMemory(comando + 1, percorso, n * sizeof(WCHAR));
    comando[n + 1] = L'"';
    comando[n + 2] = 0;
    return ERROR_SUCCESS;
}

BOOL avvio_attivo(void)
{
    WCHAR corrente[MAX_PATH + 1], salvato[MAX_PATH + 1];
    DWORD bytes = sizeof(salvato);
    if (comando_corrente(corrente) != ERROR_SUCCESS) return FALSE;
    LONG e = RegGetValueW(HKEY_CURRENT_USER, chiave, nome, RRF_RT_REG_SZ,
                          NULL, salvato, &bytes);
    return e == ERROR_SUCCESS && lstrcmpiW(corrente, salvato) == 0;
}

LONG imposta_avvio(BOOL attivo)
{
    if (!attivo) {
        LONG e = RegDeleteKeyValueW(HKEY_CURRENT_USER, chiave, nome);
        return e == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : e;
    }
    WCHAR comando[MAX_PATH + 1];
    LONG e = comando_corrente(comando);
    if (e != ERROR_SUCCESS) return e;
    return RegSetKeyValueW(HKEY_CURRENT_USER, chiave, nome, REG_SZ, comando,
                           (lstrlenW(comando) + 1) * sizeof(WCHAR));
}
