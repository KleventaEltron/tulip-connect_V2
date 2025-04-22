#ifndef _HEATPUMP_H    /* Guard against multiple inclusion */
#define _HEATPUMP_H

#include "heatpump_parameters.h"
#include "../../app_heatpump_comm.h"
#include "../states.h"

void ParseHeatpumpData(uint8_t * txBufferHeatpump, uint8_t * rxBufferHeatpump);
void FillTxBuffer(uint8_t * txBuffer);
uint8_t FillBufferWithStartupSettings(bool doFirstTimeHeatpumpCommunicationSettings);

#endif /* _HEATPUMP_H */