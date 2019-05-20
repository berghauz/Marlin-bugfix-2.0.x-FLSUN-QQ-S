/**
 * Marlin 3D Printer Firmware
 *
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 * Copyright (c) 2016 Bob Cousins bobcousins42@googlemail.com
 * Copyright (c) 2015-2016 Nico Tonnhofer wurstnase.reprap@gmail.com
 * Copyright (c) 2016 Victor Perez victor_pv@hotmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * persistent_store_flash.cpp
 * HAL for stm32duino and compatible (STM32F1)
 * Implementation of EEPROM settings in SDCard
 */

#ifdef __STM32F1__

#include "../../inc/MarlinConfig.h"

#if BOTH(EEPROM_SETTINGS, FLASH_EEPROM_EMULATION)

#define EEPROM_ERASE		((uint16)0xFF)

#include "../shared/persistent_store_api.h"
#include <flash_stm32.h>
#include <EEPROM.h>

static uint8_t ram_eeprom[EEPROM_SIZE] __attribute__((aligned(4))) = {0};
static bool eeprom_changed = false;
bool firstWrite = false;

// Store settings in the last two pages
// Flash pages must be erased before writing, so keep track.
uint32_t pageBase = EEPROM_START_ADDRESS;

inline uint16 CheckPage(uint32 pageBase)
{
	uint32 pageEnd = pageBase + (uint32)EEPROM_PAGE_SIZE;

	// Page Status not EEPROM_ERASED
	if ((*(__IO uint16*)pageBase) != EEPROM_ERASED)
		return EEPROM_BAD_FLASH;
	for(pageBase += 4; pageBase < pageEnd; pageBase += 4)
		if ((*(__IO uint32*)pageBase) != 0xFFFFFFFF)	// Verify if slot is empty
			return EEPROM_BAD_FLASH;
	return EEPROM_OK;
}

bool PersistentStore::access_start() {
  
  if (CheckPage(EEPROM_PAGE0_BASE) != EEPROM_OK || CheckPage(EEPROM_PAGE1_BASE) != EEPROM_OK) {
  	for (uint32_t i = 0; i < EEPROM_SIZE; i++) {
		uint8* accessPoint = (uint8*)pageBase + i;
		uint8_t c = *accessPoint;
		ram_eeprom[i] = c;
	}
  } else {                                          
	for (int i = 0; i < EEPROM_SIZE; i++) {
		ram_eeprom[i] = EEPROM_ERASE;
	}
  }
  eeprom_changed = false;
  return true;
}

bool PersistentStore::access_finish() {
  FLASH_Status status;
  if (eeprom_changed) {

    FLASH_Unlock();

    status = FLASH_ErasePage(EEPROM_PAGE0_BASE);
    if (status != FLASH_COMPLETE) return true;
    status = FLASH_ErasePage(EEPROM_PAGE1_BASE);
    if (status != FLASH_COMPLETE) return true;	
  }

  int i = 0;
  int wordsToWrite = EEPROM_SIZE / sizeof(uint16_t);
  uint16_t* wordBuffer = (uint16_t *)ram_eeprom;
  while (wordsToWrite) {
    status = FLASH_ProgramHalfWord(pageBase + (i *2), wordBuffer[i]);
    if (status != FLASH_COMPLETE) return true;
    wordsToWrite--;
    i++;
  }

  FLASH_Lock();

  eeprom_changed = false;
  return true;
}

bool PersistentStore::read_data(int &pos, uint8_t* value, size_t size, uint16_t *crc, const bool writing/*=true*/) {

  const uint8_t * const buff = writing ? &value[0] : &ram_eeprom[pos];
  if (writing) for (size_t i = 0; i < size; i++) value[i] = ram_eeprom[pos + i];
  crc16(crc, buff, size);
  pos += size;

  return false;
}

bool PersistentStore::write_data(int &pos, const uint8_t *value, size_t size, uint16_t *crc) {

  for (size_t i = 0; i < size; i++) ram_eeprom[pos + i] = value[i];
  eeprom_changed = true;
  crc16(crc, value, size);
  pos += size;

  return false;
}

size_t PersistentStore::capacity() { return E2END + 1; }

#endif // EEPROM_SETTINGS && EEPROM FLASH
#endif // __STM32F1__
