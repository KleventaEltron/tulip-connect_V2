# TULIP CONNECT V2
dit repository bevat de gerefactorde code van de Tulip Connect Firmware vanaf versie ```1-1-6.bin```. Alle verdere releases zullen vanuit dit repository dan ook vanaf versie ```2-X-X.bin``` zijn.

## _Let op!_
Na het clonen van het repository zal het ```credentials.h``` bestand niet herkend worden, omdat deze niet wordt meegenomen door de ```gitignore```. Dit bestand bevat de nodige Namen en Tokens om te kunnen loggen, updates op te kunnen halen, etc. Zonder dit bestand zullen veel functies uit de Tulip Connect software dus ook niet werken. Volg de volgende stappen om dit bestand correct toe te voegen:
- Verwijder het bestaande ```credentials.h``` bestand onder ```'Header files/files'``` in ```MPLAB``` of in de project map onder ```'src/files'```.
- Voeg het bestand opnieuw toe op dezelfde plek.
- Voeg de volgende info toe:

```c
#ifndef _CREDENTIALS_H
#define _CREDENTIALS_H

/* UNCOMMENT VOOR LOGGING NAAR PRODUCTIE */
#define HOST        "Productie endpoint hier"
#define TOKEN       "Productie token hier" 

/* TEST PARAMETERS */
//#define HOST      "Test endpoint hier"
//#define TOKEN     "Test token hier"        

#endif
```
✨Na dit gedaan te hebben zou het project succesvol moeten kunnen builden en runnen!✨
