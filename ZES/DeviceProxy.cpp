#include "stdafx.h"
#include "DeviceProxy.h"
#include <vector>
#include <memory>
#include <thread>
#include <assert.h>
#include <mutex>

#define USER_NETWORK_NUM         (0)      // The network key is assigned to this network number
#define USER_NETWORK_KEY      {0xB9, 0xA5, 0x21, 0xFB, 0xBD, 0x72, 0xC3, 0x45} //ANT+ Key

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

#define USER_RADIOFREQ        (57)     // HRM Profile specified

BOOL DeviceProxy::_hasInit = FALSE;
int DeviceProxy::_lastChannel = 0;
int DeviceProxy::_lastSerialNum = 0x0A;
vector <unique_ptr<DeviceProxy>>* DeviceProxy::_registeredDevices = new vector <unique_ptr<DeviceProxy>>();
vector <unique_ptr<pair<int, char>>>* DeviceProxy::_registeredActionKeys = new vector <unique_ptr<pair<int, char>>>();
UCHAR* DeviceProxy::_aucResponseBuffer = new UCHAR[MAX_CHANNEL_EVENT_SIZE];
int DeviceProxy::_devNum = 0;

mutex DeviceProxy::_mx;

using namespace std;

DeviceProxy::DeviceProxy()
{
}


DeviceProxy::~DeviceProxy()
{
}

BOOL DeviceProxy::LoadANTLib() {
	BOOL dllloaded = ANT_Load();
	return dllloaded;
}

vector <unique_ptr<DeviceProxy>>* DeviceProxy::RegisteredDevices() {
	return _registeredDevices;
}

vector <unique_ptr<pair<int, char>>>* DeviceProxy::RegisteredActionKeys() {
	return _registeredActionKeys;
}

void DeviceProxy::DispatchCommand(char command) {
	_mx.lock();
	int associatedChannel = -1;
	for (std::vector<std::unique_ptr<pair<int, char>>>::iterator it = DeviceProxy::RegisteredActionKeys()->begin();
			it != DeviceProxy::RegisteredActionKeys()->end();
			++it) {
		if (it->get()->second == command)
			associatedChannel = it->get()->first;
	}
	_mx.unlock();

	if (associatedChannel >= 0) {
		DeviceProxy* device = DeviceProxy::FindDeviceByChannel(associatedChannel);

		if (device != NULL)
			device->ProcessCommand(command);
	}
}

void DeviceProxy::ProcessCommand(char cmd) {
}

DeviceProxy* DeviceProxy::FindDeviceByChannel(int channel) {
	// Could probabaly replace the vector with a map key'ed on the channel...
	_mx.lock();
	DeviceProxy* locatedPx = NULL;
	for (std::vector<std::unique_ptr<DeviceProxy>>::iterator it = DeviceProxy::RegisteredDevices()->begin();
	it != DeviceProxy::RegisteredDevices()->end();
		++it) {
		if (it->get()->Channel() == channel)
			locatedPx = it->get();
	}
	_mx.unlock();

	return locatedPx;
}

