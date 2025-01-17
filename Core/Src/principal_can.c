/*
 * principal_can.c
 *
 *  Created on: Dec 30, 2021
 *      Author: Rodolfo
 */

#include "principal.h"

static void Tx_Analog_1_4(CAN_HandleTypeDef* hcan);
static void Tx_Analog_5_8(CAN_HandleTypeDef* hcan);
static void Tx_Analog_9_12(CAN_HandleTypeDef* hcan);
static void Tx_RTC(CAN_HandleTypeDef* hcan);
static void Tx_Verify(CAN_HandleTypeDef* hcan);
static void Tx_Beacon(CAN_HandleTypeDef* hcan);
static void Save_ECU(CAN_HandleTypeDef* hcan);
static void Save_PDM(CAN_HandleTypeDef* hcan);

void Principal_Verify_LEDs()
{
	verifyADC = 0;

	for(uint8_t i = 0; i < NBR_OF_CHANNELS; i++)
		if(adcBuffer[i] > ADC_THRESHOLD)
			verifyADC |= (1 << i);

	HAL_GPIO_TogglePin(LED_OK);

	if(flagDatalogger == DL_SAVE)
		HAL_GPIO_WritePin(LED_DATALOGGER, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(LED_DATALOGGER, GPIO_PIN_RESET);

	if((verifyCAN & 1) == 1)
		HAL_GPIO_WritePin(LED_CAN_TX, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(LED_CAN_TX, GPIO_PIN_RESET);

	if((verifyCAN & 2) == 2)
		HAL_GPIO_WritePin(LED_CAN_RX, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(LED_CAN_RX, GPIO_PIN_RESET);

	return;
}

void Principal_CAN_Start(CAN_HandleTypeDef* hcan)
{
	CAN_FilterTypeDef sFilterConfig;
	uint32_t filter_id = 0, mask_id = 0;

	filter_id = CAN_DAQ_FILTER;
	mask_id = CAN_DAQ_MASK;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = filter_id >> 13;
	sFilterConfig.FilterIdLow = (filter_id << 3) & 0xFFF8;
	sFilterConfig.FilterMaskIdHigh = mask_id >> 13;
	sFilterConfig.FilterMaskIdLow = (mask_id << 3) & 0xFFF8;
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.FilterBank = 0;
	sFilterConfig.SlaveStartFilterBank = 14;

	HAL_CAN_ConfigFilter(hcan, &sFilterConfig);

	filter_id = CAN_CFG_FILTER;
	mask_id = CAN_CFG_MASK;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = filter_id >> 13;
	sFilterConfig.FilterIdLow = (filter_id << 3) & 0xFFF8;
	sFilterConfig.FilterMaskIdHigh = mask_id >> 13;
	sFilterConfig.FilterMaskIdLow = (mask_id << 3) & 0xFFF8;
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.FilterBank = 1;
	sFilterConfig.SlaveStartFilterBank = 15;

	HAL_CAN_ConfigFilter(hcan, &sFilterConfig);

	FT_CAN_FilterConfig(hcan, FT_Power_ECU, 2, CAN_RX_FIFO0);
	FT_CAN_FilterConfig(hcan, FT_WBO2_Nano, 3, CAN_RX_FIFO0);
	PDM_CAN_FilterConfig(hcan, 4, CAN_RX_FIFO0);

	HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

	HAL_CAN_Start(hcan);
}

void Principal_Transmit_Msg(CAN_HandleTypeDef* hcan, uint8_t msg_number)
{
	switch(msg_number)
	{
		case ANALOG_1_4:
			Tx_Analog_1_4(hcan);
			break;

		case ANALOG_5_8:
			Tx_Analog_5_8(hcan);
			break;

		case ANALOG_9_12:
			Tx_Analog_9_12(hcan);
			break;

		case VERIFY_MSG:
			Tx_Verify(hcan);
			break;

		case RTC_MSG:
			Tx_RTC(hcan);
			break;

		case BEACON_MSG:
			Tx_Beacon(hcan);
			break;

		case ECU_SAVE:
			Save_ECU(hcan);
			break;

		case PDM_SAVE:
			Save_PDM(hcan);
			break;

		default:
			return;
	}
}

static void Tx_Analog_1_4(CAN_HandleTypeDef* hcan)
{
	if((verifyADC & 0x000f) == 0x0000)
	{
		accCAN[ANALOG_1_4] = 0;
		return;
	}

	txHeader.IDE = CAN_ID_STD;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.TransmitGlobalTime = DISABLE;
	txHeader.StdId = FIRST_ID + ANALOG_1_4;
	txHeader.DLC = 8;

	txData[0] = adcBuffer[0] >> 8;
	txData[1] = adcBuffer[0] & 0xff;
	txData[2] = adcBuffer[1] >> 8;
	txData[3] = adcBuffer[1] & 0xff;
	txData[4] = adcBuffer[2] >> 8;
	txData[5] = adcBuffer[2] & 0xff;
	txData[6] = adcBuffer[3] >> 8;
	txData[7] = adcBuffer[3] & 0xff;

	if(flagDatalogger == DL_SAVE)
		Principal_Datalogger_Save_Buffer(hcan, txHeader.StdId, txHeader.DLC, txData);

	if((accCAN[ANALOG_1_4] >= perCAN[ANALOG_1_4]) && (perCAN[ANALOG_1_4] != MSG_DISABLED))
	{
		accCAN[ANALOG_1_4] -= perCAN[ANALOG_1_4];

		if(HAL_CAN_AddTxMessage(hcan, &txHeader, txData, &txMailbox) == HAL_OK)
			verifyCAN |= 1;
		else
			verifyCAN &= 0x02;

		//Wait Transmission finish
		for(uint8_t i = 0; HAL_CAN_GetTxMailboxesFreeLevel(hcan) != 3 && i < 3; i++);
	}

	return;
}

static void Tx_Analog_5_8(CAN_HandleTypeDef* hcan)
{
	if((verifyADC & 0x00f0) == 0x0000)
	{
		accCAN[ANALOG_5_8] = 0;
		return;
	}

	txHeader.IDE = CAN_ID_STD;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.TransmitGlobalTime = DISABLE;
	txHeader.StdId = FIRST_ID + ANALOG_5_8;
	txHeader.DLC = 8;

	txData[0] = adcBuffer[4] >> 8;
	txData[1] = adcBuffer[4] & 0xff;
	txData[2] = adcBuffer[5] >> 8;
	txData[3] = adcBuffer[5] & 0xff;
	txData[4] = adcBuffer[6] >> 8;
	txData[5] = adcBuffer[6] & 0xff;
	txData[6] = adcBuffer[7] >> 8;
	txData[7] = adcBuffer[7] & 0xff;

	if(flagDatalogger == DL_SAVE)
		Principal_Datalogger_Save_Buffer(hcan, txHeader.StdId, txHeader.DLC, txData);

	if((accCAN[ANALOG_5_8] >= perCAN[ANALOG_5_8]) && (perCAN[ANALOG_5_8] != MSG_DISABLED))
	{
		accCAN[ANALOG_5_8] -= perCAN[ANALOG_5_8];

		if(HAL_CAN_AddTxMessage(hcan, &txHeader, txData, &txMailbox) == HAL_OK)
			verifyCAN |= 1;
		else
			verifyCAN &= 0x02;

		//Wait Transmission finish
		for(uint8_t i = 0; HAL_CAN_GetTxMailboxesFreeLevel(hcan) != 3 && i < 3; i++);
	}

	return;
}

static void Tx_Analog_9_12(CAN_HandleTypeDef* hcan)
{
	if((verifyADC & 0x0f00) == 0x0000)
	{
		accCAN[ANALOG_9_12] = 0;
		return;
	}

	txHeader.IDE = CAN_ID_STD;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.TransmitGlobalTime = DISABLE;
	txHeader.StdId = FIRST_ID + ANALOG_9_12;
	txHeader.DLC = 8;

	if((verifyADC & 0x0f00) == 0)
		return;

	txData[0] = adcBuffer[8] >> 8;
	txData[1] = adcBuffer[8] & 0xff;
	txData[2] = adcBuffer[9] >> 8;
	txData[3] = adcBuffer[9] & 0xff;
	txData[4] = adcBuffer[10] >> 8;
	txData[5] = adcBuffer[10] & 0xff;
	txData[6] = adcBuffer[11] >> 8;
	txData[7] = adcBuffer[11] & 0xff;

	if(flagDatalogger == DL_SAVE)
		Principal_Datalogger_Save_Buffer(hcan, txHeader.StdId, txHeader.DLC, txData);

	if((accCAN[ANALOG_9_12] >= perCAN[ANALOG_9_12]) && (perCAN[ANALOG_9_12] != MSG_DISABLED))
	{
		accCAN[ANALOG_9_12] -= perCAN[ANALOG_9_12];

		if(HAL_CAN_AddTxMessage(hcan, &txHeader, txData, &txMailbox) == HAL_OK)
			verifyCAN |= 1;
		else
			verifyCAN &= 0x02;

		//Wait Transmission finish
		for(uint8_t i = 0; HAL_CAN_GetTxMailboxesFreeLevel(hcan) != 3 && i < 3; i++);
	}

	return;
}

static void Tx_RTC(CAN_HandleTypeDef* hcan)
{
	txHeader.IDE = CAN_ID_STD;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.TransmitGlobalTime = DISABLE;
	txHeader.StdId = FIRST_ID + RTC_MSG;
	txHeader.DLC = 6;

	HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);

	txData[0] = rtcDate.Year;
	txData[1] = rtcDate.Month;
	txData[2] = rtcDate.Date;
	txData[3] = rtcTime.Hours;
	txData[4] = rtcTime.Minutes;
	txData[5] = rtcTime.Seconds;


	if(flagDatalogger == DL_SAVE)
		Principal_Datalogger_Save_Buffer(hcan, txHeader.StdId, txHeader.DLC, txData);

	if((accCAN[RTC_MSG] >= perCAN[RTC_MSG]) && (perCAN[RTC_MSG] != MSG_DISABLED))
	{
		accCAN[RTC_MSG] -= perCAN[RTC_MSG];

		if(HAL_CAN_AddTxMessage(hcan, &txHeader, txData, &txMailbox) == HAL_OK)
			verifyCAN |= 1;
		else
			verifyCAN &= 0x02;

		//Wait Transmission finish
		for(uint8_t i = 0; HAL_CAN_GetTxMailboxesFreeLevel(hcan) != 3 && i < 3; i++);
	}

	return;
}

static void Tx_Verify(CAN_HandleTypeDef* hcan)
{
	txHeader.IDE = CAN_ID_STD;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.TransmitGlobalTime = DISABLE;
	txHeader.StdId = FIRST_ID + VERIFY_MSG;
	txHeader.DLC = 8;

	txData[0] = verifyADC & 0xff;
	txData[1] = (verifyADC >> 8) & 0x0f;

	if(flagDatalogger == DL_SAVE)
	{
		txData[1] |= (1 << 4);
		HAL_GPIO_WritePin(OUT0_GPIO_Port, OUT0_Pin, GPIO_PIN_RESET);
	}
	else
		HAL_GPIO_WritePin(OUT0_GPIO_Port, OUT0_Pin, GPIO_PIN_SET);

	if(flagRTC == RTC_OK)
		txData[1] |= (1 << 5);

	__FREQ_TO_BUFFER(txData[2], perMsg[ANALOG_1_4]);
	__FREQ_TO_BUFFER(txData[3], perMsg[ANALOG_5_8]);
	__FREQ_TO_BUFFER(txData[4], perMsg[ANALOG_9_12]);
	__FREQ_TO_BUFFER(txData[5], perMsg[RTC_MSG]);
	__FREQ_TO_BUFFER(txData[6], perMsg[PDM_SAVE]);
	__FREQ_TO_BUFFER(txData[7], perMsg[ECU_SAVE]);

	if(flagDatalogger == DL_SAVE)
		Principal_Datalogger_Save_Buffer(hcan, txHeader.StdId, txHeader.DLC, txData);

	if((accCAN[VERIFY_MSG] >= perCAN[VERIFY_MSG]) && (perCAN[VERIFY_MSG] != MSG_DISABLED))
	{
		accCAN[VERIFY_MSG] -= perCAN[VERIFY_MSG];

		if(HAL_CAN_AddTxMessage(hcan, &txHeader, txData, &txMailbox) == HAL_OK)
			verifyCAN |= 1;
		else
			verifyCAN &= 0x02;

		//Wait Transmission finish
		for(uint8_t i = 0; HAL_CAN_GetTxMailboxesFreeLevel(hcan) != 3 && i < 3; i++);
	}

	return;
}

static void Tx_Beacon(CAN_HandleTypeDef* hcan)
{
	uint16_t buffer[3];

	txHeader.IDE = CAN_ID_STD;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.TransmitGlobalTime = DISABLE;
	txHeader.StdId = BEACON_ID;
	txHeader.DLC = 5;

	buffer[0] = accLap / 60000;
	buffer[1] = accLap / 1000;
	buffer[2] = accLap % 1000;

	accLap = 0;

	txData[0] = lapNumber;
	txData[1] = buffer[0] & 0xff;
	txData[2] = buffer[1] & 0xff;
	txData[3] = buffer[2] >> 8;
	txData[4] = buffer[2] & 0xff;

	if(flagDatalogger == DL_SAVE)
		Principal_Datalogger_Save_Buffer(hcan, txHeader.StdId, txHeader.DLC, txData);

	if((accCAN[BEACON_MSG] >= perCAN[BEACON_MSG]) && (perCAN[BEACON_MSG] != MSG_DISABLED))
	{
		accCAN[BEACON_MSG] -= perCAN[BEACON_MSG];

		if(HAL_CAN_AddTxMessage(hcan, &txHeader, txData, &txMailbox) == HAL_OK)
			verifyCAN |= 1;
		else
			verifyCAN &= 0x02;

		//Wait Transmission finish
		for(uint8_t i = 0; HAL_CAN_GetTxMailboxesFreeLevel(hcan) != 3 && i < 3; i++);
	}

	return;
}

static void Save_ECU(CAN_HandleTypeDef* hcan)
{
	uint8_t id = 0;
	uint8_t length = 0;
	uint8_t buffer[8];

	if(flagDatalogger != DL_SAVE)
		return;

	id = ECU_FIRST_ID;
	length = 8;

	buffer[0] = ecuData.rpm >> 8;
	buffer[1] = ecuData.rpm & 0xff;
	buffer[2] = ecuData.tps >> 8;
	buffer[3] = ecuData.tps & 0xff;
	buffer[4] = ecuData.iat >> 8;
	buffer[5] = ecuData.iat & 0xff;
	buffer[6] = ecuData.ect >> 8;
	buffer[7] = ecuData.ect & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = ECU_FIRST_ID + 1;
	length = 8;

	buffer[0] = ecuData.map >> 8;
	buffer[1] = ecuData.map & 0xff;
	buffer[2] = ecuData.fuel_pressure >> 8;
	buffer[3] = ecuData.fuel_pressure & 0xff;
	buffer[4] = ecuData.oil_pressure >> 8;
	buffer[5] = ecuData.oil_pressure & 0xff;
	buffer[6] = ecuData.coolant_pressure >> 8;
	buffer[7] = ecuData.coolant_pressure & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = ECU_FIRST_ID + 2;
	length = 8;

	buffer[0] = ecuData.lambda >> 8;
	buffer[1] = ecuData.lambda & 0xff;
	buffer[2] = ecuData.oil_temperature >> 8;
	buffer[3] = ecuData.oil_temperature & 0xff;
	buffer[4] = ecuData.wheel_speed_fl;
	buffer[5] = ecuData.wheel_speed_fr;
	buffer[6] = ecuData.wheel_speed_rl;
	buffer[7] = ecuData.wheel_speed_rr;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = ECU_FIRST_ID + 3;
	length = 8;

	buffer[0] = ecuData.battery_voltage >> 8;
	buffer[1] = ecuData.battery_voltage & 0xff;
	buffer[2] = ecuData.fuel_flow_total >> 8;
	buffer[3] = ecuData.fuel_flow_total & 0xff;
	buffer[4] = ecuData.gear & 0xff;
	buffer[5] = ecuData.electro_fan & 0xff;
	buffer[6] = ecuData.injection_bank_a_time >> 8;
	buffer[7] = ecuData.injection_bank_a_time & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = ECU_FIRST_ID + 4;
	length = 2;

	buffer[0] = ecuData.lambda_correction >> 8;
	buffer[1] = ecuData.lambda_correction & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = ECU_FIRST_ID + 5;
	length = 8;

	buffer[0] = ecuData.accel_long >> 8;
	buffer[1] = ecuData.accel_long & 0xff;
	buffer[2] = ecuData.accel_lat >> 8;
	buffer[3] = ecuData.accel_lat & 0xff;
	buffer[4] = ecuData.yaw_rate_pitch >> 8;
	buffer[5] = ecuData.yaw_rate_pitch & 0xff;
	buffer[6] = ecuData.yaw_rate_roll >> 8;
	buffer[7] = ecuData.yaw_rate_roll & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	return;
}

static void Save_PDM(CAN_HandleTypeDef* hcan)
{
	uint8_t id = 0, length = 0, buffer[8];

	if(flagDatalogger != DL_SAVE)
		return;

	id = PDM_FIRST_ID;
	length = 8;

	buffer[0] = pdmReadings.Current_Buffer[0] << 8;
	buffer[1] = pdmReadings.Current_Buffer[0] & 0xff;
	buffer[2] = pdmReadings.Current_Buffer[1] << 8;
	buffer[3] = pdmReadings.Current_Buffer[1] & 0xff;
	buffer[4] = pdmReadings.Current_Buffer[2] << 8;
	buffer[5] = pdmReadings.Current_Buffer[2] & 0xff;
	buffer[6] = pdmReadings.Current_Buffer[3] << 8;
	buffer[7] = pdmReadings.Current_Buffer[3] & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = PDM_FIRST_ID + 1;
	length = 8;

	buffer[0] = pdmReadings.Current_Buffer[4] << 8;
	buffer[1] = pdmReadings.Current_Buffer[4] & 0xff;
	buffer[2] = pdmReadings.Current_Buffer[5] << 8;
	buffer[3] = pdmReadings.Current_Buffer[5] & 0xff;
	buffer[4] = pdmReadings.Current_Buffer[6] << 8;
	buffer[5] = pdmReadings.Current_Buffer[6] & 0xff;
	buffer[6] = pdmReadings.Current_Buffer[7] << 8;
	buffer[7] = pdmReadings.Current_Buffer[7] & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = PDM_FIRST_ID + 2;
	length = 8;

	buffer[0] = pdmReadings.Current_Buffer[8] << 8;
	buffer[1] = pdmReadings.Current_Buffer[8] & 0xff;
	buffer[2] = pdmReadings.Current_Buffer[9] << 8;
	buffer[3] = pdmReadings.Current_Buffer[9] & 0xff;
	buffer[4] = pdmReadings.Current_Buffer[10] << 8;
	buffer[5] = pdmReadings.Current_Buffer[10] & 0xff;
	buffer[6] = pdmReadings.Current_Buffer[11] << 8;
	buffer[7] = pdmReadings.Current_Buffer[11] & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = PDM_FIRST_ID + 3;
	length = 8;

	buffer[0] = pdmReadings.Current_Buffer[12] << 8;
	buffer[1] = pdmReadings.Current_Buffer[12] & 0xff;
	buffer[2] = pdmReadings.Current_Buffer[13] << 8;
	buffer[3] = pdmReadings.Current_Buffer[13] & 0xff;
	buffer[4] = pdmReadings.Current_Buffer[14] << 8;
	buffer[5] = pdmReadings.Current_Buffer[14] & 0xff;
	buffer[6] = pdmReadings.Current_Buffer[15] << 8;
	buffer[7] = pdmReadings.Current_Buffer[15] & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = PDM_FIRST_ID + 4;
	length = 8;

	buffer[0] = pdmReadings.Tempetature_Buffer[0] << 8;
	buffer[1] = pdmReadings.Tempetature_Buffer[0] & 0xff;
	buffer[2] = pdmReadings.Tempetature_Buffer[1] << 8;
	buffer[3] = pdmReadings.Tempetature_Buffer[1] & 0xff;
	buffer[4] = pdmReadings.Tempetature_Buffer[2] << 8;
	buffer[5] = pdmReadings.Tempetature_Buffer[2] & 0xff;
	buffer[6] = pdmReadings.Tempetature_Buffer[3] << 8;
	buffer[7] = pdmReadings.Tempetature_Buffer[3] & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = PDM_FIRST_ID + 5;
	length = 8;

	buffer[0] = pdmReadings.Tempetature_Buffer[4] << 8;
	buffer[1] = pdmReadings.Tempetature_Buffer[4] & 0xff;
	buffer[2] = pdmReadings.Tempetature_Buffer[5] << 8;
	buffer[3] = pdmReadings.Tempetature_Buffer[5] & 0xff;
	buffer[4] = pdmReadings.Tempetature_Buffer[6] << 8;
	buffer[5] = pdmReadings.Tempetature_Buffer[6] & 0xff;
	buffer[6] = pdmReadings.Tempetature_Buffer[7] << 8;
	buffer[7] = pdmReadings.Tempetature_Buffer[7] & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = PDM_FIRST_ID + 6;
	length = 8;

	buffer[0] = pdmReadings.Duty_Cycle_Buffer[0] << 8;
	buffer[1] = pdmReadings.Duty_Cycle_Buffer[0] & 0xff;
	buffer[2] = pdmReadings.Duty_Cycle_Buffer[1] << 8;
	buffer[3] = pdmReadings.Duty_Cycle_Buffer[1] & 0xff;
	buffer[4] = pdmReadings.Duty_Cycle_Buffer[2] << 8;
	buffer[5] = pdmReadings.Duty_Cycle_Buffer[2] & 0xff;
	buffer[6] = pdmReadings.Duty_Cycle_Buffer[3] << 8;
	buffer[7] = pdmReadings.Duty_Cycle_Buffer[3] & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	id = PDM_FIRST_ID + 7;
	length = 4;

	buffer[0] = pdmReadings.Input_Voltage << 8;
	buffer[1] = pdmReadings.Input_Voltage & 0xff;
	buffer[2] = pdmReadings.Output_Verify << 8;
	buffer[3] = pdmReadings.Output_Verify & 0xff;

	Principal_Datalogger_Save_Buffer(hcan, id, length, buffer);

	return;
}
