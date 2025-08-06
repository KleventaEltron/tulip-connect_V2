#ifndef _HEATPUMP_H    /* Guard against multiple inclusion */
#define _HEATPUMP_H

#include "heatpump_parameters.h"
#include "../../app_heatpump_comm.h"

void ParseHeatpumpData(uint8_t * txBufferHeatpump, uint8_t * rxBufferHeatpump);
void FillTxBuffer(uint8_t * txBuffer);
void FillBufferWithStartupSettings(bool doFirstTimeHeatpumpCommunicationSettings);
void CheckHeatpumpStaticSettings();
char * getHeatpumpStateToString(APP_HEATPUMP_COMM_STATES logState);

#endif /* _HEATPUMP_H */