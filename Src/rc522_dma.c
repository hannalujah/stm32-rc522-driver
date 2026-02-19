/**


 */
#include "main.h"
#include "string.h"
#include "rc522_main.h"

#define M_RC522_TIMING_TIMECONFIG	20
#define	M_RC522_TIMING_NOTDETECTED  30000
#define M_RC522_TIMING_TIMEOUT 		300000
#define M_RC522_TIMING_CARDVALID	1000
#define M_RC522_VALIDATION_TRYCOUNT 5
#define M_RC522_VALIDATION_TIMEOUT  2000
#define M_CONFIG_RC522_SPIHANDLE    hspi2

// Prototyping the functions used later
void Module_init(void);
void Module_deinit(void);
void ReadRFID(void);
void ResetRFID(void);

uint8_t test;
_Bool rfidOFFtoCard = 0;
_Bool rfidOFFwrite = 0;
_Bool rfidOFFread = 0;


typedef enum {
	M_RC522_READSTEP_FINDCARD,
	M_RC522_READSTEP_FINDCARD_PCDTRANSCEIVE,
	M_RC522_READSTEPS_FINDCARD_CHECKTAGTYPE,
	M_RC522_READSTEP_GETID,
	M_RC522_READSTEP_GETID_PCDTRANSCEIVE,
	M_RC522_READSTEPS_CHECKID,
	M_RC522_READSTEP_VERIFYID
}e_rc522_readCardSteps_typedef;

typedef enum{
	M_RC522_READWRITE_STEP_ONE,
	M_RC522_READWRITE_STEP_TWO
}e_rc522_read_write_step_typedef;

typedef enum {
	M_RC522_TOCARD_STEP_WRITEIENREG,
	M_RC522_TOCARD_STEP_CLEARIRQREG_READ,
	M_RC522_TOCARD_STEP_CLEARIRQREG_WRITE,
	M_RC522_TOCARD_STEP_SETFIFOLEVEL_READ,
	M_RC522_TOCARD_STEP_SETFIFOLEVEL_WRITE,
	M_RC522_TOCARD_STEP_WRITECOMMANDREGIDLE,
	M_RC522_TOCARD_STEP_WRITEFIFODATAREG,
	M_RC522_TOCARD_STEP_WRITECOMMANDREG,
	M_RC522_TOCARD_STEP_SETBITFRAMING_READ,
	M_RC522_TOCARD_STEP_SETBITFRAMING_WRITE,
	M_RC522_TOCARD_STEP_READIRQREG,
	M_RC522_TOCARD_STEP_CHECKIRQREG,
	M_RC522_TOCARD_STEP_CLEARBITFRAMING_READ,
	M_RC522_TOCARD_STEP_CLEARBITFRAMING_WRITE,
	M_RC522_TOCARD_STEP_READERRORREG,
	M_RC522_TOCARD_STEP_READFIFOLEVEL,
	M_RC522_TOCARD_STEP_READCONTROLREG,
	M_RC522_TOCARD_STEP_CHECKLASTBITS,
	M_RC522_TOCARD_STEP_READFIFODATA_ONE,
	M_RC522_TOCARD_STEP_READFIFODATA_TWO
}e_rc522_toCard_step_typedef;

typedef struct {
	e_rc522_readCardSteps_typedef   step;
	e_rc522_toCard_step_typedef     toCardStep;

	uint8_t                         status;
	uint8_t                         backData[MAX_LEN];
	uint8_t                         backLen;

	uint8_t                         tagType[2];
	uint8_t                         checkID[5];
	uint8_t                         checkIDPrev[5];

	uint8_t                         irqEn;
	uint8_t                         waitIrq;

	uint8_t                         n;
	uint8_t                         ifnSet;
	uint8_t                         i;
	uint8_t                         lastBits;
	uint8_t                         serNumCheck;
	uint8_t                         verificationTries;

	uint8_t                         txData[2];
	uint8_t                         rxData[2];
	uint8_t                         readData;
	_Bool                           DMATxResponse;
	_Bool                           DMATxRxResponse;
	uint16_t                        validationTimeOut;
}s_rc522_DMA_structure;
s_rc522_DMA_structure s_rc522_DMA = {0};

