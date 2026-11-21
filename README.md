# SnapTile 1.1

[English](#english) · [Italiano](#italiano) · [Release v1.1.0](https://github.com/lellomele/SnapTile/releases/tag/v1.1.0)

## English
Portable x64 window tiling utility for Windows 10. Copyright © 2026 Raffaele Mele.

Run SnapTile.exe. Hover over the Maximize or Restore button and click a zone. Shift-click to fill the remaining zones with other windows on the same monitor. With three or more eligible windows, the top row shows suggested groups.

Press Ctrl+Alt+Z to open layouts from the keyboard. Use Tab or the arrows, Enter to apply and Esc to dismiss. Right-click the notification area icon for startup settings, the quick guide, copyright information and Exit.

When upgrading from Affianca10, turn off its startup option if enabled and exit the old program before launching SnapTile. Enable Start with Windows in SnapTile if desired.

Run build.cmd to compile with MinGW-w64. Run build.cmd prova to include regression tests. The build checks MINGW_HOME, C:\mingw, C:\mingw64 and PATH.

## Italiano

SnapTile è una utility portatile x64 per affiancare le finestre su Windows 10. Scarica il pacchetto dalla [release v1.1.0](https://github.com/lellomele/SnapTile/releases/tag/v1.1.0), estrailo e avvia **SnapTile.exe**.

Ferma il puntatore sul pulsante Ingrandisci/Ripristina e fai clic su una zona. Con **Maiusc + clic** riempi anche le altre zone con le finestre aperte sullo stesso monitor. Con almeno tre finestre idonee compaiono due combinazioni suggerite.

Premi **Ctrl + Alt + Z** per aprire gli schemi da tastiera. Usa Tab o le frecce per scegliere, Invio per applicare ed Esc per chiudere. Dal menu dell’icona nell’area di notifica puoi selezionare **Start with Windows**, consultare la guida rapida e il copyright o scegliere **Exit**.

Se usavi Affianca10, disattiva il suo avvio automatico e chiudi la vecchia versione prima di avviare SnapTile.

Per compilare con MinGW-w64 esegui `build.cmd`. Per includere i test esegui `build.cmd prova`. Lo script cerca il compilatore in MINGW_HOME, C:\mingw, C:\mingw64 e nel PATH.