BOOL DeviceProxy::ANTLibEvent(UCHAR channel, UCHAR msgId) {
	OutputDebugString(L"lib event.\n");

	// Attempt to look up the device object.
	DeviceProxy* device = DeviceProxy::FindDeviceByChannel(channel);

	BOOL bSuccess = FALSE;

	switch (msgId)
	{
	case MESG_STARTUP_MESG_ID:
	{
		UCHAR ucReason = _aucResponseBuffer[MESSAGE_BUFFER_DATA1_INDEX];
		break;
	}
	case MESG_CAPABILITIES_ID:
	{
		UCHAR ucStandardOptions = _aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX];
		UCHAR ucAdvanced = _aucResponseBuffer[MESSAGE_BUFFER_DATA4_INDEX];
		UCHAR ucAdvanced2 = _aucResponseBuffer[MESSAGE_BUFFER_DATA5_INDEX];
		break;
	}
	case MESG_CHANNEL_STATUS_ID:
	{
		char astrStatus[][32] = { "STATUS_UNASSIGNED_CHANNEL",
			"STATUS_ASSIGNED_CHANNEL",
			"STATUS_SEARCHING_CHANNEL",
			"STATUS_TRACKING_CHANNEL" };

		UCHAR ucStatusByte = _aucResponseBuffer[MESSAGE_BUFFER_DATA2_INDEX] & STATUS_CHANNEL_STATE_MASK; // MUST MASK OFF THE RESERVED BITS
		break;
	}
	case MESG_CHANNEL_ID_ID:
	{
		USHORT usDeviceNumber = _aucResponseBuffer[MESSAGE_BUFFER_DATA2_INDEX] | (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] << 8);
		UCHAR ucDeviceType = _aucResponseBuffer[MESSAGE_BUFFER_DATA4_INDEX];
		UCHAR ucTransmissionType = _aucResponseBuffer[MESSAGE_BUFFER_DATA5_INDEX];

		// All profile types will transmit at 4hz.
		ANT_SetChannelPeriod(channel, USER_PERIOD_FULL);

		break;
	}
	case MESG_VERSION_ID:
	{
		break;
	}
	case MESG_RESPONSE_EVENT_ID:
	{
		switch (_aucResponseBuffer[MESSAGE_BUFFER_DATA2_INDEX])
		{
		case MESG_CHANNEL_MESG_PERIOD_ID:
		{
			bSuccess = ANT_SetChannelRFFreq(channel, USER_RADIOFREQ);
		}
		break;
		case MESG_NETWORK_KEY_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}

			// This event will be received after the network key has been successfully set.
			// For each registered device, we'll need to assign it to its channel.
			// Each call to ANT_AssignChannel will start an command/response chain 
			// here of setting up the channel and opening it for braodcast.

			int *channels = new int[DeviceProxy::RegisteredDevices()->size()];
			int offset = 0;

			_mx.lock();
			for (std::vector<std::unique_ptr<DeviceProxy>>::iterator it = DeviceProxy::RegisteredDevices()->begin();
			it != DeviceProxy::RegisteredDevices()->end();
				++it) {
				channels[offset++] = it->get()->Channel();
			}
			_mx.unlock();

			for (int idx = 0; idx < offset; idx++)
				bSuccess = ANT_AssignChannel(channels[idx], PARAMETER_TX_NOT_RX, USER_NETWORK_NUM);

			delete channels;

			break;
		}

		case MESG_ASSIGN_CHANNEL_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}

			// Need to increment the device number over different instances, as its possible
			// that multiple devices of a single device profile class are registered and transmitting
			// concurrently.  i.e., two HR sensors.  Most receivers (Zwift, Garmin head units)
			// treat a pair/remeber by the device type/class and its ID.  For most hardware, the device 
			// number is essentially its serial number, being set for the individual device by the manufacturer.
			// For simplicity sake, we'll simply increment by one per registered device.  When the user pairs the device
			// in their receiver, providing the register/add the devices in the same order the next time this
			// app is run, they should not have to search/add the devices again...

			static int deviceNum = 1;
			bSuccess = ANT_SetChannelId(device->Channel(), deviceNum++, device->GetDeviceType(), 1);
			break;
		}

		case MESG_CHANNEL_ID_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}

			// All device classes will transmit at 4hz.
			bSuccess = ANT_SetChannelPeriod(channel, device->ChannelPeriod());
			break;
		}

		case MESG_CHANNEL_RADIO_FREQ_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}
			bSuccess = ANT_OpenChannel(channel);
			break;
		}

		case MESG_OPEN_CHANNEL_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}

#if defined (ENABLE_EXTENDED_MESSAGES)
			ANT_RxExtMesgsEnable(TRUE);
#endif
			break;
		}

		case MESG_RX_EXT_MESGS_ENABLE_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] == INVALID_MESSAGE)
			{
				break;
			}
			else if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}

			break;
		}
		case MESG_UNASSIGN_CHANNEL_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}

			break;
		}
		case MESG_CLOSE_CHANNEL_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] == CHANNEL_IN_WRONG_STATE)
			{
				break;
			}
			else if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] != RESPONSE_NO_ERROR)
			{
				break;
			}

			break;
		}
		case MESG_REQUEST_ID:
		{
			if (_aucResponseBuffer[MESSAGE_BUFFER_DATA3_INDEX] == INVALID_MESSAGE)
			{
				break;
			}
		}

		default:
		{
			break;
		}
		}
	}
	}
	return(TRUE);
}

int DeviceProxy::GetMessageResolution() {
	return 4; // 4Hz
}

USHORT DeviceProxy::ChannelPeriod() {
	return USER_PERIOD_FULL;
}

