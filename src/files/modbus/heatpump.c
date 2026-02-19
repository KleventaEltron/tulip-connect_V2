/* TODO:  Include other files here if needed. */
#include <xc.h>
#include <string.h>
#include <stdbool.h>                    // Defines true
#include "heatpump.h"
#include "definitions.h"

#include "modbus.h"
#include "heatpump_parameters.h"
#include "files\eeprom.h"
#include "../time_counters.h"
#include "../states.h"

#define COMMUNICATION_ARRAY_NORMAL_ROWS 17
#define COMMUNICATION_ARRAY_EXTRA_ROWS_FOR_CASCADE 15
#define COMMUNICATION_ARRAY_MAX_ROWS (COMMUNICATION_ARRAY_NORMAL_ROWS + COMMUNICATION_ARRAY_EXTRA_ROWS_FOR_CASCADE)
#define COMMUNICATION_ARRAY_BYTES_PER_MESSAGE 8

static uint8_t CommunicationArray[COMMUNICATION_ARRAY_MAX_ROWS][COMMUNICATION_ARRAY_BYTES_PER_MESSAGE] = 
{
    {0x01, 0x03, 0x08, 0x00, 0x00, 0x10, 0x46, 0x66},   // Zelfde
    {0x01, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0xF1},   // Zelfde
    {0x01, 0x03, 0x00, 0x5A, 0x00, 0x22, 0xE5, 0xC0},   // Zelfde
    {0x01, 0x03, 0x00, 0xE0, 0x00, 0x22, 0xC4, 0x25},   // Zelfde
    {0x01, 0x03, 0x01, 0x00, 0x00, 0x64, 0x45, 0xDD},   // Zelfde
    {0x01, 0x03, 0x01, 0x64, 0x00, 0x64, 0x04, 0x02},   // 01 03 01 64 00 64 04 02
    {0x01, 0x03, 0x01, 0xC8, 0x00, 0x64, 0xC4, 0x23},   // 01 03 01 C8 00 64 C4 23
    {0x01, 0x03, 0x03, 0x00, 0x00, 0x69, 0x85, 0xA0},   // 01 03 03 00 00 69 85 A0
    // 01 03 08 00 00 10: 0x0800 tm 0x080F  (Unit System Parameter L)   // Zelfde
    // 01 03 00 00 00 5A: 0x0000 tm 0x0059  (Real-time data)            // Zelfde
    // 01 03 00 5A 00 22: 0x005A tm 0x007B  (Real-time data)            // Zelfde
    // 01 03 00 E0 00 22: 0x00E0 tm 0x0101  (Real-time data)            // Zelfde
    // 01 03 01 00 00 64: 0x0100 tm 0x0163  (System parameters)         // Zelfde
    // 01 03 01 64 00 4D: 0x0164 tm 0x01B0  (System parameters)         // 0x0164 tm 0x01C7 
    // 01 03 01 B1 00 5F: 0x01B1 tm 0x020F  (System parameters)         // 0x01C8 tm 0x022B
    // 01 03 03 00 00 64: 0x0300 tm 0x0363  (User parameters)           // 0x0300 tm 0x0368
    
    {0x01, 0x03, 0x03, 0x70, 0x00, 0x30, 0x44, 0x41},   // Zelfde
    {0x01, 0x03, 0x04, 0x00, 0x00, 0x64, 0x45, 0x11},   // 01 03 04 00 00 64 45 11 -> CRC fout???
    {0x01, 0x03, 0x06, 0x00, 0x00, 0x64, 0x44, 0xA9},   // Zelfde
    {0x01, 0x03, 0x04, 0x64, 0x00, 0x64, 0x04, 0xCE},   // 01 03 04 64 00 64 04 CE
    {0x01, 0x03, 0x06, 0x64, 0x00, 0x64, 0x05, 0x76},   // 01 03 06 64 00 64 05 76
    {0x01, 0x03, 0x04, 0xC8, 0x00, 0x64, 0xC4, 0xEF},   // 01 03 04 C8 00 64 C4 EF
    {0x01, 0x03, 0x06, 0xC8, 0x00, 0x64, 0xC5, 0x57},   // 01 03 06 C8 00 64 C5 57
    {0x01, 0x03, 0x08, 0x40, 0x00, 0x10, 0x47, 0xB2},   // Zelfde
    {0x01, 0x03, 0x08, 0x80, 0x00, 0x10, 0x47, 0x8E},   // Zelfde
    // Unknown settings:
    // 01 03 03 70 00 30: 0x0370 tm 0x039F  (Unknown settings 1)        // Zelfde
    // 01 03 04 00 00 64: 0x0400 tm 0x0463  (Unknown settings 2)        // Zelfde
    // 01 03 06 00 00 64: 0x0600 tm 0x0663  (Unknown settings 3)        // Zelfde
    // 01 03 04 64 00 4D: 0x0464 tm 0x04B0  (Unknown settings 4)        // 0x0464 tm 0x04C7
    // 01 03 06 64 00 4D: 0x0664 tm 0x06B0  (Unknown settings 5)        // 0x0664 tm 0x06C7
    // 01 03 04 B1 00 5F: 0x04B1 tm 0x050F  (Unknown settings 6)        // 0x04C8 tm 0x052B
    // 01 03 06 B1 00 5F: 0x06B1 tm 0x070F  (Unknown settings 7)        // 0x06C8 tm 0x072B
    // 01 03 08 40 00 10: 0x0840 tm 0x084F  (Unknown settings 8)        // Zelfde
    // 01 03 08 80 00 10: 0x0880 tm 0x088F  (Unknown settings 9)        // Zelfde
    
    // Data from slaves if needed
    {0x02, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0xC2},   // Slave 1  address 2
    {0x03, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC4, 0x13},   // Slave 2  address 3
    {0x04, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0xA4},   // Slave 3  address 4
    {0x05, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC4, 0x75},   // Slave 4  address 5
    {0x06, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC4, 0x46},   // Slave 5  address 6
    {0x07, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0x97},   // Slave 6  address 7
    {0x08, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0x68},   // Slave 7  address 8
    {0x09, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC4, 0xB9},   // Slave 8  address 9
    {0x0A, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC4, 0x8A},   // Slave 9  address 10
    {0x0B, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0x5B},   // Slave 10 address 11
    {0x0C, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC4, 0xEC},   // Slave 11 address 12
    {0x0D, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0x3D},   // Slave 12 address 13
    {0x0E, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0x0E},   // Slave 13 address 14
    {0x0F, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC4, 0xDF},   // Slave 14 address 15
    {0x10, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC6, 0xB0}    // Slave 15 address 16
};
/*
static uint8_t CommunicationArray[17][8] = 
{
    {0x01, 0x03, 0x08, 0x00, 0x00, 0x10, 0x46, 0x66},   // Zelfde
    {0x01, 0x03, 0x00, 0x00, 0x00, 0x5A, 0xC5, 0xF1},   // Zelfde
    {0x01, 0x03, 0x00, 0x5A, 0x00, 0x22, 0xE5, 0xC0},   // Zelfde
    {0x01, 0x03, 0x00, 0xE0, 0x00, 0x22, 0xC4, 0x25},   // Zelfde
    {0x01, 0x03, 0x01, 0x00, 0x00, 0x64, 0x45, 0xDD},   // Zelfde
    {0x01, 0x03, 0x01, 0x64, 0x00, 0x4D, 0xC5, 0xDC},   // 01 03 01 64 00 64 04 02
    {0x01, 0x03, 0x01, 0xB1, 0x00, 0x5F, 0x54, 0x29},   // 01 03 01 C8 00 64 C4 23
    {0x01, 0x03, 0x03, 0x00, 0x00, 0x64, 0x44, 0x65},   // 01 03 03 00 00 69 85 A0
    // 01 03 08 00 00 10: 0x0800 tm 0x080F  (Unit System Parameter L)   // Zelfde
    // 01 03 00 00 00 5A: 0x0000 tm 0x0059  (Real-time data)            // Zelfde
    // 01 03 00 5A 00 22: 0x005A tm 0x007B  (Real-time data)            // Zelfde
    // 01 03 00 E0 00 22: 0x00E0 tm 0x0101  (Real-time data)            // Zelfde
    // 01 03 01 00 00 64: 0x0100 tm 0x0163  (System parameters)         // Zelfde
    // 01 03 01 64 00 4D: 0x0164 tm 0x01B0  (System parameters)         // 0x0164 tm 0x01C7 
    // 01 03 01 B1 00 5F: 0x01B1 tm 0x020F  (System parameters)         // 0x01C8 tm 0x022B
    // 01 03 03 00 00 64: 0x0300 tm 0x0363  (User parameters)           // 0x0300 tm 0x0368
    
    {0x01, 0x03, 0x03, 0x70, 0x00, 0x30, 0x44, 0x41},   // Zelfde
    {0x01, 0x03, 0x04, 0x00, 0x00, 0x64, 0x45, 0x44},   // 01 03 04 00 00 64 45 11 -> CRC fout???
    {0x01, 0x03, 0x06, 0x00, 0x00, 0x64, 0x44, 0xA9},   // Zelfde
    {0x01, 0x03, 0x04, 0x64, 0x00, 0x4D, 0xC5, 0x10},   // 01 03 04 64 00 64 04 CE
    {0x01, 0x03, 0x06, 0x64, 0x00, 0x4D, 0xC4, 0xA8},   // 01 03 06 64 00 64 05 76
    {0x01, 0x03, 0x04, 0xB1, 0x00, 0x5F, 0x54, 0xE5},   // 01 03 04 C8 00 64 C4 EF
    {0x01, 0x03, 0x06, 0xB1, 0x00, 0x5F, 0x55, 0x5D},   // 01 03 06 C8 00 64 C5 57
    {0x01, 0x03, 0x08, 0x40, 0x00, 0x10, 0x47, 0xB2},   // Zelfde
    {0x01, 0x03, 0x08, 0x80, 0x00, 0x10, 0x47, 0x8E}    // Zelfde
    // Unknown settings:
    // 01 03 03 70 00 30: 0x0370 tm 0x039F  (Unknown settings 1)        // Zelfde
    // 01 03 04 00 00 64: 0x0400 tm 0x0463  (Unknown settings 2)        // Zelfde
    // 01 03 06 00 00 64: 0x0600 tm 0x0663  (Unknown settings 3)        // Zelfde
    // 01 03 04 64 00 4D: 0x0464 tm 0x04B0  (Unknown settings 4)        // 0x0464 tm 0x04C7
    // 01 03 06 64 00 4D: 0x0664 tm 0x06B0  (Unknown settings 5)        // 0x0664 tm 0x06C7
    // 01 03 04 B1 00 5F: 0x04B1 tm 0x050F  (Unknown settings 6)        // 0x04C8 tm 0x052B
    // 01 03 06 B1 00 5F: 0x06B1 tm 0x070F  (Unknown settings 7)        // 0x06C8 tm 0x072B
    // 01 03 08 40 00 10: 0x0840 tm 0x084F  (Unknown settings 8)        // Zelfde
    // 01 03 08 80 00 10: 0x0880 tm 0x088F  (Unknown settings 9)        // Zelfde
};
*/

