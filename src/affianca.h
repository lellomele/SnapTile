#ifndef AFFIANCA_H
#define AFFIANCA_H

#include <windows.h>

enum Schema { META, ASIMMETRICO, TRE_ZONE, QUATTRO_ZONE, NUM_SCHEMI };
enum { MAX_ZONE = 4 };

typedef struct {
    HWND hwnd;
    DWORD processo;
    DWORD thread;
} Finestra;

typedef struct {
    Finestra finestra;
    RECT zona;
    RECT richiesta;
    ULONGLONG scadenza;
    ULONGLONG verifica;
    BOOL ripristino, inviata;
} Spostamento;

enum Esito { IN_CORSO, SISTEMATA, FALLITA };
enum TipoPulsante { NESSUN_PULSANTE, PULSANTE_NATIVO, DA_VERIFICARE };

int numero_zone(enum Schema schema);
RECT rettangolo_zona(enum Schema schema, int zona, RECT area);
RECT centro_pulsanti_dwm(RECT finestra, RECT gruppo, int dpi, BOOL rtl);
BOOL area_monitor(HMONITOR monitor, RECT *area);
int dpi_monitor(HMONITOR monitor);
BOOL idonea(HWND hwnd);
Finestra identifica(HWND hwnd);
BOOL ancora_valida(Finestra f);
enum TipoPulsante esamina_pulsante(HWND hwnd, POINT p, RECT *pulsante);
void stima_pulsante_dwm(HWND hwnd, RECT finestra, POINT p, RECT *pulsante);
BOOL verifica_pulsante_pronta(Finestra finestra, POINT p, ULONGLONG osservata_da, RECT *pulsante);
enum Esito avanza_spostamento(Spostamento *s, ULONGLONG ora);

#endif
