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
