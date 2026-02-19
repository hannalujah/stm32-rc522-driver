#ifndef INC_RC522_MAIN_H_
#define INC_RC522_MAIN_H_
#include "rc522.h"
#include <string.h>

#define M_RC522_TAGTYPE_MAXLEN	      4
#define M_RC522_CARDID_MAXLEN	      8


typedef struct {
	uint32_t     detectionRemainingTime;
	_Bool        isDetectionEnabled;
	_Bool        isInitialized;
	_Bool        isModuleOn;
	uint8_t      res1;
}s_rc522_detectionStatus_structure;


typedef struct {
	uint8_t 	tagtype[M_RC522_TAGTYPE_MAXLEN];
	uint8_t		cardID[M_RC522_CARDID_MAXLEN];
	_Bool  		isIDValid;
	_Bool  		isUnreadWaiting;
	uint16_t    res1;
}s_rc522_IDStatus_structure;


typedef struct {
	s_rc522_detectionStatus_structure   s_detection;
	s_rc522_IDStatus_structure          s_IDStatus;
}s_rc522_status_structure;


typedef struct {
	uint32_t         detectionTimeout;
	uint32_t         detectionFailsafeTimeout;
	uint32_t 		 detectionMaximumValidTime;
	uint32_t         confirmationTries;
}s_rc522_config_structure;

typedef struct {
	s_rc522_config_structure    s_config;
	s_rc522_status_structure    s_status;
} s_rc522_structure;
extern s_rc522_structure s_rc522;

void RC522_init(void);
void RC522_run(void);
void RC522_tick();
_Bool RC522_isActive(void);


void RC522_startDetection(void);


// DMAs
HAL_StatusTypeDef RC522_Write(uint8_t addr, uint8_t val);
HAL_StatusTypeDef RC522_Read(uint8_t addr, uint8_t* readValue);
void RC522_startTransmit(void);




#endif /* INC_RC522_MAIN_H_ */