//uint16_t CoilAddresses[ADDRESSES_COILS_ADDRESSES];

// TYPE PARAMETER:         | START ADDRESS: | END ADDRESS: | TOTAL AMOUNT: |  
// ------------------------------------------------------------------------------------------------------------------
// Real-time data          | 0x0000         | 0x00FF       | 256           | 
// System parameters       | 0x0100         | 0x02FF       | 512           |
// User parameters         | 0x0300         | 0x036F       | 112           |
// Unit System Parameter L | 0x0800         | 0x083F       | 64            |
// Coils address           | 0x1000         | 0x10FF       | 256           |
// ------------------------------------------------------------------------------------------------------------------
// TOTAL:                                                    1198


static void parseWriteReg(uint8_t * rxBuffer)
{
    uint16_t regAddress = ((uint16_t)rxBuffer[MODBUS_REG_ADDRESS_MSB_INDEX] << 8) + rxBuffer[MODBUS_REG_ADDRESS_LSB_INDEX];
    uint16_t data       = ((uint16_t)rxBuffer[MODBUS_MOD_DATA_MSB_INDEX] << 8) + rxBuffer[MODBUS_MOD_DATA_LSB_INDEX];
    
    ConfirmSettingIsEchoed(regAddress, data);
}

void saveDataToMemory(uint16_t address, uint16_t data, uint8_t deviceAddress)
{
    deviceAddress -= 1; // Decrease deviceAddress with 1, because pointer in array is always 1 lower than device address in modbus protocol
    
    if ((address >= START_ADDRESS_REAL_TIME_DATA_1) && (address < START_ADDRESS_REAL_TIME_DATA_1 + REGISTERS_AMOUNT_REAL_TIME_DATA_1))
    {
        address -= START_ADDRESS_REAL_TIME_DATA_1;
        RealTimeData1[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][deviceAddress] = data;
    }
    else if ((address >= START_ADDRESS_REAL_TIME_DATA_2) && (address < START_ADDRESS_REAL_TIME_DATA_2 + REGISTERS_AMOUNT_REAL_TIME_DATA_2))
    {
        address -= START_ADDRESS_REAL_TIME_DATA_2;
        RealTimeData2[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNIT_SYSTEM_PARAMETERS) && (address < START_ADDRESS_UNIT_SYSTEM_PARAMETERS + REGISTERS_AMOUNT_UNIT_SYSTEM_PARAMETERS))
    {
        address -= START_ADDRESS_UNIT_SYSTEM_PARAMETERS;
        UnitSystemParameters[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_USER_PARAMETERS) && (address < START_ADDRESS_USER_PARAMETERS + REGISTERS_AMOUNT_USER_PARAMETERS))
    {
        address -= START_ADDRESS_USER_PARAMETERS;
        UserParameters[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_USER_ORDER) && (address < START_ADDRESS_USER_ORDER + REGISTERS_AMOUNT_USER_ORDER))
    {
        address -= START_ADDRESS_USER_ORDER;
        UserOrder[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_VERSION_INFORMATION) && (address < START_ADDRESS_VERSION_INFORMATION + REGISTERS_AMOUNT_VERSION_INFORMATION))
    {
        address -= START_ADDRESS_VERSION_INFORMATION;
        VersionInformation[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNIT_SYSTEM_PARAMETER_L) && (address < START_ADDRESS_UNIT_SYSTEM_PARAMETER_L + REGISTERS_AMOUNT_UNIT_SYSTEM_PARAMETER_L))
    {
        address -= START_ADDRESS_UNIT_SYSTEM_PARAMETER_L;
        UnitSystemParameterL[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_COIL_ADDRESSES) && (address < START_ADDRESS_COIL_ADDRESSES + REGISTERS_AMOUNT_COIL_ADDRESSES))
    {
        address -= START_ADDRESS_COIL_ADDRESSES;
        CoilAddresses[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    // Unknown arrays:
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_1) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_1 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_1))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_1;
        UnknownParameters1[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_2) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_2 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_2))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_2;
        UnknownParameters2[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_3) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_3 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_3))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_3;
        UnknownParameters3[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_4) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_4 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_4))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_4;
        UnknownParameters4[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_5) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_5 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_5))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_5;
        UnknownParameters5[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_6) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_6 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_6))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_6;
        UnknownParameters6[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_7) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_7 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_7))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_7;
        UnknownParameters7[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_8) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_8 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_8))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_8;
        UnknownParameters8[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_9) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_9 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_9))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_9;
        UnknownParameters9[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] = data;
    }
    else{}        
    //asm("nop");
}

