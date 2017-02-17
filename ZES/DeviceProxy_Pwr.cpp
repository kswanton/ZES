#include "stdafx.h"
#include "DeviceProxy_Pwr.h"
#include <assert.h>

DeviceProxy_Pwr::DeviceProxy_Pwr()
{
}


DeviceProxy_Pwr::~DeviceProxy_Pwr()
{
}

DeviceTypes DeviceProxy_Pwr::GetDeviceType() {
	return DEVICETYPE_Power;
}

USHORT DeviceProxy_Pwr::ChannelPeriod() {
	return USER_PERIOD_DOUBLE;
}

void DeviceProxy_Pwr::PopulateNextMessage(UCHAR* buffer) {
	
	ULONGLONG currentTicks = GetTickCount64();

	if (_varyMeasures && (currentTicks - _lastAdjustment) > 1000) {
		// Just like the HR profile, we can try to make some adjustments to power to 
		// make it look 'real' instead of having the rider hold the exact same power without ever
		// wavering in their power output.
		// Power will vary at a faster rate than HR, every second.
		// I'm sure we could write up some algo to closly match what an average
		// rider would vary in their power, and and how that might differ when they're 
		// in their different zones.  i.e., if the rider is focusing on a 20min full effort,
		// they're likely not going to vary as much as if they're just farting around in Z2/3.
		// But we'll keep it simple and just vary the output by +/-5% with a maximum of 2% per adjustment.
		
		// Get the adjust val.
		int adjustPer = (rand() % (3 - (-2))) + (-2);
		int adjustedPower = (int)(_effectivePwr * (1 + (adjustPer / 100.0)));
		int maxPower = (int)(_pwr * 1.105);
		int minPower = (int)(_pwr * 0.95);

		if (adjustedPower > maxPower)
			_effectivePwr = maxPower;
		else if (adjustedPower < minPower)
			_effectivePwr = minPower;
		else
			_effectivePwr = adjustedPower;

		_lastAdjustment = currentTicks;
	}

	// Regardless of the message, a couple of things need to occur.
	// The accumulated power need to be incremented by the value of this period.
	// The event counter needs to inc by one.
	// Since this could be a background message, that means the current power will not be
	// sent in this slot, and instead manufacturer or model info will be sent.
	// Receivers will use accumlated along with event counts to 'fill in' the gaps.
	_runningPower += _effectivePwr;
	_eventCount++;

	if (_messageNum == 0)
	{
		switch (_currentBGMessage) {
		case PWR_DATA_PAGE_MANU:

			buffer[MESSAGE_BUFFER_DATA1_INDEX] = PWR_DATA_PAGE_MANU;
			buffer[MESSAGE_BUFFER_DATA2_INDEX] = 0xFF;
			buffer[MESSAGE_BUFFER_DATA3_INDEX] = 0xFF;
			buffer[MESSAGE_BUFFER_DATA4_INDEX] = 0x01;
			buffer[MESSAGE_BUFFER_DATA5_INDEX] = PWR_MANUFACTURER_ID & 0xFF;
			buffer[MESSAGE_BUFFER_DATA6_INDEX] = (PWR_MANUFACTURER_ID >> 8) & 0xFF;
			buffer[MESSAGE_BUFFER_DATA7_INDEX] = PWR_MODEL_NUM & 0xFF;
			buffer[MESSAGE_BUFFER_DATA8_INDEX] = (PWR_MODEL_NUM >> 8) &0xFF;

			_currentBGMessage = PWR_DATA_PAGE_DEVINF;
			break;

		case PWR_DATA_PAGE_DEVINF:
		default:

			buffer[MESSAGE_BUFFER_DATA1_INDEX] = PWR_DATA_PAGE_DEVINF;
			buffer[MESSAGE_BUFFER_DATA2_INDEX] = 0xFF;
			buffer[MESSAGE_BUFFER_DATA3_INDEX] = 0xFF;
			buffer[MESSAGE_BUFFER_DATA4_INDEX] = 0x01;
			buffer[MESSAGE_BUFFER_DATA5_INDEX] = SerialNum() & 0xFF;
			buffer[MESSAGE_BUFFER_DATA6_INDEX] = (SerialNum() >> 8) & 0xFF;
			buffer[MESSAGE_BUFFER_DATA7_INDEX] = (SerialNum() >> 16) & 0xFF;
			buffer[MESSAGE_BUFFER_DATA8_INDEX] = (SerialNum() >> 24) & 0xFF;

			_currentBGMessage = PWR_DATA_PAGE_MANU;
			break;
		}
	}
	else
	{
		// Normal data page.
		buffer[MESSAGE_BUFFER_DATA1_INDEX] = PWR_DATA_PAGE_SIMPPWR;
		buffer[MESSAGE_BUFFER_DATA2_INDEX] = _eventCount;
		buffer[MESSAGE_BUFFER_DATA3_INDEX] = 0xFF;
		buffer[MESSAGE_BUFFER_DATA4_INDEX] = 0xFF;
		buffer[MESSAGE_BUFFER_DATA5_INDEX] = _runningPower & 0xFF;
		buffer[MESSAGE_BUFFER_DATA6_INDEX] = (_runningPower >> 8) & 0xFF;
		buffer[MESSAGE_BUFFER_DATA7_INDEX] = _effectivePwr & 0xFF;
		buffer[MESSAGE_BUFFER_DATA8_INDEX] = (_effectivePwr >> 8) & 0xFF;
	}
	
	// The buffer is ready to be sent to the ANT lib to be broadcast.
	ANT_SendBroadcastData(Channel(), buffer);

	_messageNum++;

	if (_messageNum >= PWR_MAXMSGNUM)
		_messageNum = 0;
}

void DeviceProxy_Pwr::GetStatusMessage(char* buffer, int bufferLen) {
	sprintf(
		buffer, 
		"CH:%i, Pwr: %i, Effective:%i, Actions: Pwr+1:%c, Pwr+10:%c, Pwr-1:%c, Pwr-10:%c", 
		Channel(), 
		_pwr, 
		_effectivePwr,
		_powerUpSingle, 
		_powerUpTen, 
		_powerDownSingle, 
		_powerDownTen);
}

void DeviceProxy_Pwr::ProcessCommand(char cmd) {
	if (cmd == _powerUpSingle) {
		_pwr++;
	}
	else if (cmd == _powerDownSingle) {
		_pwr--;
	}
	else if (cmd == _powerUpTen) {
		_pwr += 10;
	}
	else if (cmd == _powerDownTen) {
		_pwr -= 10;
	}

	_effectivePwr = _pwr;
}

BOOL DeviceProxy_Pwr::Start() {
	if (!DeviceProxy::Start())
		return FALSE;

	char buffer[50];
	sprintf(buffer, "Pwr Starting on channel %i\n", Channel());
	OutputDebugStringA(buffer);

	// Four action keys for this profile, Pwr up/down by 1 and by 10.
	_powerUpSingle = this->RegisterNewActionKey();
	_powerUpTen = this->RegisterNewActionKey();

	_powerDownSingle = this->RegisterNewActionKey();
	_powerDownTen = this->RegisterNewActionKey();

	return TRUE;
}