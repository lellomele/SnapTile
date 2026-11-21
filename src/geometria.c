#include "affianca.h"

/* I bordi condivisi usano la stessa frazione, anche con dimensioni dispari. */
static const struct {
    int numero;
    BYTE frazioni[MAX_ZONE][4];
} modelli[NUM_SCHEMI] = {
    {2, {{0,0,5,10}, {5,0,10,10}}},
    {2, {{0,0,6,10}, {6,0,10,10}}},
    {3, {{0,0,5,10}, {5,0,10,5}, {5,5,10,10}}},
    {4, {{0,0,5,5}, {5,0,10,5}, {0,5,5,10}, {5,5,10,10}}}
};

int numero_zone(enum Schema schema)
{
    return modelli[schema].numero;
}

RECT rettangolo_zona(enum Schema schema, int zona, RECT area)
{
    const BYTE *f = modelli[schema].frazioni[zona];
    int w = area.right - area.left, h = area.bottom - area.top;
    RECT r = {area.left + MulDiv(w, f[0], 10), area.top + MulDiv(h, f[1], 10),
              area.left + MulDiv(w, f[2], 10), area.top + MulDiv(h, f[3], 10)};
    return r;
}

/* DWM restituisce un unico rettangolo in pixel relativi alla finestra.
   Con tre comandi standard usiamo solo il quarto centrale, lontano dai
   confini stimati con Riduci e Chiudi. */
RECT centro_pulsanti_dwm(RECT finestra, RECT gruppo, int dpi, BOOL rtl)
{
    RECT vuoto = {0};
    LONG w = finestra.right - finestra.left, h = finestra.bottom - finestra.top;
    if (dpi <= 0 || IsRectEmpty(&gruppo) || gruppo.left < 0 || gruppo.top < 0 ||
        gruppo.right > w || gruppo.bottom > h) return vuoto;
    LONG larghezza = gruppo.right - gruppo.left, altezza = gruppo.bottom - gruppo.top;
    LONG bordo = rtl ? gruppo.left : w - gruppo.right;
    if (bordo > MulDiv(24, dpi, 96) || gruppo.top > MulDiv(16, dpi, 96) ||
        larghezza < MulDiv(72, dpi, 96) || larghezza > MulDiv(192, dpi, 96) ||
        altezza < MulDiv(14, dpi, 96) || altezza > MulDiv(52, dpi, 96) ||
        larghezza < 2 * altezza) return vuoto;

    LONG margine = (3 * larghezza + 7) / 8;
    InflateRect(&gruppo, -margine, -MulDiv(2, dpi, 96));
    OffsetRect(&gruppo, finestra.left, finestra.top);
    return gruppo;
}