char * getHeatpumpStateToString(APP_HEATPUMP_COMM_STATES logState) {
    switch(logState){
        case(APP_HEATPUMP_COMM_STATE_INIT): return "0, Init"; break;        
        case(APP_HEATPUMP_COMM_STATE_SEND_DATA): return "1, Send Data"; break;   
        case(APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_SENT): return "2, Wait Send"; break;    
        case(APP_HEATPUMP_COMM_STATE_RECEIVE_DATA): return "3, Receive Data"; break;    
        case(APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_RECEIVED): return "4, Wait Receive"; break;    
        case(APP_HEATPUMP_COMM_STATE_CHECKSUM_CHECK): return "5, Checksum"; break;      
        case(APP_HEATPUMP_COMM_STATE_PARSE_DATA):  return "6, Parse Data"; break;    
        case(APP_HEATPUMP_COMM_STATE_DELAY): return "7, Delay"; break;              
        default: return "8, Unkown"; break;
    }
}



static void parseReadRegs(uint8_t * txBuffer, uint8_t * rxBuffer)
{
    //uint8_t i;
   
    //uint16_t checksum;
    //uint16_t currentAddress;
    uint16_t data;
    uint16_t j = 3;
    uint8_t  deviceAddress = rxBuffer[MODBUS_ADDRESS_INDEX];
    uint16_t registerAddress = ((uint16_t)txBuffer[MODBUS_REG_ADDRESS_MSB_INDEX] << 8) + txBuffer[MODBUS_REG_ADDRESS_LSB_INDEX];
    uint16_t amountOfRegisters = ((uint16_t)txBuffer[MODBUS_REG_AMOUNT_MSB_INDEX] << 8) + txBuffer[MODBUS_REG_AMOUNT_LSB_INDEX]; 
    //uint8_t  amountOfBytes = rxBuffer[MODBUS_BYTES_RETURNED_INDEX]; 
     
    for (uint16_t i = registerAddress; i < (registerAddress + amountOfRegisters); i++)
    {
        data = ((uint16_t)rxBuffer[j] << 8) + rxBuffer[j + 1];
        j += 2;
        //checkWhatData(i);
        saveDataToMemory(i, data, deviceAddress);
    }
    
//    if (GetDigitalInput2()){
//        RealTimeData1[ADDRESS_COMPRESSOR_OPERATING_FREQUENCY][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][MASTER_HEATPUMP_IN_CASCADE] = 50;
//    }
//    else {
//        RealTimeData1[ADDRESS_COMPRESSOR_OPERATING_FREQUENCY][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][MASTER_HEATPUMP_IN_CASCADE] = 0;
//    }
}

