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

int numero_zone(enum Schema schema);
RECT rettangolo_zona(enum Schema schema, int zona, RECT area);
BOOL area_monitor(HMONITOR monitor, RECT *area);
int dpi_monitor(HMONITOR monitor);
BOOL idonea(HWND hwnd);
Finestra identifica(HWND hwnd);
BOOL ancora_valida(Finestra f);
BOOL pulsante_ingrandisci(HWND hwnd, POINT p, RECT *pulsante);
enum Esito avanza_spostamento(Spostamento *s, ULONGLONG ora);

#endif
