#ifndef _TIMER_COUNTERS_H    /* Guard against multiple inclusion */
#define _TIMER_COUNTERS_H

#define TENTH_SECOND_COUNTER_1_SECOND           10
#define TENTH_SECOND_COUNTER_5_SECONDS          50
#define TENTH_SECOND_COUNTER_100_MILLISECOND    1
//#define TENTH_SECOND_COUNTER_1_DAY              864000 // 1 dag
#define TENTH_SECOND_COUNTER_1_MINUTE            600 // 1 minuten
#define TENTH_SEC0ND_COUNTER_5_MINUTES          3000
#define TENTH_SECOND_COUNTER_20_SECONDS         200     // 1 minuten
#define TENTH_SECOND_COUNTER_30_SECONDS         300
//#define TENTH_SECOND_COUNTER_1_DAY  36000             // 1 uur

#define SYS_STUCK_TIMER_MAX_LIMIT               300
#define SECONDS_IN_DAY                          86400


//uint32_t secondCounterLegionella;
//uint32_t waitingThreeWayValveSwitch;
//uint32_t systemStuckProtectionCounter;

void InitTimerCounters ( void );

void UpdateCounters ( void );
uint32_t getSecondCounterHeatingTask();
void setSecondCounterHeatingTask(uint32_t count);
uint32_t getSecondCounterHotwaterTask();
void setSecondCounterHotwaterTask(uint32_t count);
uint32_t getSecondCounterCirculationPumpTask();
void setSecondCounterCirculationPumpTask(uint32_t count);
uint32_t getSecondCounterDelayAfterChangingSettings();
void setSecondCounterDelayAfterChangingSettings(uint32_t count);
uint32_t getSecondCounterLegionella();
void setSecondCounterLegionella(uint32_t value);
uint32_t getWaitingThreeWayValveSwitch();
void setWaitingThreeWayValveSwitch(uint32_t value);
//uint32_t getSystemStuckProtectionCounter();
//void setSystemStuckProtectionCounter(uint32_t value);
uint32_t getWriteNewSetPointHeatpumpCounter();
void setWriteNewSetPointHeatpumpCounter(uint32_t value);
uint32_t getCheckSilentModeOnTimerCounter();
void setCheckSilentModeOnTimerCounter(uint32_t value);
uint32_t getsystemOnCounter();
void setSystemOnCounter(uint32_t value);
uint32_t getWaitForSettingEchoProtection();
void setWaitForSettingEchoProtection(uint32_t count);
uint32_t getWriteHeatpumpRunningModeCounter();
void setWriteHeatpumpRunningModeCounter(uint32_t value);
uint32_t getWriteHeatpumpForcedOffCounter();
void setWriteHeatpumpForcedOffCounter(uint32_t value);
uint32_t getWaitingTurningHeatpumpOn();
void setWaitingTurningHeatpumpOn(uint32_t value);
uint32_t getCheckHeatpumpStaticSettingsCounter();
void setCheckHeatpumpStaticSettingsCounter(uint32_t value);

bool LedsTimerExpired ( void );
bool HeatingHotWaterTimerExpired ( void );
bool DisplayCommunicationTimerExpired ( void );
bool HeatpumpCommunicationTimerExpired ( void );
bool LoggingTimerExpired ( void );
bool LoggingTimerExpiredSettingsInterval ( void );
bool LoggingTimerSDCardExpired ( void );
void resetLoggingTimers( void );
//bool FtpTimerExpired ( void );
//bool ResetSoftwareTimerExpired ( void );

#endif 