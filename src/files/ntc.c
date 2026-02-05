/* TODO:  Include other files here if needed. */
#include <xc.h>
#include <string.h>
#include <stdbool.h>                    // Defines true
#include "definitions.h"
#include "ntc.h"

static int16_t ntc_10k_tabel[2][NTC_TABLE_SIZE] =
{
  {3976, 3932, 3876, 3800, 3712, 3600, 3468, 3312, 3132, 2936, 2724, 2504, 2276, 2048, 1824, 1620, 1424, 1244, 1088, 944, 816, 704, 612, 528, 456, 396, 344, 300,  260,    0},
  {-400, -350, -300, -250, -200, -150, -100,  -50,    0,   50,  100,  150,  200,  250,  300,  350,  400,  450,  500, 550, 600, 650, 700, 750, 800, 850, 900, 950, 1000, 2000}
//  [0],  [1],  [2],  [3],  [4],  [5],  [6],  [7],  [8],  [9], [10], [11], [12], [13], [14], [15], [16], [17], [18],[19],[20],[21],[22],[23],[24],[25],[26],[27], [28], [29]
};

static uint8_t ntc_verschil[NTC_TABLE_SIZE] = 
 { 36, 44, 56, 76, 88, 112, 132, 156, 180, 196, 212, 220, 228, 228, 224, 204, 196, 180, 156, 128, 128, 112,  92,  84,  72,  60,  52,  44,  40, 255};
//[0],[1],[2],[3],[4], [5], [6], [7], [8], [9],[10],[11],[12],[13],[14],[15],[16],[17],[18],[19],[20],[21],[22],[23],[24],[25],[26],[27],[28],[29]

uint32_t  adc_seq_regs[4] = {0x180A, 0x180B, 0x1804, 0x1805};
// NTC1 ADC1_AIN10  0x180A -> Reg.INPUTCTRL
// NTC2 ADC1_AIN11  0x180B
// NTC3 ADC1_AIN4   0x1804
// NTC4 ADC1_AIN5   0x1805

volatile uint16_t     adc_res[4]      = {0};
volatile bool         adc_dma_done    = 0;
float input_voltage;

int16_t TempNtc[4] = {TEMPERATURE_ALARM_VALUE, TEMPERATURE_ALARM_VALUE, TEMPERATURE_ALARM_VALUE, TEMPERATURE_ALARM_VALUE};


void adc_sram_dma_callback(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
	if (event == DMAC_TRANSFER_EVENT_COMPLETE)
    {
        adc_dma_done = true;
    }
}


int16_t Temp_ntc_10k (int16_t AdcWaarde, int8_t offset)
{
    uint16_t rest = 0;
    uint8_t wijzer = 0;
     
    for (; AdcWaarde < ntc_10k_tabel[0][wijzer]; wijzer++);                       // vergelijken met groffe waarde
   
    rest = AdcWaarde - ntc_10k_tabel[0][wijzer];                                  // rest waarde berekenen
    
    AdcWaarde = ntc_10k_tabel[1][wijzer] - ((50 * rest) / ntc_verschil[wijzer]);      // temperatuur berekenen
    
    AdcWaarde += offset;
    
    if (AdcWaarde > 999) {                                    // maximale begrensen
        AdcWaarde = TEMPERATURE_ALARM_VALUE;  // error waarde
    }
    
    if (AdcWaarde < -400){                                                  // minimaal begrensen
        AdcWaarde = TEMPERATURE_ALARM_VALUE;  // error waarde
    }
    
    return(AdcWaarde);                                                        // teperatuur return
}

void AdcVerwerk(void)
{
    /*
    printf("\r\n ADC conversion of 4 inputs done \r\n");
    input_voltage = (float)adc_res[0] * ADC_VREF / 4095U;
    printf("\r\n NTC1: ADC Count: %04d, ADC Input Voltage = %d.%02d V \r\n\r\n", 
        adc_res[0], (int)input_voltage, (int)((input_voltage - (int)input_voltage)*100.0));
    input_voltage = (float)adc_res[1] * ADC_VREF / 4095U;
    printf("\r\n NTC2: ADC Count: %04d, ADC Input Voltage = %d.%02d V \r\n\r\n", 
        adc_res[1], (int)input_voltage, (int)((input_voltage - (int)input_voltage)*100.0));
    input_voltage = (float)adc_res[2] * ADC_VREF / 4095U;
    printf("\r\n NTC3: ADC Count: %04d, ADC Input Voltage = %d.%02d V \r\n\r\n", 
        adc_res[2], (int)input_voltage, (int)((input_voltage - (int)input_voltage)*100.0));
    input_voltage = (float)adc_res[3] * ADC_VREF / 4095U;
    printf("\r\n NTC4: ADC Count: %04d, ADC Input Voltage = %d.%02d V \r\n\r\n", 
        adc_res[3], (int)input_voltage, (int)((input_voltage - (int)input_voltage)*100.0));

    printf("\r\n ADC conversion of 4 inputs done \r\n");
    */
    
    TempNtc[NTC_HEATING_BUFFER]     = Temp_ntc_10k(adc_res[NTC_HEATING_BUFFER]      , 0);
    TempNtc[NTC_HOT_WATER_BUFFER]   = Temp_ntc_10k(adc_res[NTC_HOT_WATER_BUFFER]    , 0);
    TempNtc[NTC_RESERVED_1]         = Temp_ntc_10k(adc_res[NTC_RESERVED_1]          , 0);
    TempNtc[NTC_RESERVED_2]         = Temp_ntc_10k(adc_res[NTC_RESERVED_2]          , 0);
}

int16_t GetNtcTemperature(uint8_t whichSensor)
{
    return TempNtc[whichSensor];
}

uint16_t GetAdcValue(uint8_t whichSensor)
{
    return adc_res[whichSensor];
}

uint8_t ntc_find_segment_from_adc(uint16_t adcValue)
{
    if (adcValue >= ntc_10k_tabel[0][0]) {
        // If adc value is equal or bigger than first in array, return the first in the array
        return 0;
    }

    if (adcValue <= ntc_10k_tabel[0][NTC_TABLE_SIZE - 2]) {
        // if adc is equal or lower than 260 (temperature is higher than 100 degrees), return [27]
        return NTC_TABLE_SIZE - 3;
    }

    for (int i = 0; i < (NTC_TABLE_SIZE - 2); i++)
    {
        uint16_t highCompareValue = ntc_10k_tabel[0][i];
        uint16_t  lowCompareValue = ntc_10k_tabel[0][i+1];

        if ((adcValue >= lowCompareValue) && (adcValue <= highCompareValue)) {
            return i;
        }
    }

    return NTC_TABLE_SIZE - 3;
}

uint8_t ntc_get_counts_per_5C_from_adc(uint16_t adcValue)
{
    uint8_t segment = ntc_find_segment_from_adc(adcValue);

    uint8_t counts = ntc_verschil[segment];

    if (counts == 255) {
        return 1;  
    }
    
    if (counts == 0) {
        return 1;
    }

    return counts;
}