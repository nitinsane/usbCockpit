/**
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <poll.h>

#include <Poco/Net/Socket.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/SocketAddress.h>

#include "hidapi.h"


#define VENDOR_ID 0x03eb
#define PRODUCT_ID 0x2043

#define TO_FG_PORT 9209
#define FROM_FG_PORT 9210

#define BUFSIZE 1024


#define NUM_AXES 8
#define NUM_BUTTONS 40

#pragma pack(push,1)
typedef struct {
	unsigned char reportID;
  struct {
	    int16_t axis[NUM_AXES]; /**< Current absolute position axis 1-8, signed 16-bit integer */
	    int8_t button[(NUM_BUTTONS+7)/8];  /**< Bit mask of the currently pressed joystick buttons 1-40, 8 per byte */
	} data;
} DeviceToHostReport_Data_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
	unsigned char reportID;

	struct {
    int8_t button[(NUM_BUTTONS+7)/8];  /**< Bit mask of the currently pressed joystick buttons 1-40, 8 per byte */
    int16_t axis[NUM_AXES]; /**< Current absolute position axis 1-8, signed 16-bit integer */
	} data;
} HostToDeviceReport_Data_t;
#pragma pack(pop)

typedef Poco::Net::DatagramSocket Socket;

// bit position in the USB report
// these positions are synchronized with Arduino code
#define AP_BUTTON_BIT 0
#define FD_BUTTON_BIT 1
#define AT_BUTTON_BIT 2
#define LNAV_BUTTON_BIT 3
#define VNAV_BUTTON_BIT 4
#define FLCH_BUTTON_BIT 5
#define HDG_HOLD_BUTTON_BIT 6
#define VS_HOLD_BUTTON_BIT 7
#define ALT_HOLD_BUTTON_BIT 8
#define LOC_BUTTON_BIT 9
#define APP_BUTTON_BIT 10
#define AT_DISENGAGE_BUTTON_BIT 11

// bit position in the USB report
// these positions are synchronized with Arduino code
#define AP_LED_BIT 0
#define AT_LED_BIT 2
#define LNAV_LED_BIT 3
#define VNAV_LED_BIT 4
#define FLCH_LED_BIT 5
#define HDG_HOLD_LED_BIT 6
#define VS_HOLD_LED_BIT 7
#define ALT_HOLD_LED_BIT 8
#define LOC_LED_BIT 9
#define APP_LED_BIT 10



// index in the generic protocol exchanged with FG
// these positions are synchronized with to_* and from* XML files
#define ALT_HOLD_INDEX 1

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}


void setButton ( HostToDeviceReport_Data_t* const hostReport, const unsigned int button)
{
	unsigned int index = button/8;
	unsigned int bit = button - 8*index;

	hostReport->data.button [index] |= 1 << bit;
}

void clearButton ( HostToDeviceReport_Data_t* const hostReport, const unsigned int button)
{
	unsigned int index = button/8;
	unsigned int bit = button - 8*index;

	hostReport->data.button [index] &= ~(1 << bit);
}

bool getButtonState (const DeviceToHostReport_Data_t* const deviceReport, const unsigned int button)
{
	unsigned int index = button/8;
	unsigned int bit = button - 8*index;

	unsigned char mask = 1 << bit;

	return !((deviceReport->data.button [index] & mask) == 0);
}

/*
 * parseBuffer
 *   helper function to construct host report from the buffer read over UDP socket
 *   the order of entries here are synchronized with the generic protocol XML files in FG
 */
bool parseBuffer (const char* const buf, HostToDeviceReport_Data_t* hostReport)
{
	unsigned int fdFlag, altHold;

	sscanf (buf, "%d,%d\n", &fdFlag, &altHold);

	
	if (fdFlag == 1)
		setButton (hostReport, FD_BUTTON);
	else
		clearButton (hostReport, FD_BUTTON);


	hostReport->data.axis [ALT_HOLD_INDEX] = altHold;	

	return true;
}

/*
 * prepareBuffer
 *  constructs the buffer from device report
 *  The order of entries here are synchronized with the generic protocol XML files in FG
 */
void prepareBuffer (const char* buf, const DeviceToHostReport_Data_t* const deviceReport)
{
	// The order of entries here are synchronized with the generic protocol XML files in FG

	int v = snprintf (buf, BUFSIZE, "%d,%d\n", getButtonState (deviceReport, FD_BUTTON) == 1 ? 1 : 0, deviceReport->data.axis [ALT_HOLD_INDEX]);
}


