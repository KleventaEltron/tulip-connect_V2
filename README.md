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

## Heatpump Setpoint Control
Voor het instellen van het setpoint in de warmtepomp is de volgende functie in ```files/states.c``` Aanwezig. 

```c
void setActiveModeControllerHeatpumpSetpoint(int16_t newSetpoint) {
    app_active_mode_controllerData.setPoint = newSetpoint;
}
```

Het correcte setpoint wordt elke 10 seconden geverifieerd in de hoofdloop van het programma met de functie ```checkHeatpumpSetpoint()```. Indien deze niet overeen komt wordt de nieuwe instelling weggeschreven. 10 Seconden later wordt deze weer geverifieerd.

```c
 void checkHeatpumpSetpoint() {
    if (getWriteNewSetPointHeatpumpCounter() < 10) {
        return;
    }
    
    if (app_active_mode_controllerData.setPoint != (UserParameters[ADDRESS_HEATING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] * 10)) {   
        // Setpoint in heatpump is not correct, send the correct one
        ChangeHeatpumpSetting(ADDRESS_HEATING_SET_TEMPERATURE, (app_active_mode_controllerData.setPoint / 10));
    }
    getWriteNewSetPointHeatpumpCounter(0); 
 }
```