void ParseHeatpumpData(uint8_t * txBuffer, uint8_t * rxBuffer)
{
    if (rxBuffer[MODBUS_ADDRESS_INDEX] == txBuffer[MODBUS_ADDRESS_INDEX])
    {   // voor mij
        if (rxBuffer[MODBUS_COMMAND_INDEX] == MB_FC_WRITE_REG)
        {
            parseWriteReg(rxBuffer);
        }
        else if (rxBuffer[MODBUS_COMMAND_INDEX] == MB_FC_READ_REGS)
        {
            parseReadRegs(txBuffer, rxBuffer);
        }
    }
    else{} // Niet voor mij
}

void FillTxBuffer(uint8_t * txBuffer)
{
    static uint8_t i = 0;
    
    //if (setting.settingStatus == SETTING_SEND_STATUS_SETTING_FILLED)
    if(getSettingsQueuedAmount() > 0)
    {
        MANUAL_SETTING setting = (MANUAL_SETTING)PopFirstSetting();
        txBuffer[MODBUS_ADDRESS_INDEX] =            setting.modbusDeviceAddress;
        txBuffer[MODBUS_COMMAND_INDEX] =            setting.modbusCommand;
        txBuffer[MODBUS_REG_ADDRESS_MSB_INDEX] =    (uint8_t)(setting.modbusWriteRegister >> 8);
        txBuffer[MODBUS_REG_ADDRESS_LSB_INDEX] =    (uint8_t)(setting.modbusWriteRegister >> 0);
        txBuffer[MODBUS_MOD_DATA_MSB_INDEX] =       (uint8_t)(setting.modbusWriteData >> 8);
        txBuffer[MODBUS_MOD_DATA_LSB_INDEX] =       (uint8_t)(setting.modbusWriteData >> 0);

        uint16_t checksum = calculateCRC16(txBuffer, 6);
        txBuffer[MODBUS_CHECKSUM_LSB_INDEX] = (uint8_t)(checksum >> 0);
        txBuffer[MODBUS_CHECKSUM_MSB_INDEX] = (uint8_t)(checksum >> 8);
        
        ChangeFirstSettingStatus(SETTING_SEND_STATUS_SETTING_IS_SENT);
        setWaitForSettingEchoProtection(0);
    }
    else
    {
        // Just asking the normal data
        bool foundFrame = false;
        
        while (!foundFrame) {
            // Wait for valid frame to send
            
            if (i < COMMUNICATION_ARRAY_NORMAL_ROWS) {
                // First 17 normal rows, always send these
                foundFrame = true;
            }
            else if (i < COMMUNICATION_ARRAY_MAX_ROWS){
                // Extra 15 rows for cascade slave(s)
                // First time here i = 17;
                uint8_t extraIndex = i - (COMMUNICATION_ARRAY_NORMAL_ROWS - 1);  // 0 t/m 14

                uint16_t cascadeSlavesStatus = getCascadeSlaveStatus();

                // controleer of het bijbehorende bit aan staat
                if ((cascadeSlavesStatus >> extraIndex) & 1) {
                    // bit is 1 ? Cascade Slave is present
                    foundFrame = true;
                } 
                else {
                    // bit is 0 ? Cascade Slave is NOT present
                    i++;
                }
            }
            else {
                // end
                i = 0;
            }
        }

        txBuffer[MODBUS_ADDRESS_INDEX] =            CommunicationArray[i][0];
        txBuffer[MODBUS_COMMAND_INDEX] =            CommunicationArray[i][1];
        txBuffer[MODBUS_REG_ADDRESS_MSB_INDEX] =    CommunicationArray[i][2];
        txBuffer[MODBUS_REG_ADDRESS_LSB_INDEX] =    CommunicationArray[i][3];
        txBuffer[MODBUS_REG_AMOUNT_MSB_INDEX] =     CommunicationArray[i][4];
        txBuffer[MODBUS_REG_AMOUNT_LSB_INDEX] =     CommunicationArray[i][5];

        uint16_t checksum = calculateCRC16(txBuffer, 6);
        txBuffer[MODBUS_CHECKSUM_LSB_INDEX] = (uint8_t)(checksum >> 0);
        txBuffer[MODBUS_CHECKSUM_MSB_INDEX] = (uint8_t)(checksum >> 8);
        
        i++; 
        
        if (i >= COMMUNICATION_ARRAY_MAX_ROWS) {
            i = 0;
        }
    }
}

