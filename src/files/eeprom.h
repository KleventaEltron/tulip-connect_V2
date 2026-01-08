#ifndef _EEPROM_H    /* Guard against multiple inclusion */
#define _EEPROM_H

// Define the NVMCTRL_SEESBLK_MASK_BITS to extract the NVMCTRL_SEESBLK bits(35:32) from NVM User Page Mapping(0x00804000)
#define NVMCTRL_SEESBLK_MASK_BITS   0x0F

// Define the NVMCTRL_SEEPSZ_MASK_BITS to extract the NVMCTRL_SEEPSZ bits(38:36) from NVM User Page Mapping(0x00804000)
#define NVMCTRL_SEEPSZ_MASK_BITS    0x07

/* A specific byte pattern stored at the begining of SmartEEPROM data area.
 * When the application comes from a reset, if it finds this signature,
 * the assumption is that the SmartEEPROM has some valid data.
 */
#define SMEE_CUSTOM_SIG         0x5a5a5a5a      // t/m V1-0-10, daarna niet meer
//#define SMEE_CUSTOM_SIG         0x12345678

// Define the number of bytes to be read when a read request comes 
#define MAX_BUFF_SIZE          256

// This example demo assumes fuses SBLK = 1 and PSZ = 3, thus total size is 4096 bytes 
#define SEEP_FINAL_BYTE_INDEX   4095


// Eeprom addresses:
#define SEEP_ADDR_EEPROM_VERSION        0 // 4 bytes    V1-0-8 of lager
#define SEEP_ADDR_TEST                  4 // 1 byte     V1-0-8 of lager
#define SEEP_ADDR_HEATPUMP_MODE         5 // 2 bytes    V1-0-8 of lager
#define SEEP_ADDR_HEATING_SETPOINT      7 // 2 bytes    V1-0-8 of lager
#define SEEP_ADDR_HOT_WATER_SETPOINT    9 // 2 bytes    V1-0-8 of lager

// Circulation pump settings:
#define SEEP_ADDR_PUMP_MAXIMUM_OFF_TIME_SEC                                 11 // 4 bytes   V1-0-8 of lager
#define SEEP_ADDR_PUMP_ON_TIME_AFTER_OFF_TIME_REACHED_SEC                   15 // 2 bytes   V1-0-8 of lager
#define SEEP_ADDR_PUMP_LAG_TIME_AFTER_THERMOSTAT_CONTACT_DISCONNECTED_SEC   17 // 2 bytes   V1-0-8 of lager

// Hot water settings:
#define SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT  19 // 2 bytes   V1-0-8 of lager
#define SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC 21 // 2 bytes   V1-0-8 of lager

// Heating settings:
#define SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME   23 // 2 bytes   V1-0-8 of lager
#define SEEP_ADDR_HEATING_TIME_CONSTANT_SEC         25 // 2 bytes   V1-0-8 of lager

#define SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE  27 // 2 bytes   V1-0-8 of lager
#define SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_START       29 // 2 bytes   V1-0-8 of lager
#define SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_STEPS       31 // 2 bytes   V1-0-8 of lager

// Sterilization
#define SEEP_ADDR_DAY_COUNTER_STERILIZATION                     33 // 2 bytes   V1-0-9
#define SEEP_ADDR_SECONDS_COUNTER_DAYS                          35 // 4 bytes   V1-0-9
#define SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_START           39 // 2 bytes   V1-0-9
#define SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_STEPS           41 // 2 bytes   V1-0-9
#define SEEP_ADDR_STERILIZATION_MAX_TIME_IN_STERILIZATION_MODE  43 // 2 bytes   V1-0-9
#define SEEP_ADDR_STERILIZATION_MAX_TIME_OUT_OF_STERILIZATION_MODE_WITH_ELEMENT_ON  45 // 2 bytes   V1-0-9

#define SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON    47 // 2 bytes   V1-0-9
#define SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF   49 // 2 bytes   V1-0-9

#define SEEP_ADDR_RESET_COUNTER                             51 // 4 bytes   V1-0-12
#define SEEP_ADDR_CONNECT_HAS_EVER_CONTACTED_HEATPUMP       55 // 1 bytes   V1-0-12

#define SEEP_ADDR_PUMP_OFF_TEMP_TOO_LOW                     56 // 2 bytes   V1-0-12
#define SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_LOW_TEMP           58 // 2 bytes   V1-0-12

#define BOOTLOADER_SOFTWARE_VERSION                         60 // 4 bytes V1-0-13

#define SEEP_ADDR_DISPLAY_PUMP_ON                           65 // 1 byte V2-0-0