typedef struct {
	uint32_t 					     timeConf;
	uint32_t 						 onTime;
	uint32_t 						 readingTries;
	uint32_t 						 detectionTimeoutTicker;
	uint32_t 						 detectionFailsafeTimeoutTicker;
	uint32_t 						 detectionMaximumValidTimeoutTicker;
	uint8_t							 checkID[M_RC522_CARDID_MAXLEN];
	uint8_t							 checkIDPREV[M_RC522_CARDID_MAXLEN];
}s_rc522_internal_structure;
static s_rc522_internal_structure s_rc522_internal = {0};

s_rc522_structure s_rc522 = {0};
void RC522_init()
{
	/************************************************************/
	if (!s_rc522.s_config.detectionTimeout)          s_rc522.s_config.detectionTimeout 		    = M_RC522_TIMING_NOTDETECTED;
	if (!s_rc522.s_config.detectionFailsafeTimeout)  s_rc522.s_config.detectionFailsafeTimeout  = M_RC522_TIMING_TIMEOUT;
	if (!s_rc522.s_config.confirmationTries)         s_rc522.s_config.confirmationTries         = M_RC522_VALIDATION_TRYCOUNT;
	if (!s_rc522.s_config.detectionMaximumValidTime) s_rc522.s_config.detectionMaximumValidTime = M_RC522_TIMING_CARDVALID;

}

void RC522_startDetection(void)
{
	if (!s_rc522.s_status.s_detection.isModuleOn)
	{
		memset(&(s_rc522_DMA), RESET, sizeof(s_rc522_DMA_structure));
		memset(&(s_rc522_internal), RESET, sizeof(s_rc522_internal_structure));

		HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_SET);
		HAL_GPIO_WritePin(MFRC522_RST_GPIO_Port, MFRC522_RST_Pin, GPIO_PIN_SET);
		HAL_Delay(2);
		s_rc522.s_status.s_detection.isModuleOn = SET;

		Module_init();

		/* Initialization Complete */
		s_rc522.s_status.s_detection.isDetectionEnabled = SET; // Enabling the RFID Module
		s_rc522.s_status.s_detection.isModuleOn = SET;
	}
}

void RC522_run(void)
{

	if (s_rc522.s_status.s_detection.isInitialized && s_rc522.s_status.s_detection.isModuleOn)
	{
		ReadRFID(); // reading the cards in case we'd initialized the module beforehand
	}
}



void RC522_tick()
{
	s_rc522_internal.onTime++;
	s_rc522_internal.detectionTimeoutTicker++;
	s_rc522_internal.detectionFailsafeTimeoutTicker++;
	s_rc522_internal.detectionMaximumValidTimeoutTicker++;
	s_rc522_DMA.validationTimeOut++;
}


void Module_init(void)
{
	MFRC522_Init();

	s_rc522.s_status.s_detection.isInitialized = SET;

	s_rc522_DMA.irqEn = 0x77;
	s_rc522_DMA.waitIrq = 0x30;
}