uint16_t getHeatpumpData(uint16_t address)
{
    uint16_t returnData;
    
    // Known parameters
    if ((address >= START_ADDRESS_REAL_TIME_DATA_1) && (address < START_ADDRESS_REAL_TIME_DATA_1 + REGISTERS_AMOUNT_REAL_TIME_DATA_1))
    {
        address -= START_ADDRESS_REAL_TIME_DATA_1;
        //RealTimeDataStatussen[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
        returnData = RealTimeData1[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][MASTER_HEATPUMP_IN_CASCADE];
    }
    else if ((address >= START_ADDRESS_REAL_TIME_DATA_2) && (address < START_ADDRESS_REAL_TIME_DATA_2 + REGISTERS_AMOUNT_REAL_TIME_DATA_2))
    {
        address -= START_ADDRESS_REAL_TIME_DATA_2;
        returnData = RealTimeData2[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNIT_SYSTEM_PARAMETERS) && (address < START_ADDRESS_UNIT_SYSTEM_PARAMETERS + REGISTERS_AMOUNT_UNIT_SYSTEM_PARAMETERS))
    {
        address -= START_ADDRESS_UNIT_SYSTEM_PARAMETERS;
        returnData = UnitSystemParameters[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_USER_PARAMETERS) && (address < START_ADDRESS_USER_PARAMETERS + REGISTERS_AMOUNT_USER_PARAMETERS))
    {
        address -= START_ADDRESS_USER_PARAMETERS;
        returnData = UserParameters[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_USER_ORDER) && (address < START_ADDRESS_USER_ORDER + REGISTERS_AMOUNT_USER_ORDER))
    {
        address -= START_ADDRESS_USER_ORDER;
        returnData = UserOrder[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_VERSION_INFORMATION) && (address < START_ADDRESS_VERSION_INFORMATION + REGISTERS_AMOUNT_VERSION_INFORMATION))
    {
        address -= START_ADDRESS_VERSION_INFORMATION;
        returnData = VersionInformation[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNIT_SYSTEM_PARAMETER_L) && (address < START_ADDRESS_UNIT_SYSTEM_PARAMETER_L + REGISTERS_AMOUNT_UNIT_SYSTEM_PARAMETER_L))
    {
        address -= START_ADDRESS_UNIT_SYSTEM_PARAMETER_L;
        returnData = UnitSystemParameterL[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_COIL_ADDRESSES) && (address < START_ADDRESS_COIL_ADDRESSES + REGISTERS_AMOUNT_COIL_ADDRESSES))
    {
        address -= START_ADDRESS_COIL_ADDRESSES;
        returnData = CoilAddresses[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    
    // Unknown parameters
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_1) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_1 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_1))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_1;
        returnData = UnknownParameters1[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_2) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_2 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_2))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_2;
        returnData = UnknownParameters2[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_3) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_3 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_3))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_3;
        returnData = UnknownParameters3[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_4) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_4 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_4))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_4;
        returnData = UnknownParameters4[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_5) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_5 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_5))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_5;
        returnData = UnknownParameters5[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_6) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_6 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_6))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_6;
        returnData = UnknownParameters6[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_7) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_7 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_7))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_7;
        returnData = UnknownParameters7[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_8) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_8 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_8))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_8;
        returnData = UnknownParameters8[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else if ((address >= START_ADDRESS_UNKNOWN_PARAMTERS_9) && (address < START_ADDRESS_UNKNOWN_PARAMTERS_9 + REGISTERS_AMOUNT_UNKNOWN_PARAMTERS_9))
    {
        address -= START_ADDRESS_UNKNOWN_PARAMTERS_9;
        returnData = UnknownParameters9[address][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    }
    else
    {
        returnData = UINT16_MAX; 
    }
    
    return returnData;
}

uint16_t getWaterFlowProtectionValue(uint16_t unitToolingNumber)
{
    if ((unitToolingNumber == CURRENT_UNIT_TOOLING_NO_6KW_1) || (unitToolingNumber == CURRENT_UNIT_TOOLING_NO_6KW_2)) {
        // 6KW unit
        return 5;
    }
    
    if ((unitToolingNumber == CURRENT_UNIT_TOOLING_NO_12KW_1) || (unitToolingNumber == CURRENT_UNIT_TOOLING_NO_12KW_2)) {
        // 12KW unit
        return 10;
    }
    
    if ((unitToolingNumber == CURRENT_UNIT_TOOLING_NO_18KW_1) || (unitToolingNumber == CURRENT_UNIT_TOOLING_NO_18KW_2)) {
        // 18KW unit
        return 15;
    }
    
    return UINT16_MAX;
}

void FillBufferWithStartupSettings(bool doFirstTimeHeatpumpCommunicationSettings) {
    //ChangeHeatpumpSetting(ADDRESS_PARAMETER_PASSWORD_SETTING, 255);
    //ChangeHeatpumpSetting(ADDRESS_TANK_TEMPERATURE_PROBE_ENABLED, TANK_TEMPERATURE_PROBE_DISABLED); 
    //ChangeHeatpumpSetting(ADDRESS_UNIT_TEMPERATURE_CONTROL_METHOD, UNIT_TEMPERATURE_CONTROL_METHOD_RETURN_WATER); 
    //ChangeHeatpumpSetting(ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION, 0);  
    //ChangeHeatpumpSetting(ADDRESS_DEVICE_REACHING_TARGET_TEMPERATURE_AND_SHUTDOWN_MODE, 1); 
    
    if (doFirstTimeHeatpumpCommunicationSettings == true)
    {
        ChangeHeatpumpSetting(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE, 5); 
        ChangeHeatpumpSetting(ADDRESS_STERILIZATION_INTERVAL_DAYS, 7); 
        ChangeHeatpumpSetting(ADDRESS_STERILIZATION_START_TIME, 14); 
        ChangeHeatpumpSetting(ADDRESS_STERILIZATION_RUN_TIME, 10); 
        ChangeHeatpumpSetting(ADDRESS_STERILIZATION_TEMPERATURE_SETTING, 65); 
        ChangeHeatpumpSetting(ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION, 0);
    }      
}

void CheckHeatpumpStaticSettings() {
    if (getCheckHeatpumpStaticSettingsCounter() < 30) {
        return;
    }
    
    setCheckHeatpumpStaticSettingsCounter(0);

    if (getHeatpumpData(ADDRESS_PARAMETER_PASSWORD_SETTING) != 255) { 
        ChangeHeatpumpSetting(ADDRESS_PARAMETER_PASSWORD_SETTING, 255);
    }
    
    if (getHeatpumpData(ADDRESS_TANK_TEMPERATURE_PROBE_ENABLED) != TANK_TEMPERATURE_PROBE_DISABLED) { 
        ChangeHeatpumpSetting(ADDRESS_TANK_TEMPERATURE_PROBE_ENABLED, TANK_TEMPERATURE_PROBE_DISABLED);
    }
    
    if (getHeatpumpData(ADDRESS_UNIT_TEMPERATURE_CONTROL_METHOD) != UNIT_TEMPERATURE_CONTROL_METHOD_RETURN_WATER) { 
        ChangeHeatpumpSetting(ADDRESS_UNIT_TEMPERATURE_CONTROL_METHOD, UNIT_TEMPERATURE_CONTROL_METHOD_RETURN_WATER);
    }

    if (getHeatpumpData(ADDRESS_DEVICE_REACHING_TARGET_TEMPERATURE_AND_SHUTDOWN_MODE) != 1) { 
        ChangeHeatpumpSetting(ADDRESS_DEVICE_REACHING_TARGET_TEMPERATURE_AND_SHUTDOWN_MODE, 1);
    }
    
    if (getHeatpumpData(ADDRESS_WATER_FLOW_SWITCH_PROTECTION_LOCK_SETTING) != 1) { 
        ChangeHeatpumpSetting(ADDRESS_WATER_FLOW_SWITCH_PROTECTION_LOCK_SETTING, 1);
    }
    
    if (getHeatpumpData(ADDRESS_COOLING_ANTI_FREEZE_MODE) != 1) {
        ChangeHeatpumpSetting(ADDRESS_COOLING_ANTI_FREEZE_MODE, 1);
    }
    
    if (getHeatpumpData(ADDRESS_COOLING_ANTI_FREEZE_TEMPERATURE_VALUE) != 0) {
        ChangeHeatpumpSetting(ADDRESS_COOLING_ANTI_FREEZE_TEMPERATURE_VALUE, 0);
    }    
    
    uint16_t correctWaterFlowProtectionValue = getWaterFlowProtectionValue(getHeatpumpData(ADDRESS_CURRENT_UNIT_TOOLING_NO));
    
    if (correctWaterFlowProtectionValue != UINT16_MAX){
        // Value is not UINT16_MAX, so it can be checked with its current value, otherwise skip this
        if (getHeatpumpData(ADDRESS_WATER_FLOW_IS_TOO_LOW_PROTECTION_VALUE) != correctWaterFlowProtectionValue) {
            ChangeHeatpumpSetting(ADDRESS_WATER_FLOW_IS_TOO_LOW_PROTECTION_VALUE, correctWaterFlowProtectionValue);
        }
    }
}

/*
uint8_t FillBufferWithStartupSettings(bool doFirstTimeHeatpumpCommunicationSettings)
{
    //static uint8_t i = UINT8_MAX;
    static uint8_t i = 0;
    switch (i)
    {
        case 0:
        {
            ChangeHeatpumpSetting(ADDRESS_PARAMETER_PASSWORD_SETTING, 255);
            i++;
            break;
        }
        
        case 1:
        {
            ChangeHeatpumpSetting(ADDRESS_TANK_TEMPERATURE_PROBE_ENABLED, TANK_TEMPERATURE_PROBE_DISABLED); 
            i++;
            break;
        }
        
        case 2:
        {
            ChangeHeatpumpSetting(ADDRESS_UNIT_TEMPERATURE_CONTROL_METHOD, UNIT_TEMPERATURE_CONTROL_METHOD_RETURN_WATER); 
            i++;
            break;
        }
        
        case 3:
        {
            ChangeHeatpumpSetting(ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION, 0);  
            i++;
            break;
        }
        
        case 4:
        { 
            ChangeHeatpumpSetting(ADDRESS_DEVICE_REACHING_TARGET_TEMPERATURE_AND_SHUTDOWN_MODE, 1); 
            i++;
            break;
        }
        
        case 5:
        {
            if (doFirstTimeHeatpumpCommunicationSettings == true)
            {
                ChangeHeatpumpSetting(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE, 5); 
                ChangeHeatpumpSetting(ADDRESS_STERILIZATION_INTERVAL_DAYS, 7); 
                ChangeHeatpumpSetting(ADDRESS_STERILIZATION_START_TIME, 14); 
                ChangeHeatpumpSetting(ADDRESS_STERILIZATION_RUN_TIME, 10); 
                ChangeHeatpumpSetting(ADDRESS_STERILIZATION_TEMPERATURE_SETTING, 65); 
            }       
            i++;
            break;
        }
        
        default:
        {
            i = UINT8_MAX;
            break;
        }
    }
    return i;
}
 */