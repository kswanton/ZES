#include "stdafx.h"
#include "DeviceProxy.h"
#include "DeviceProxy_HR.h"
#include <assert.h>


DeviceProxy_HR::DeviceProxy_HR()
{
}


DeviceProxy_HR::~DeviceProxy_HR()
{
}

DeviceTypes DeviceProxy_HR::GetDeviceType() {
	return DEVICETYPE_HR;
}

USHORT DeviceProxy_HR::ChannelPeriod() {
	return USER_PERIOD_FULL;
}

void DeviceProxy_HR::PopulateNextMessage(UCHAR* buffer) {
	
	// First, calculate if a HR beat has occured;
	// Note: We're going to ignore rollover here - I doubt
	// someone is going to run this to 2^64mil... what is that?  ~200 million days?
	ULONGLONG currentTicks = GetTickCount64();
	ULONGLONG millFromLastBeat = currentTicks - _lastBeat;

	if (_varyMeasures && (currentTicks - _lastAdjustment) > 5000) {
		// Measures should vary to make the output look real...
		// ... this is providing the user has set a HR to match the power
		// they're putting out I guess.
		// I'm no scientist, so I'm just going to go with my gut and let the 
		// HR float between -3 -> +3 from the rate... and the way this'll work
		// is also quite unscientific:  
		// Adjust the rate every 5 seconds by drawing a random number between -1 -> +1 and 
		// apply this to the current effective rate, checking to ensure its not exceeded the -3/+3 limits.
		int adjustVal = (rand() % (2 - (-1))) + (-1);

		if (_effectiveRate + adjustVal >= _beatRate - 3
			&& _effectiveRate + adjustVal <= _beatRate + 3)
			_effectiveRate += adjustVal;

		_lastAdjustment = currentTicks;
	}

	// Step one is to figure out how often a beat should be 
	// recorded, in mil.
	int millsPerBeat = (int)(60000.0 / (_effectiveRate * 1000) * 1000);

	/*
	char tmp[1024];
	sprintf(tmp, "MillsPerBeat: %i\n", millsPerBeat);
	OutputDebugStringA(tmp);
	*/

	if (millFromLastBeat > millsPerBeat)
	{
		_beatTime = ((currentTicks - _deviceStartupTime) * 1024) / 1000; 
		_lastBeat = currentTicks;
		_heartBeats += (UCHAR)(millFromLastBeat / millsPerBeat);

		char tmp[1024];
		sprintf(tmp, "Total: %i\n", _heartBeats);
		OutputDebugStringA(tmp);
	}

	// Data offsets 5-8 (1 offset) are static overall normal and background pages.
	buffer[MESSAGE_BUFFER_DATA8_INDEX] = _effectiveRate; //Byte 7 (big endian)
	buffer[MESSAGE_BUFFER_DATA7_INDEX] = _heartBeats;
	buffer[MESSAGE_BUFFER_DATA6_INDEX] = (UCHAR)(_beatTime >> 8); //msb
	buffer[MESSAGE_BUFFER_DATA5_INDEX] = (UCHAR)(_beatTime & MASK_HEARTBEAT_EVENT_TIME_LSB); //lsb

	// Couple of rules for HR messaging.  There are 3 mandatory background pages that 
	// need to be sent for this profile - 
	//  1 - Cumulative Operating Time
	//  2 - Manufacturer info
	//  3 - Model/SN info
	if (_messageNum < 4) 
	{
		// Determine which background page is going to get sent.
		switch (_backgroundPageNum) {
		case HRM_DATA_PAGE_1:
		{
			// this background page is cummlative op time.
			ULONGLONG timeSinceStart = (currentTicks - _deviceStartupTime) / 1000 / 2;

			//Page Number
			buffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_1;

			//Cumulative Operating Time
			buffer[MESSAGE_BUFFER_DATA2_INDEX] = timeSinceStart & 0xFF;
			buffer[MESSAGE_BUFFER_DATA3_INDEX] = (timeSinceStart >> 8) & 0xFF;
			buffer[MESSAGE_BUFFER_DATA4_INDEX] = (timeSinceStart >> 16) & 0xFF;
		}
			break;

		case HRM_DATA_PAGE_2:
		{
			buffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_2;

			//Manufacture ID
			buffer[MESSAGE_BUFFER_DATA2_INDEX] = HRM_MANUFACTURER_ID;

			//Serial Number
			buffer[MESSAGE_BUFFER_DATA3_INDEX] = SerialNum() & MASK_SERIAL_NUMBER_LSB;
			buffer[MESSAGE_BUFFER_DATA4_INDEX] = (SerialNum() >> 8) & MASK_SERIAL_NUMBER_LSB;
		}
			break;

		case HRM_DATA_PAGE_3:
		{
			//Page number
			buffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_3;

			//Hardware Version
			buffer[MESSAGE_BUFFER_DATA2_INDEX] = HRM_HARDWARE_VERSION;

			//Software Version
			buffer[MESSAGE_BUFFER_DATA3_INDEX] = HRM_SOFTWARE_VERSION;

			//Model Number
			buffer[MESSAGE_BUFFER_DATA4_INDEX] = HRM_MODEL_NUMBER;
		}
			break;
		}

		if (_messageNum >= 3)
			_backgroundPageNum = _backgroundPageNum == HRM_DATA_PAGE_3 ? HRM_DATA_PAGE_1 : _backgroundPageNum + 1;
	}
	else 
	{
		// Standard datapage.
		buffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_0;
		for (UCHAR i = MESSAGE_BUFFER_DATA2_INDEX; i <= MESSAGE_BUFFER_DATA4_INDEX; i++) {
			buffer[i] = HRM_RESERVED;
		}
	}

	// Every 4th message needs to have its 'Toggle bit' flipped.
	// Not sure what the purpose of this is.  Some check for receivers to know that the device is
	// not just replaying the exact same message in error?  Shows that messages are 'fresh'? 
	if (_messageNum % 4 == 0) // Flip it.
		_toggle = ~_toggle;

	if (_toggle == MASK_TOGGLE_BIT)
		buffer[MESSAGE_BUFFER_DATA1_INDEX] = buffer[MESSAGE_BUFFER_DATA1_INDEX] | _toggle;
	else
		buffer[MESSAGE_BUFFER_DATA1_INDEX] = buffer[MESSAGE_BUFFER_DATA1_INDEX] & _toggle;

	// The buffer is ready to be sent to the ANT lib to be broadcast.
	ANT_SendBroadcastData(Channel(), buffer);

	_messageNum++;

	if (_messageNum >= HRM_MAXMSGNUM)
		_messageNum = 0;
}

void DeviceProxy_HR::GetStatusMessage(char* buffer, int bufferLen) {
	// Should be checking for an overflow... 
	sprintf(
		buffer, 
		"CH:%i, HR:%i, Effective:%i, Actions: Increase HR:%c, Decrease HR:%c", 
		Channel(), 
		_beatRate, 
		_effectiveRate,
		_powerUp, 
		_powerDown);
}

void DeviceProxy_HR::ProcessCommand(char cmd) {
	if (cmd == _powerUp) {
		_beatRate++;
	}
	else if (cmd == _powerDown) {
		_beatRate--;
	}

	_effectiveRate = _beatRate;
}

BOOL DeviceProxy_HR::Start() {
	if (!DeviceProxy::Start())
		return FALSE;

	char buffer[50];
	sprintf(buffer, "HR Starting on channel %i\n", Channel());
	OutputDebugStringA(buffer);

	// Two action keys for this profile, HR up and HR down.
	_powerUp = this->RegisterNewActionKey();
	_powerDown = this->RegisterNewActionKey();

	return TRUE;
}