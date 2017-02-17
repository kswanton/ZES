#pragma once
#include "DeviceProxy.h"
class DeviceProxy_Pwr :
	public DeviceProxy
{
private:
	int _messageNum = 0;
	USHORT _runningPower = 0;
	ULONGLONG _lastAdjustment = 0;
	int _pwr = 85;
	int _effectivePwr = _pwr;
	int _currentBGMessage = PWR_DATA_PAGE_MANU;
	char _powerUpSingle = ' ';
	char _powerDownSingle = ' ';
	char _powerUpTen = ' ';
	char _powerDownTen = ' ';

	UCHAR _eventCount = 0;
public:
	DeviceProxy_Pwr();
	~DeviceProxy_Pwr();
	DeviceTypes GetDeviceType();
	void GetStatusMessage(char* buffer, int bufferLen);
	BOOL Start();
	void PopulateNextMessage(UCHAR* buffer);
	void ProcessCommand(char cmd);
	USHORT ChannelPeriod();
};