HAL_StatusTypeDef RC522_toCard_nonBlocking (uint8_t command, uint8_t* sendData, uint8_t sendLen, uint8_t* backData, uint8_t* backLen){

	static e_rc522_toCard_step_typedef toCardSteps = M_RC522_TOCARD_STEP_WRITEIENREG;
	if (rfidOFFtoCard)
	{
		rfidOFFtoCard = RESET;
		toCardSteps = M_RC522_TOCARD_STEP_WRITEIENREG;
	}
	switch (toCardSteps)
	{
	case M_RC522_TOCARD_STEP_WRITEIENREG:
		s_rc522_DMA.status = MI_ERR;
		if (RC522_Write(CommIEnReg, (s_rc522_DMA.irqEn | 0x80)) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_CLEARIRQREG_READ:
		if (RC522_Read(CommIrqReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_CLEARIRQREG_WRITE:
		if (RC522_Write(CommIrqReg, (s_rc522_DMA.readData & (~0x80))) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_SETFIFOLEVEL_READ:
		if (RC522_Read(FIFOLevelReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_SETFIFOLEVEL_WRITE:
		if (RC522_Write(FIFOLevelReg, (s_rc522_DMA.readData | 0x80)) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_WRITECOMMANDREGIDLE:
		s_rc522_DMA.i = 0;
		if (RC522_Write(CommandReg, PCD_IDLE) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_WRITEFIFODATAREG:
		if (s_rc522_DMA.i < sendLen)
		{
			if (RC522_Write(FIFODataReg, sendData[s_rc522_DMA.i]) == HAL_OK)
			{
				s_rc522_DMA.i++;
			}
			break;
		}
		toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_WRITECOMMANDREG:
		if (RC522_Write(CommandReg, command) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_SETBITFRAMING_READ:
		if (RC522_Read(BitFramingReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_SETBITFRAMING_WRITE:
		if (RC522_Write(BitFramingReg, (s_rc522_DMA.readData | 0x80)) == HAL_OK) toCardSteps++;
		s_rc522_DMA.i = 0;
		break;

	case M_RC522_TOCARD_STEP_READIRQREG:
		if (RC522_Read(CommIrqReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_CHECKIRQREG:
		s_rc522_DMA.n = s_rc522_DMA.readData;
		if (!(s_rc522_DMA.n & 0x01) && !(s_rc522_DMA.n & s_rc522_DMA.waitIrq))
		{
			if (++s_rc522_DMA.i > 25)
			{
				toCardSteps = RESET;
				return HAL_ERROR;
			}
			toCardSteps = M_RC522_TOCARD_STEP_READIRQREG;
			break;
		}
		toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_CLEARBITFRAMING_READ:
		if (RC522_Read(BitFramingReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_CLEARBITFRAMING_WRITE:
		if (RC522_Write(BitFramingReg, (s_rc522_DMA.readData & (~0x80))) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_READERRORREG:
		if (RC522_Read(ErrorReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_READFIFOLEVEL:
		if ((s_rc522_DMA.readData & 0x1B))
		{
			s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD;
			toCardSteps = RESET;
			break;
			// ERROR
		}
		s_rc522_DMA.status = MI_OK;
		if (s_rc522_DMA.n & s_rc522_DMA.irqEn & 0x01)
		{
			s_rc522_DMA.status = MI_NOTAGERR;
		}
		if (RC522_Read(FIFOLevelReg, &s_rc522_DMA.readData) == HAL_OK)	toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_READCONTROLREG:
		if (!s_rc522_DMA.ifnSet)
		{
			s_rc522_DMA.n = s_rc522_DMA.readData;
			s_rc522_DMA.ifnSet = SET;
		}
		if (RC522_Read(ControlReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_CHECKLASTBITS:
		s_rc522_DMA.ifnSet = RESET;
		s_rc522_DMA.lastBits = (s_rc522_DMA.readData & 0x07);
		if (s_rc522_DMA.lastBits)
		{
			*backLen = ((s_rc522_DMA.n - 1)*8) + s_rc522_DMA.lastBits;
		}
		else
		{
			*backLen = (s_rc522_DMA.n)*8;
		}

		if (s_rc522_DMA.n == 0)           s_rc522_DMA.n = 1;
		if (s_rc522_DMA.n > MAX_LEN) s_rc522_DMA.n = MAX_LEN;

		s_rc522_DMA.i = 0;
		toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_READFIFODATA_ONE:
		if (RC522_Read(FIFODataReg, &s_rc522_DMA.readData) == HAL_OK) toCardSteps++;
		break;

	case M_RC522_TOCARD_STEP_READFIFODATA_TWO:
		if (s_rc522_DMA.i < s_rc522_DMA.n)
		{
			backData[s_rc522_DMA.i] = s_rc522_DMA.readData;
			s_rc522_DMA.i++;
			toCardSteps = M_RC522_TOCARD_STEP_READFIFODATA_ONE;
			break;
		}
		else
		{
			s_rc522_DMA.step++;
			toCardSteps = RESET;
			return HAL_OK;
		}

		break;
	}
	return HAL_BUSY;
}



void ReadRFID(void)
{

	HAL_StatusTypeDef result = HAL_ERROR;
	switch (s_rc522_DMA.step)
	{
	case M_RC522_READSTEP_FINDCARD:
		if(RC522_Write(BitFramingReg, 0x07) == HAL_OK){
			s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD_PCDTRANSCEIVE;
			s_rc522_DMA.backData[0] = PICC_REQIDL;
		}
		break;

	case M_RC522_READSTEP_FINDCARD_PCDTRANSCEIVE:

		result = RC522_toCard_nonBlocking(PCD_TRANSCEIVE, s_rc522_DMA.backData, 1, s_rc522_DMA.backData, &s_rc522_DMA.backLen);
		if(result == HAL_OK){
			s_rc522_DMA.step = M_RC522_READSTEPS_FINDCARD_CHECKTAGTYPE;
		}else if (result == HAL_ERROR) {
			s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD;
		}
		break;

	case M_RC522_READSTEPS_FINDCARD_CHECKTAGTYPE:
		if ((s_rc522_DMA.backLen != 0x10) || (s_rc522_DMA.status != MI_OK))
		{
			s_rc522_DMA.step = RESET;
			memset(s_rc522_DMA.backData, RESET, sizeof(s_rc522_DMA.backData));
			memset(s_rc522_DMA.tagType, RESET, sizeof(s_rc522_DMA.tagType));
			s_rc522_DMA.backLen = RESET;
			memset(s_rc522_DMA.checkID, RESET, sizeof(s_rc522_DMA.checkID));
			// RESET CHECK ID
			break;
		}
		memcpy(s_rc522_DMA.tagType, s_rc522_DMA.backData, sizeof(s_rc522_DMA.tagType));
		memset(s_rc522_DMA.backData, RESET, sizeof(s_rc522_DMA.backData));
		s_rc522_DMA.step = M_RC522_READSTEP_GETID;
		break;

	case M_RC522_READSTEP_GETID:
		if(RC522_Write(BitFramingReg, 0x00) == HAL_OK){
			s_rc522_DMA.step = M_RC522_READSTEP_GETID_PCDTRANSCEIVE;
			s_rc522_DMA.backData[0] = PICC_ANTICOLL;
			s_rc522_DMA.backData[1] = 0x20;
		}
		break;

	case M_RC522_READSTEP_GETID_PCDTRANSCEIVE:
		result = RC522_toCard_nonBlocking(PCD_TRANSCEIVE, s_rc522_DMA.backData, 2, s_rc522_DMA.backData, &s_rc522_DMA.backLen);
		if(result == HAL_OK){
			s_rc522_DMA.step = M_RC522_READSTEPS_CHECKID;
		}else if (result == HAL_ERROR) {
			s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD;
		}
		break;

	case M_RC522_READSTEPS_CHECKID:
		if (s_rc522_DMA.status == MI_OK)
		{
			s_rc522_DMA.serNumCheck = 0;
			for (s_rc522_DMA.i = 0; s_rc522_DMA.i < 4; s_rc522_DMA.i++)
			{
				s_rc522_DMA.serNumCheck ^= s_rc522_DMA.backData[s_rc522_DMA.i];
			}
			if (s_rc522_DMA.serNumCheck != s_rc522_DMA.backData[4])
			{
				// ERROR / RESET
				s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD;
				memset(s_rc522_DMA.backData, RESET, sizeof(s_rc522_DMA.backData));
				// RESET CHECKIDPREV
				memset(s_rc522_DMA.checkIDPrev, RESET, sizeof(s_rc522_DMA.checkIDPrev));
				s_rc522_DMA.verificationTries = RESET;
				break;
			}
			else
			{
				// OK
				memcpy(s_rc522_DMA.checkID, s_rc522_DMA.backData, sizeof(s_rc522_DMA.checkID));
				s_rc522_DMA.step = M_RC522_READSTEP_VERIFYID;
				break;
			}
		}
		s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD;
		memset(s_rc522_DMA.backData, RESET, sizeof(s_rc522_DMA.backData));
		// RESET CHECKIDPREV
		memset(s_rc522_DMA.checkIDPrev, RESET, sizeof(s_rc522_DMA.checkIDPrev));
		break;

	case M_RC522_READSTEP_VERIFYID:
		if (memcmp(s_rc522_DMA.checkIDPrev, s_rc522_DMA.checkID, sizeof(s_rc522_DMA.checkID)))
		{
			memcpy(s_rc522_DMA.checkIDPrev, s_rc522_DMA.checkID, sizeof(s_rc522_DMA.checkID));
			s_rc522.s_status.s_IDStatus.isIDValid = RESET;
			s_rc522_DMA.verificationTries = RESET;
		}
		else
		{
			if (!s_rc522.s_status.s_IDStatus.isIDValid)
			{
				if (++s_rc522_DMA.verificationTries > M_RC522_VALIDATION_TRYCOUNT)
				{
					// VALID CARD
					s_rc522_DMA.validationTimeOut = RESET;
					s_rc522.s_status.s_IDStatus.isIDValid = SET;
					s_rc522.s_status.s_IDStatus.isUnreadWaiting = SET;
					memcpy(s_rc522.s_status.s_IDStatus.cardID, s_rc522_DMA.checkID, sizeof(s_rc522_DMA.checkID));
					break;
				}
			}
			else if (s_rc522_DMA.validationTimeOut > M_RC522_VALIDATION_TIMEOUT)
			{
				s_rc522.s_status.s_IDStatus.isIDValid = RESET;
				s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD;
				break;
			}
		}
		break;

	default:
		s_rc522_DMA.step = M_RC522_READSTEP_FINDCARD;
		break;
	}
}


void ResetRFID(void)
{
	memset(&s_rc522_internal, RESET, sizeof(s_rc522_internal));
	memset(&s_rc522.s_status, RESET, sizeof(s_rc522.s_status));
	Module_deinit();
}


void Module_deinit(void)
{
	s_rc522.s_status.s_detection.isInitialized = RESET;
	s_rc522.s_status.s_detection.isDetectionEnabled = RESET;
	s_rc522.s_status.s_detection.isModuleOn = RESET;

	rfidOFFread = SET;
	rfidOFFwrite = SET;
	rfidOFFtoCard = SET;
}


void RC522_startTransmit(void)
{
	HAL_GPIO_WritePin(MFRC522_CS_PORT,MFRC522_CS_PIN,GPIO_PIN_RESET);
}

void RC522_endTransmit(void)
{
	HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_SET);
}

HAL_StatusTypeDef RC522_Write(uint8_t addr, uint8_t val)
{
	HAL_StatusTypeDef result = HAL_BUSY;
	static e_rc522_read_write_step_typedef readWriteSteps = M_RC522_READWRITE_STEP_ONE;
	if (rfidOFFwrite)
	{
		rfidOFFwrite = RESET;
		readWriteSteps = M_RC522_READWRITE_STEP_ONE;
	}
	switch (readWriteSteps) {
	case M_RC522_READWRITE_STEP_ONE:
		s_rc522_DMA.DMATxResponse = RESET;
		s_rc522_DMA.txData[0] = ((addr << 1) & 0x7E);
		s_rc522_DMA.txData[1] = val;
		RC522_startTransmit();
		if(HAL_SPI_Transmit_DMA(&M_CONFIG_RC522_SPIHANDLE, s_rc522_DMA.txData, 2) == HAL_OK){
			readWriteSteps = M_RC522_READWRITE_STEP_TWO;
		}
		break;

	case M_RC522_READWRITE_STEP_TWO:
		if(s_rc522_DMA.DMATxResponse == RESET){
			break;
		}
		s_rc522_DMA.DMATxResponse = RESET;
		readWriteSteps = M_RC522_READWRITE_STEP_ONE;
		result = HAL_OK;
		break;


	default:
		readWriteSteps = M_RC522_READWRITE_STEP_ONE;
		break;
	}
	return result;
}

HAL_StatusTypeDef RC522_Read(uint8_t addr, uint8_t* readValue)
{
	HAL_StatusTypeDef result = HAL_BUSY;
	static e_rc522_read_write_step_typedef readReadSteps = M_RC522_READWRITE_STEP_ONE;

	if (rfidOFFread)
	{
		rfidOFFread = RESET;
		readReadSteps = M_RC522_READWRITE_STEP_ONE;
	}
	switch (readReadSteps) {
	case M_RC522_READWRITE_STEP_ONE:
		s_rc522_DMA.DMATxResponse = RESET;
		s_rc522_DMA.txData[0] = (((addr << 1) & 0x7E) | 0x80);
		s_rc522_DMA.txData[1] = 0x00;
		RC522_startTransmit();
		if(HAL_SPI_TransmitReceive_DMA(&M_CONFIG_RC522_SPIHANDLE, s_rc522_DMA.txData, s_rc522_DMA.rxData, 2) == HAL_OK){
			readReadSteps = M_RC522_READWRITE_STEP_TWO;
		}
		break;

	case M_RC522_READWRITE_STEP_TWO:
		if(s_rc522_DMA.DMATxRxResponse == RESET){
			break;
		}
		s_rc522_DMA.DMATxRxResponse = RESET;
		*readValue = s_rc522_DMA.rxData[1];
		readReadSteps = M_RC522_READWRITE_STEP_ONE;
		result = HAL_OK;
		break;

	default:
		readReadSteps = M_RC522_READWRITE_STEP_ONE;
		break;
	}
	return result;
}


void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &M_CONFIG_RC522_SPIHANDLE)
	{
		RC522_endTransmit();
		if (s_rc522.s_status.s_detection.isModuleOn)
		{
			s_rc522_DMA.DMATxRxResponse = SET;
		}
	}
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &M_CONFIG_RC522_SPIHANDLE)
	{
		RC522_endTransmit();
		if (s_rc522.s_status.s_detection.isModuleOn)
		{
			s_rc522_DMA.DMATxResponse = SET;
		}
	}
}