BOOL DeviceProxy::ANTChannelEvent(UCHAR channel, UCHAR evtId) {
	OutputDebugString(L"chan event.\n");

	// Need to find the handler for this channel and request the next data 
	// buffer be configured.
	_mx.lock();
	DeviceProxy* locatedPx = NULL;
	for (std::vector<std::unique_ptr<DeviceProxy>>::iterator it = DeviceProxy::RegisteredDevices()->begin();
		it != DeviceProxy::RegisteredDevices()->end();
		++it) {
		if (it->get()->Channel() == channel)
			locatedPx = it->get();
	}
	_mx.unlock();

	if (locatedPx != NULL) {
		locatedPx->PopulateNextMessage(locatedPx->_aucChannelBuffer);
		locatedPx->WriteLogMessage();
	}

	return TRUE;
}

void DeviceProxy::WriteLogMessage() {
	if (_loggingRequested) {

		static ULONGLONG firstMsg = GetTickCount64();
		ULONGLONG currentMsg = GetTickCount64();
		int msgTcks = (int)(currentMsg - firstMsg);

		char message[1024];

		sprintf(message, "%010i Tx:(%d): [%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x]",
			msgTcks,
			Channel(),
			_aucChannelBuffer[MESSAGE_BUFFER_DATA1_INDEX],
			_aucChannelBuffer[MESSAGE_BUFFER_DATA2_INDEX],
			_aucChannelBuffer[MESSAGE_BUFFER_DATA3_INDEX],
			_aucChannelBuffer[MESSAGE_BUFFER_DATA4_INDEX],
			_aucChannelBuffer[MESSAGE_BUFFER_DATA5_INDEX],
			_aucChannelBuffer[MESSAGE_BUFFER_DATA6_INDEX],
			_aucChannelBuffer[MESSAGE_BUFFER_DATA7_INDEX],
			_aucChannelBuffer[MESSAGE_BUFFER_DATA8_INDEX]);

		_log << message << endl;
	}

	_updateCallback(Channel());
}

BOOL DeviceProxy::Init(
	BOOL loggingRequested, 
	BOOL varyMeasures,
	ScreenWriterCallback updateCallback) {

	if (!_hasInit) {
		OutputDebugString(L"Loading ANT+ lib.\n");
		_hasInit = 1;
		
		BOOL libLoadRes = this->LoadANTLib();

		assert(libLoadRes);
	}
	
	_varyMeasures = varyMeasures;
	_loggingRequested = loggingRequested;
	_updateCallback = updateCallback;

	return TRUE;
}

void DeviceProxy::Stop() {
	ANT_CloseChannel(Channel());
}

void DeviceProxy::GetStatusMessage(char* buffer, int bufferLen) {
}

BOOL DeviceProxy::Start() {
	_instanceChannel = this->GetNextChannel();
	_instanceSerialNum = this->GetNextSerialNum();

	if (_loggingRequested) {
		char fileName[MAX_PATH];
		sprintf(fileName, "ANT_%i.log", Channel());
		_log.open(fileName);

		_log << "Log start on ANT+ Channel " << Channel() << endl;
	}

	static BOOL alreadyInit = FALSE;

	if (!alreadyInit) {
		BOOL result = ANT_Init(_devNum, USER_BAUDRATE);

		if (!result)
			return FALSE;

		ANT_ResetSystem();
		ANT_Nap(1000);

		assert(result);

		ANT_AssignResponseFunction(DeviceProxy::ANTLibEvent, _aucResponseBuffer);

		alreadyInit = TRUE;
	}

	ANT_AssignChannelEventFunction(Channel(), DeviceProxy::ANTChannelEvent, _aucChannelBuffer);
	UCHAR ucNetKey[8] = USER_NETWORK_KEY;

	ANT_SetNetworkKey(USER_NETWORK_NUM, ucNetKey);

	return TRUE;
}

void DeviceProxy::SetDeviceNum(int devNum) {
	_devNum = devNum;
}

void DeviceProxy::PopulateNextMessage(UCHAR* buffer) {
}

int DeviceProxy::GetNextChannel() {
	return ++_lastChannel;
}

int DeviceProxy::GetNextSerialNum() {
	return ++_lastSerialNum;
}

DeviceTypes DeviceProxy::GetDeviceType() {
	return (DeviceTypes)NULL;
}

char DeviceProxy::RegisterNewActionKey() {
	static char current = 'A';
	
	int ch = Channel();
	DeviceProxy::_registeredActionKeys->push_back(make_unique<pair<int, char>>(ch, current));

	return current++;
}

int DeviceProxy::Channel() {
	return _instanceChannel;
}

int DeviceProxy::SerialNum() {
	return _instanceSerialNum;
}