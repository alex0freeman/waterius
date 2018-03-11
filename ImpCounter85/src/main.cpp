
#include "Setup.h"

#include <avr/pgmspace.h>
#include <TinyDebugSerial.h>
#include <USIWire.h>

#include "Power.h"
#include "SlaveI2C.h"
#include "Storage.h"
#include "sleep_counter.h"

#ifdef DEBUG
	TinyDebugSerial mySerial;
#endif

static enum State state = SLEEP;
static uint16_t minSleeping = 0;
static uint16_t masterWokenUpAt;

//глобальный счетчик до 65535
static uint16_t counter = 0;
static uint16_t counter2 = 0;

//одно измерение
struct SensorData {
	uint16_t counter;	     
	uint16_t counter2;     
};

//Заголовок данных для ESP.
//размер кратен 2байтам (https://github.com/esp8266/Arduino/issues/1825)
struct SlaveStats info = {
	0, //байт прочитано
	WAKE_MASTER_EVERY_MIN, //период передачи данных на сервер, мин  (требуется ручная калибровка внутреннего генератора)
	MEASUREMENT_EVERY_MIN, //период измерения данных в буфер, мин
	0, //напряжение питания, мВ (требуется ручная калибровка)
	0, //байт в 1 измерении
	DEVICE_ID, //модель устройства. пока = 1
	NUMBER_OF_SENSORS, //кол-во сенсоров = 2
	0 // пустой байт для кратности 2
};

Storage storage( sizeof( SensorData ) );
SlaveI2C slaveI2C;

void setup() 
{
	DEBUG_CONNECT(9600);
  	DEBUG_PRINTLN(F("==== START ===="));

	resetWatchdog(); //??? Needed for deep sleep to succeed
	adc_disable(); //выключаем ADC

	//при включении проснемся и будем готовы передать 0 на сервер.
	minSleeping = WAKE_MASTER_EVERY_MIN;
	state = MEASURING;
}

void loop() 
{
	switch ( state ) {
		case SLEEP:
			DEBUG_PRINT(F("LOOP (SLEEP)"));
			slaveI2C.end();			// выключаем i2c slave. 
			gotoDeepSleep( MEASUREMENT_EVERY_MIN, &counter, &counter2);	// Глубокий сон X минут
			minSleeping += MEASUREMENT_EVERY_MIN;
			state = MEASURING;

			DEBUG_PRINT("COUNTERS:"); DEBUG_PRINTLN(counter); DEBUG_PRINT("; "); DEBUG_PRINTLN(counter2);
			break;

		case MEASURING:
			DEBUG_PRINT(F("LOOP (MEASURING)"));
			SensorData sensorData;
			sensorData.counter = counter;
			sensorData.counter2 = counter2;
			storage.addElement( &sensorData );

			state = SLEEP;			// Сохранили текущие значения
			if ( minSleeping >= WAKE_MASTER_EVERY_MIN ) 
				state = MASTER_WAKE; // пришло время будить ESP
			break;

		case MASTER_WAKE:
			DEBUG_PRINT(F("LOOP (MASTER_WAKE)"));
			info.vcc = readVcc();   // заранее запишем текущее напряжение
			slaveI2C.begin();		// Включаем i2c slave
			wakeESP();              // импульс на reset ESP
			masterWokenUpAt = millis();  //запомнили время пробуждения
			state = SENDING;
			break;

		case SENDING:
			if (slaveI2C.masterGoingToSleep()) 
			{
				if (slaveI2C.masterGotOurData()) 
				{
					DEBUG_PRINT(F("ESP ok"));
					storage.clear();
				}
				else
				{
					DEBUG_PRINT(F("ESP send fail"));
				}
				minSleeping = 0;
				state = SLEEP;
			}

			if (millis() - masterWokenUpAt > GIVEUP_ON_MASTER_AFTER * 1000UL) 
			{
				DEBUG_PRINT(F("ESP wake up fail"));
				minSleeping = 0;
				state = SLEEP;
			}
			break;
	}
}