/*
 * readDataFromFlightgear
 *   read data over the UDP socket and construct host report
 */
bool readDataFromFlightgear (Socket& readSocket, HostToDeviceReport_Data_t* hostReport)
{
	char buf [BUFSIZE];

	bzero(buf, BUFSIZE);

	Poco::Timespan ts (0,0);
	if (! readSocket.poll (ts, Poco::Net::Socket::SELECT_READ))
		return false;

	int n = readSocket.receiveBytes (buf, BUFSIZE);
	
	return parseBuffer (buf, hostReport);
}

/*
 * sendUSBReport
 *   send host report over the USB
 */
void sendUSBReport (hid_device* handle, const HostToDeviceReport_Data_t* const hostReport)
{
	if (hid_send_feature_report (handle, (const unsigned char *)hostReport, sizeof (HostToDeviceReport_Data_t)) == -1) {
		error ("error sending feature report");
	}

#if DEBUG
	printf ("sendUSBReport ");
	printf ("Report ID = %02X\n", hostReport->reportID);
	printf ("Axis ==> ");
	for (int i = 0; i < NUM_AXES; i++)
		printf ("%02d  ", hostReport->data.axis [i]);
	printf ("\nbuttons ==> ");

	for (int i = 0; i < (NUM_BUTTONS+7)/8;i++)
		printf ("%02X  ", hostReport->data.button [i]);
	printf ("\n");
#endif

}

/*
 * readUSBReport
 *   read the report from USB device
 */
bool readUSBReport (hid_device* handle, DeviceToHostReport_Data_t* deviceReport)
{
	if (hid_get_feature_report (handle, (unsigned char *)deviceReport, sizeof (DeviceToHostReport_Data_t)) == -1) {
		error ("error reading feature report");
		return false;
	}

#if DEBUG
	printf ("readUSBReport ");
	printf ("Report ID = %02X\n", deviceReport->reportID);
	printf ("Axis ==> ");
	for (int i = 0; i < NUM_AXES; i++)
		printf ("%02d  ", deviceReport->data.axis [i]);
	printf ("\nbuttons ==> ");

	for (int i = 0; i < (NUM_BUTTONS+7)/8;i++)
		printf ("%02X  ", deviceReport->data.button [i]);
	printf ("\n");
#endif

	return true;
}


/*
 * sendDataToflightgear
 *   prepare data from device report and send it over UDP socket to flightgear
 *  The order of entries here are synchronized with the generic protocol XML files in FG
 */
void sendDataToFlightgear (Socket& writeSocket, const DeviceToHostReport_Data_t* const deviceReport)
{
	static char buf [BUFSIZE];

	prepareBuffer (buf, deviceReport);


	// send to FG
	Poco::Net::SocketAddress sa ("127.0.0.1", TO_FG_PORT);
	writeSocket.sendTo (buf, v, sa, 0);

#if DEBUG
	printf ("sendToFG => %s\n", buf);
#endif

}

void connect2USBDevice (hid_device *device)
{
}


#define BUFSIZE 1024


int main(int argc, char **argv) {
	Poco::Net::SocketAddress sa(Poco::Net::IPAddress(), FROM_FG_PORT);
	Poco::Net::DatagramSocket readSocket (sa);

	Poco::Net::SocketAddress sa2(Poco::Net::IPAddress(), TO_FG_PORT);
	Poco::Net::DatagramSocket writeSocket (sa);

	HostToDeviceReport_Data_t hostReport;
	DeviceToHostReport_Data_t deviceReport;
	hid_device* usbDeviceHandle;

	memset (&hostReport, 0, sizeof (hostReport));
	memset (&deviceReport, 0, sizeof (deviceReport));


	// initialize HID API
	if (hid_init ()) {
		error ("error init");
	}

	// connect to USB device
	usbDeviceHandle = hid_open (VENDOR_ID, PRODUCT_ID, NULL);

	if (!usbDeviceHandle) {
		error ("error opening");
	}


	for (;;) {
		// if there's a message from FG, send it over USB
		if (readDataFromFlightgear (readSocket, &hostReport))
			sendUSBReport (usbDeviceHandle, &hostReport);

		// if there's message from USB device, send it to FG
		if (readUSBReport (usbDeviceHandle, &deviceReport))
			sendDataToFlightgear (writeSocket, &deviceReport);

	}


}