#define SEEP_ADDR_PUMP_OFF_TEMP_TOO_HIGH                    66 // 2 bytes V2-0-0
#define SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_HIGH_TEMP          68 // 2 bytes V2-0-0

#define SEEP_ADDR_COOLING_SETPOINT                          70 // 2 bytes V2-0-1

#define SEEP_ADDR_EVU_CONTACT_ENABLE                        74 // 1 byte V2-0-5
#define SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF         75 // 1 byte V2-0-5
#define SEEP_ADDR_SWITCH_HEATPUMP_ON_OFF_WITH_THERMOSTAT    76 // 1 byte V2-0-5

#define SEEP_ADDR_SOFTWARE_RESET                            77 // 1 byte V2-0-5
#define SEEP_ADDR_SILENT_MODE                               78 // 1 byte V2-0-5
#define SEEP_ADDR_START_TIME_SILENT_MODE                    79 // 2 byte V2-0-5
#define SEEP_ADDR_END_TIME_SILENT_MODE                      81 // 2 byte V2-0-5
#define SEEP_ADDR_BOOST_MODE                                83 // 1 byte V2-0-5
#define SEEP_ADDR_USE_SILENT_MODE_TIMERS                    84 // 1 byte V2-0-5
#define SEEP_ADDR_BLOCK_HOTWATER                            85 // 1 byte V2-0-5
#define SEEP_ADDR_START_TIME_BLOCK_HOTWATER                 86 // 2 byte V2-0-5
#define SEEP_ADDR_END_TIME_BLOCK_HOTWATER                   88 // 2 byte V2-0-5
// #define SEEP_ADDR_MINIMUM_TIME_IN_HOTWATER                  87 // 1 byte V2-0-5
// #define SEEP_ADDR_HOTWATER_ELEMENT_ON_AFTER_TIME            88 // 1 byte V2-0-5
// #define SEEP_ADDR_HOTWATER_ELEMENT_MAX_ON_TIME              89 // 1 byte V2-0-5
// #define SEEP_ADDR_PUMP_ON_TIME_AFTER_OFF_TIME_REACHED       90 // 1 byte V2-0-5

#define SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP      90 // 2 bytes V2-0-5 of lager
#define SEEP_ADDR_COOLING_SETPOINT_CURVE_BACKUP      92 // 2 bytes V2-0-5 of lager

#define SEEP_ADDR_HEATING_CURVE                             94 // 2 byte V2-0-5
#define SEEP_ADDR_COOLING_CURVE                             96 // 2 byte V2-0-5

// DIGITAL INPUTS ARE PLACEHOLDER NAMES, CHANGE WHEN CONFIGURABLE THROUGH WEB APP
#define SEEP_ADDR_DIGITAL_INPUT_TWO                          98 // 1 byte V2-0-5
#define SEEP_ADDR_DIGITAL_INPUT_ONE                        99 // 1 byte V2-0-5

#define SEEP_ADDR_COOLING_CONTACT_ENABLE                     100 // 1 byte V2-0-5
// RELAIS OUTPUTS ARE PLACEHOLDER NAMES, CHANGE WHEN CONFIGURABLE THROUGH WEB APP
#define SEEP_ADDR_RELAIS_OUTPUT_TWO                          101 // 1 byte V2-0-5
#define SEEP_ADDR_RELAIS_OUTPUT_THREE                        102 // 1 byte V2-0-5

#define SEEP_ADDR_EMERGENCY_MODE_HEATING_ENABLED                        103 // 2 bytes V2-0-12
#define SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED                       105 // 2 bytes V2-0-12
#define SEEP_ADDR_STERILIZATION_ON_HOLD                                 107 // 2 bytes V2-0-12

#define SEEP_ADDR_CIRCULATION_PUMP_OFF_TEMPERATURE                      109 // 2 bytes V2-0-13
#define SEEP_ADDR_CIRCULATION_PUMP_ON_TEMPERATURE                       111 // 2 bytes V2-0-13

#define SEEP_ADDR

//.....
#define SEEP_ADDR_LAST                  4095

void WriteSmartEeprom8(uint32_t address, uint8_t data);
void WriteSmartEeprom16(uint32_t address, uint16_t data);
void WriteSmartEeprom32(uint32_t address, uint32_t data);
uint8_t ReadSmartEeprom8(uint32_t address);
uint16_t ReadSmartEeprom16(uint32_t address);
uint32_t ReadSmartEeprom32(uint32_t address);
void SmartEepromInit(void);
void restoreEepromValuesToDefault(void);
void printCustomEepromParameters(void);

#endif 