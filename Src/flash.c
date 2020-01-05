/*
 * flash.c
 *
 *  Created on: Nov 24, 2019
 *      Author: Mateusz
 */

#include "stm32f4xx.h"
#define FLASH_ADDRESS 0x800C000


//static void flash_lock(void)
//{
//	SET_BIT(FLASH->CR, FLASH_CR_LOCK);

void Write_Flash(uint8_t data)
{
     HAL_FLASH_Unlock();
     __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGSERR );
     FLASH_Erase_Sector(FLASH_SECTOR_3, VOLTAGE_RANGE_3);
     HAL_FLASH_Program(TYPEPROGRAM_WORD, FLASH_ADDRESS, data);
     HAL_FLASH_Lock();
}
uint8_t Read_Flash_config(void)
{
	uint8_t* address = (uint8_t*)0x800C000;
	return *address;
}
