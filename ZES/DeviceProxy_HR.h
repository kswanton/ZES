#pragma once
#include "DeviceProxy.h"
class DeviceProxy_HR :
	public DeviceProxy
{
private:
	int _messageNum = 0;
	int _backgroundPageNum = HRM_DATA_PAGE_1;
	char _powerUp = ' ';
	char _powerDown = ' ';
	UCHAR _heartBeats = 0;
	ULONGLONG _lastBeat = 0;
	ULONGLONG _lastAdjustment = 0;
	int _beatRate = 30;
	int _effectiveRate = _beatRate;
	ULONGLONG _beatTime;
	int _toggle = MASK_TOGGLE_BIT;
public:
	DeviceProxy_HR();
	~DeviceProxy_HR();
	DeviceTypes GetDeviceType();
	void GetStatusMessage(char* buffer, int bufferLen);
	BOOL Start();
	void PopulateNextMessage(UCHAR* buffer);
	void ProcessCommand(char cmd);
	USHORT ChannelPeriod();
};

