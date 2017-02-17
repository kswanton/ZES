#include "stdafx.h"
#include <vector>
#include <memory>
#include <Windows.h>
#include "DeviceProxy.h"
#include "DeviceProxy_Pwr.h"
#include "DeviceProxy_HR.h"
#include <stdio.h>
#include <conio.h>
#include <math.h>

#define PERR(bSuccess, api){if(!(bSuccess)) printf("%s:Error %d from %s on line %d\n", __FILE__, GetLastError(), api, __LINE__);}

void printLine(const char* line, BOOL center);
void printMenu();
void cls(HANDLE hConsole);
void WriteOutput(int channel);
BOOL CtrlWatcher(DWORD fdwCtrlType);

BOOL quitRequested = FALSE;

int main()
{
	cls(GetStdHandle(STD_OUTPUT_HANDLE));

	printLine(NULL, TRUE);
	printLine("Zwift Enhancement Suite", TRUE);
	printLine("Kyle Swanton", TRUE);
	printLine("kyle@savvysoft.ca", TRUE);
	printLine(NULL, TRUE);
	
	char buff[1024];

	BOOL varyMeasures = FALSE;
	BOOL loggingRequested = FALSE;
	BOOL started = FALSE;
	BOOL bailOut = FALSE;
	int devNum = 0;

	srand((unsigned int)time(NULL));

	while (!started && !bailOut) {
		printMenu();
		fgets(buff, sizeof(buff), stdin);
		
		if (strncmp(buff, "P", 1) == 0 || strncmp(buff, "p", 1) == 0) {
			DeviceProxy::RegisteredDevices()->push_back(std::make_unique<DeviceProxy_Pwr>());
			printf("%i devices registered\n", DeviceProxy::RegisteredDevices()->size());
		}
		else if (strncmp(buff, "H", 1) == 0 || strncmp(buff, "h", 1) == 0) {
			DeviceProxy::RegisteredDevices()->push_back(std::make_unique<DeviceProxy_HR>());
			printf("%i devices registered\n", DeviceProxy::RegisteredDevices()->size());
		}
		else if (strncmp(buff, "L", 1) == 0 || strncmp(buff, "l", 1) == 0) {
			printf("Devices will log\n");
			loggingRequested = TRUE;
		}
		else if (strncmp(buff, "V", 1) == 0 || strncmp(buff, "v", 1) == 0) {
			printf("Devices will vary their measurements (to look more 'real')\n");
			varyMeasures = TRUE;
		}
		else if (strncmp(buff, "Q", 1) == 0 || strncmp(buff, "q", 1) == 0) {
			bailOut = TRUE;
		}
		else if (strncmp(buff, "S", 1) == 0 || strncmp(buff, "s", 1) == 0) {
			started = TRUE;
		}
		else if (buff[0] >= '0' && buff[0] <= '9') {
			devNum = buff[0] - '0';
			printf("Device num set to %i\n", devNum);
		}
		else {
			printf("Unknown command.  RTFM\n");
		}
	}

	if (started && DeviceProxy::RegisteredDevices()->size() == 0) {
		printf("No devices added, process completing.");
	}
	else if (started) {

		SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlWatcher, TRUE);

		HANDLE consoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
		HANDLE consoleIn = GetStdHandle(STD_INPUT_HANDLE);
		
		cls(consoleOut);

		// Hide the cursor - since we're not scrolling the console
		// with text, and instead will be simply overwriting, this
		// should reduce screen-flicker.
		CONSOLE_CURSOR_INFO info;
		info.dwSize = 10;
		info.bVisible = FALSE;
		SetConsoleCursorInfo(consoleOut, &info);

		DeviceProxy::SetDeviceNum(devNum);

		for (std::vector<std::unique_ptr<DeviceProxy>>::iterator it = DeviceProxy::RegisteredDevices()->begin(); 
				it != DeviceProxy::RegisteredDevices()->end(); 
				++it) {

			it->get()->Init(loggingRequested, varyMeasures, &WriteOutput);
			if (!it->get()->Start()) {
				printf("ERR: Unable to start ANT system; are you sure you have a ANT+ USB device attached to the system that identifies as device %i?", devNum);
				return 0;
			}
		}

		while (!quitRequested) {
			DWORD waitRes = WaitForSingleObject(consoleIn, 100);

			if (waitRes == WAIT_TIMEOUT) {
				Sleep(0);
			}
			else {
				INPUT_RECORD consoleBuffer;
				DWORD readLen;
				BOOL readResult = ReadConsoleInputA(consoleIn, &consoleBuffer, 1, &readLen);
				
				if (consoleBuffer.Event.KeyEvent.bKeyDown == 0) {
					char cmd = toupper(consoleBuffer.Event.KeyEvent.uChar.AsciiChar);

					if (cmd >= 'A' && cmd <= 'Z') {
						DeviceProxy::DispatchCommand(cmd);
					}
				}
			}
		}

		printf("\n\n\n\n");

		for (std::vector<std::unique_ptr<DeviceProxy>>::iterator it = DeviceProxy::RegisteredDevices()->begin();
		it != DeviceProxy::RegisteredDevices()->end();
			++it) {
			printf("Stopping device with type ID 0x%02x\n", it->get()->GetDeviceType());
			it->get()->Stop();
		}
	}

    return 0;
}

