/*
 * principal_interrupts.c
 *
 *  Created on: 29 de dez de 2021
 *      Author: Rodolfo
 */

#include "principal.h"

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK)
	{
		verifyCAN |= 2;

		if((rxHeader.IDE == CAN_ID_STD) && ((rxHeader.StdId & CAN_DAQ_MASK) == CAN_DAQ_FILTER) && (flagDatalogger == DL_SAVE))
			Principal_Datalogger_Save_Buffer(hcan, rxHeader.StdId, rxHeader.DLC, rxData);

		else if(rxHeader.IDE == CAN_ID_EXT)
		{
			if((rxHeader.ExtId & CAN_CFG_MASK) == CAN_CFG_FILTER)
				Principal_Receive_Config(&hi2c1, rxData, rxHeader.DLC);

			else
			{
				PDM_CAN_Process_Data(&rxHeader, rxData, &pdmReadings);
				FT_CAN_ReceiveData(&rxHeader, rxData, &ecuData);
			}
		}
	}

	else
		verifyCAN &= 1;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	GPIO_PinState pinLevel;

	if(GPIO_Pin == EXTI0_Pin)
	{
		pinLevel = HAL_GPIO_ReadPin(EXTI0_GPIO_Port, EXTI0_Pin);

		if(((inputConfig & 0x01) == INPUT_BEACON_PIN_0)
				&& ((((inputConfig & 0x04) == INPUT_BEACON_FALLING_EDGE) && (pinLevel == GPIO_PIN_RESET))
				|| (((inputConfig & 0x04) == INPUT_BEACON_RISING_EDGE) && (pinLevel == GPIO_PIN_SET))))
			Principal_Beacon_Detect();

		if(((inputConfig & 0x02) == INPUT_DATALOGGER_PIN_0)
				&& ((((inputConfig & 0x08) == INPUT_DATALOGGER_FALLING_EDGE) && (pinLevel == GPIO_PIN_RESET))
				|| (((inputConfig & 0x08) == INPUT_DATALOGGER_RISING_EDGE) && (pinLevel == GPIO_PIN_SET))))
			Principal_Datalogger_Button();
	}

	if(GPIO_Pin == EXTI1_Pin)
	{
		pinLevel = HAL_GPIO_ReadPin(EXTI1_GPIO_Port, EXTI1_Pin);

		if(((inputConfig & 0x01) == INPUT_BEACON_PIN_1)
				&& ((((inputConfig & 0x04) == INPUT_BEACON_FALLING_EDGE) && (pinLevel == GPIO_PIN_RESET))
				|| (((inputConfig & 0x04) == INPUT_BEACON_RISING_EDGE) && (pinLevel == GPIO_PIN_SET))))
			Principal_Beacon_Detect();

		if(((inputConfig & 0x02) == INPUT_DATALOGGER_PIN_1)
				&& ((((inputConfig & 0x08) == INPUT_DATALOGGER_FALLING_EDGE) && (pinLevel == GPIO_PIN_RESET))
				|| (((inputConfig & 0x08) == INPUT_DATALOGGER_RISING_EDGE) && (pinLevel == GPIO_PIN_SET))))
			Principal_Datalogger_Button();
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == TIM7)
	{
		accDatalogger[DL_ACC_TIMING]++;
		accDatalogger[DL_ACC_TIMEOUT]++;
		if(accDatalogger[DL_ACC_COOLDOWN] > 0) accDatalogger[DL_ACC_COOLDOWN]--;

		accCAN[ANALOG_1_4]++;
		accCAN[ANALOG_5_8]++;
		accCAN[ANALOG_9_12]++;
		accCAN[RTC_MSG]++;
		accCAN[VERIFY_MSG]++;
		accCAN[BEACON_MSG]++;

		accMsg[ANALOG_1_4]++;
		accMsg[ANALOG_5_8]++;
		accMsg[ANALOG_9_12]++;
		accMsg[RTC_MSG]++;
		accMsg[VERIFY_MSG]++;
		accMsg[BEACON_MSG]++;
		accMsg[ECU_SAVE]++;
		accMsg[PDM_SAVE]++;
		accMsg[VERIFY_LEDS]++;

		accLap++;
	}
}
