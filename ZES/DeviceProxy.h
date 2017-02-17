#pragma once

#include <Windows.h>
#include "libant.h"
#include "antmessage.h"
#include "stdafx.h"
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>
#include <fstream>

using namespace std;

#define USER_BAUDRATE            (57600)  // For AP1, use 50000; for AT3/AP2, use 57600
#define DEVNUM 0
#define MAX_RESPONSE_SIZE        (MESG_MAX_SIZE_VALUE)     // Protocol response buffer size
#define MAX_CHANNEL_EVENT_SIZE   (MESG_MAX_SIZE_VALUE)     // Channel event buffer size, assumes worst case extended message size

#define MESSAGE_BUFFER_DATA1_INDEX ((UCHAR) 0)
#define MESSAGE_BUFFER_DATA2_INDEX ((UCHAR) 1)
#define MESSAGE_BUFFER_DATA3_INDEX ((UCHAR) 2)
#define MESSAGE_BUFFER_DATA4_INDEX ((UCHAR) 3)
#define MESSAGE_BUFFER_DATA5_INDEX ((UCHAR) 4)
#define MESSAGE_BUFFER_DATA6_INDEX ((UCHAR) 5)
#define MESSAGE_BUFFER_DATA7_INDEX ((UCHAR) 6)
#define MESSAGE_BUFFER_DATA8_INDEX ((UCHAR) 7)
#define MESSAGE_BUFFER_DATA9_INDEX ((UCHAR) 8)
#define MESSAGE_BUFFER_DATA10_INDEX ((UCHAR) 9)
#define MESSAGE_BUFFER_DATA11_INDEX ((UCHAR) 10)
#define MESSAGE_BUFFER_DATA12_INDEX ((UCHAR) 11)
#define MESSAGE_BUFFER_DATA13_INDEX ((UCHAR) 12)
#define MESSAGE_BUFFER_DATA14_INDEX ((UCHAR) 13)

#define MASK_HEARTBEAT_EVENT_TIME_LSB  (0x00FF)

#define HRM_DATA_PAGE_0             (0x00)
#define HRM_DATA_PAGE_1             (0x01)
#define HRM_DATA_PAGE_2             (0x02)
#define HRM_DATA_PAGE_3             (0x03)

#define PWR_DATA_PAGE_SIMPPWR       (0x10)
#define PWR_DATA_PAGE_MANU          (0x50)
#define PWR_DATA_PAGE_DEVINF        (0x51)

#define HRM_MANUFACTURER_ID            (0xFA)
#define MASK_SERIAL_NUMBER_LSB         (0xFF)

#define PWR_MANUFACTURER_ID			(0x1D) // Power2Max/saxonar
#define PWR_MODEL_NUM				(0x62)

#define HRM_HARDWARE_VERSION        (0x32)
#define HRM_SOFTWARE_VERSION        (0x04)
#define HRM_MODEL_NUMBER            (0x5C)

#define HRM_RESERVED             (0xFF)

#define HRM_MAXMSGNUM (68)
#define PWR_MAXMSGNUM (61)

#define MASK_TOGGLE_BIT             (0x80)

#define USER_PERIOD_DOUBLE          (4091)   // HRM Profile specified
#define USER_PERIOD_FULL          (8070)   // HRM Profile specified
#define USER_PERIOD_HALF          (16140)   // HRM Profile specified
#define USER_PERIOD_QUARTER       (32280)   // HRM Profile specified

typedef void(*ScreenWriterCallback)(int);

class DeviceProxy
{
private:
	static BOOL _hasInit;
	static int _lastChannel;
	static int _lastSerialNum;
	int GetNextChannel();
	int GetNextSerialNum();
	int _instanceChannel = 0;
	int _instanceSerialNum = 0;
	ScreenWriterCallback _updateCallback;
	BOOL _loggingRequested = FALSE;
	ofstream _log;
	UCHAR* _aucChannelBuffer = new UCHAR[MAX_CHANNEL_EVENT_SIZE];
	
	static UCHAR* _aucResponseBuffer;
	static vector <unique_ptr<DeviceProxy>>* _registeredDevices;
	static vector <unique_ptr<pair<int, char>>>* _registeredActionKeys;
	static BOOL LoadANTLib();
	static mutex _mx;
	static int _devNum;

protected:
	int GetMessageResolution();
	char RegisterNewActionKey();
	BOOL _varyMeasures = FALSE;
	ULONGLONG _deviceStartupTime = GetTickCount64();

	static BOOL ANTLibEvent(UCHAR channel, UCHAR msgId);
	static BOOL ANTChannelEvent(UCHAR channel, UCHAR evtId);
	
public:
	virtual DeviceTypes GetDeviceType();
	DeviceProxy();
	~DeviceProxy();

	static vector <unique_ptr<DeviceProxy>>* RegisteredDevices();
	static vector <unique_ptr<pair<int, char>>>* RegisteredActionKeys();
	static DeviceProxy* FindDeviceByChannel(int channel);
	static void DispatchCommand(char command);
	static void SetDeviceNum(int devNum);
	
	BOOL Init(BOOL loggingRequested, BOOL varyMeasures, ScreenWriterCallback updateCallback);
	int Channel();
	int SerialNum();
	void WriteLogMessage();
	virtual void GetStatusMessage(char* buffer, int bufferLen);
	virtual BOOL Start();
	virtual void Stop();
	virtual void PopulateNextMessage(UCHAR* buffer);
	virtual void ProcessCommand(char cmd);
	virtual USHORT ChannelPeriod();
};