void WriteOutput(int channel) {
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

	static BOOL hasWrittenHeader = FALSE;

	if (!hasWrittenHeader) {

		SetConsoleCursorPosition(console, { 0,0 });

		printLine("Zwift Enhancment Suite", TRUE);
		printLine("Kyle Swanton", TRUE);
		printLine("kyle@savvysoft.ca", TRUE);

		SetConsoleCursorPosition(console, { 0, 5 + (SHORT)DeviceProxy::RegisteredDevices()->size() });
		printf("Press CTRL-C to shutdown and exit");

		hasWrittenHeader = TRUE;
	}

	SetConsoleCursorPosition(console, { 0, 3 + (SHORT)channel });
	
	// Need to find the handler.
	DeviceProxy* device = DeviceProxy::FindDeviceByChannel(channel);

	if (device != NULL) {
		char buffer[180] = "";
		device->GetStatusMessage(buffer, 180);
		printf(buffer);
	}
}

BOOL CtrlWatcher(DWORD fdwCtrlType) {
	if (fdwCtrlType == CTRL_C_EVENT)
	{
		quitRequested = TRUE;
	}

	return TRUE;
}

void cls(HANDLE hConsole)
{
	COORD coordScreen = { 0, 0 };    /* here's where we'll home the
									 cursor */
	BOOL bSuccess;
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */
	DWORD dwConSize;                 /* number of character cells in
									 the current buffer */

									 /* get the number of character cells in the current buffer */

	bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
	PERR(bSuccess, "GetConsoleScreenBufferInfo");
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	/* fill the entire screen with blanks */

	bSuccess = FillConsoleOutputCharacter(hConsole, (TCHAR) ' ',
		dwConSize, coordScreen, &cCharsWritten);
	PERR(bSuccess, "FillConsoleOutputCharacter");

	/* get the current text attribute */

	bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
	PERR(bSuccess, "ConsoleScreenBufferInfo");

	/* now set the buffer's attributes accordingly */

	bSuccess = FillConsoleOutputAttribute(hConsole, csbi.wAttributes,
		dwConSize, coordScreen, &cCharsWritten);
	PERR(bSuccess, "FillConsoleOutputAttribute");

	/* put the cursor at (0, 0) */

	bSuccess = SetConsoleCursorPosition(hConsole, coordScreen);
	PERR(bSuccess, "SetConsoleCursorPosition");
	return;
}

void printMenu() {
	printf("\n");
	printLine(NULL, FALSE);
	printLine("Menu", TRUE);
	printLine("(0)-(9) Set Device Num (defaults to 0)", FALSE);
	printLine("(P) Add Power Device", FALSE);
	printLine("(H) Add HR Device", FALSE);
	printLine("(L) Set Device Logging", FALSE);
	printLine("(V) Set Measurement Variance", FALSE);
	printLine("(S) Start Broadcast", FALSE);
	printLine("(Q) Quit", FALSE);
	printLine(NULL, FALSE);
}

void printLine(const char* line, BOOL center) {
	char val[51];

	if (line == NULL)
		memset(&val, (int)'*', 50);
	else {
		memset(&val, (int)' ', 50);
		memset(&val, (int)'*', 3);
		memset(&val[47], (int)'*', 3);
		int offset = 44 / 2 - strlen(line) / 2;
		strncpy(center ? &val[3 + offset] : &val[4], line, strlen(line));
	}

	val[50] = NULL;

	printf("%s\n", val);
}
