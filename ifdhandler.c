/*****************************************************************
/
/ File   :   ifdhandler.c
/ Author :   Kristian Beilke kbeilke@fu-berlin.de
	     adapted from pcsc-lite ifdhandler
/ Date   :   March 05, 2009
/ Purpose:   This provides reader specific low-level calls.
/            See http://www.linuxnet.com for more information.
/ License:   See file LICENSE and PCSC_LICENSE
/
******************************************************************/
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */

#include <pcsclite.h>
#include <ifdhandler.h>
#include <debuglog.h>
#include "serial.h"

/* Maximum number of readers handled */
#define IFDH_MAX_READERS        1

/* Maximum number of slots per reader handled */
#define IFDH_MAX_SLOTS          1


RESPONSECODE IFDHCreateChannel ( DWORD Lun, DWORD Channel ) {

  /* Lun - Logical Unit Number, use this for multiple card slots 
     or multiple readers. 0xXXXXYYYY -  XXXX multiple readers,
     YYYY multiple slots. The resource manager will set these 
     automatically.  By default the resource manager loads a new
     instance of the driver so if your reader does not have more than
     one smartcard slot then ignore the Lun in all the functions.
     Future versions of PC/SC might support loading multiple readers
     through one instance of the driver in which XXXX would be important
     to implement if you want this.
  */
  
  /* Channel - Channel ID.  This is denoted by the following:
     0x000001 - /dev/pcsc/1
     0x000002 - /dev/pcsc/2
     0x000003 - /dev/pcsc/3
     
     USB readers may choose to ignore this parameter and query 
     the bus for the particular reader.
  */

  /* This function is required to open a communications channel to the 
     port listed by Channel.  For example, the first serial reader on COM1 would
     link to /dev/pcsc/1 which would be a sym link to /dev/ttyS0 on some machines
     This is used to help with intermachine independance.
     
     Once the channel is opened the reader must be in a state in which it is possible
     to query IFDHICCPresence() for card status.
 
     returns:

     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
  */
	Log1(PCSC_LOG_DEBUG, "IFDHCreateChannel");
	return getDevice();
}

/* ifd 3.0 uses this */
RESPONSECODE IFDHCreateChannelByName(DWORD Lun, LPSTR deviceName)
{
	Log1(PCSC_LOG_DEBUG, "IFDHCreateChannelByName");
	return getSerialPortByName(deviceName);
}

RESPONSECODE IFDHCloseChannel ( DWORD Lun ) {
  
  /* This function should close the reader communication channel
     for the particular reader.  Prior to closing the communication channel
     the reader should make sure the card is powered down and the terminal
     is also powered down.

     returns:

     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR     
  */
    Log1(PCSC_LOG_DEBUG, "IFDHCloseChannel");

	closeSerialPort();
		
    return IFD_SUCCESS;
}

RESPONSECODE IFDHGetCapabilities ( DWORD Lun, DWORD Tag, 
				   PDWORD Length, PUCHAR Value ) {
  
  /* This function should get the slot/card capabilities for a particular
     slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
     loading a new driver for each reader then ignore Lun.

     Tag - the tag for the information requested
         example: TAG_IFD_ATR - return the Atr and it's size (required).
         these tags are defined in ifdhandler.h

     Length - the length of the returned data
     Value  - the value of the data

     returns:
     
     IFD_SUCCESS
     IFD_ERROR_TAG
  */
	Log1(PCSC_LOG_DEBUG, "IFDHGetCapabilities");
	RESPONSECODE rv;			
    switch (Tag)
    {
    case TAG_IFD_ATR:
		readUID(Length, Value);
		rv = IFD_SUCCESS;
		break;

    case TAG_IFD_SLOTS_NUMBER:
		(*Length) = 1;
		(*Value) = IFDH_MAX_SLOTS;
		rv = IFD_SUCCESS;
		break;

    case TAG_IFD_SIMULTANEOUS_ACCESS:
		(*Length) = 1;
		(*Value) = IFDH_MAX_READERS;
		rv = IFD_SUCCESS;
		break;

    default:
		(*Length) = 0;
		rv = IFD_ERROR_TAG;
    }
	return rv;
}

RESPONSECODE IFDHSetCapabilities ( DWORD Lun, DWORD Tag, 
			       DWORD Length, PUCHAR Value ) {

  /* This function should set the slot/card capabilities for a particular
     slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
     loading a new driver for each reader then ignore Lun.

     Tag - the tag for the information needing set

     Length - the length of the returned data
     Value  - the value of the data

     returns:
     
     IFD_SUCCESS
     IFD_ERROR_TAG
     IFD_ERROR_SET_FAILURE
     IFD_ERROR_VALUE_READ_ONLY
  */
    Log1(PCSC_LOG_DEBUG, "IFDHSetCapabilities");
					   
	return IFD_ERROR_TAG;
}

RESPONSECODE IFDHSetProtocolParameters ( DWORD Lun, DWORD Protocol, 
				   UCHAR Flags, UCHAR PTS1,
				   UCHAR PTS2, UCHAR PTS3) {

  /* This function should set the PTS of a particular card/slot using
     the three PTS parameters sent

     Protocol  - 0 .... 14  T=0 .... T=14
     Flags     - Logical OR of possible values:
     IFD_NEGOTIATE_PTS1 IFD_NEGOTIATE_PTS2 IFD_NEGOTIATE_PTS3
     to determine which PTS values to negotiate.
     PTS1,PTS2,PTS3 - PTS Values.

     returns:

     IFD_SUCCESS
     IFD_ERROR_PTS_FAILURE
     IFD_COMMUNICATION_ERROR
     IFD_PROTOCOL_NOT_SUPPORTED
  */
	Log1(PCSC_LOG_DEBUG, "IFDHSetProtocolParameters");
	if (Protocol != SCARD_PROTOCOL_T0 && Protocol != SCARD_PROTOCOL_T1)
	{
		return IFD_PROTOCOL_NOT_SUPPORTED;
	}
	return IFD_SUCCESS;
}


RESPONSECODE IFDHPowerICC ( DWORD Lun, DWORD Action, 
			    PUCHAR Atr, PDWORD AtrLength ) {

  /* This function controls the power and reset signals of the smartcard reader
     at the particular reader/slot specified by Lun.

     Action - Action to be taken on the card.

     IFD_POWER_UP - Power and reset the card if not done so 
     (store the ATR and return it and it's length).
 
     IFD_POWER_DOWN - Power down the card if not done already 
     (Atr/AtrLength should
     be zero'd)
 
    IFD_RESET - Perform a quick reset on the card.  If the card is not powered
     power up the card.  (Store and return the Atr/Length)

     Atr - Answer to Reset of the card.  The driver is responsible for caching
     this value in case IFDHGetCapabilities is called requesting the ATR and it's
     length.  This should not exceed MAX_ATR_SIZE.

     AtrLength - Length of the Atr.  This should not exceed MAX_ATR_SIZE.

     Notes:

     Memory cards without an ATR should return IFD_SUCCESS on reset
     but the Atr should be zero'd and the length should be zero

     Reset errors should return zero for the AtrLength and return 
     IFD_ERROR_POWER_ACTION.

     returns:

     IFD_SUCCESS
     IFD_ERROR_POWER_ACTION
     IFD_COMMUNICATION_ERROR
     IFD_NOT_SUPPORTED
  */
	Log1(PCSC_LOG_DEBUG, "IFDHPowerICC");
	RESPONSECODE rv;
	switch (Action)
	{
		case IFD_RESET:
		case IFD_POWER_UP:
			readUID(AtrLength, Atr);
			rv = IFD_SUCCESS;
			break;
		default:
			*AtrLength = 0;								
			Atr = 0;
			rv = IFD_NOT_SUPPORTED;
			break;
	}
	return rv;
}

RESPONSECODE IFDHTransmitToICC ( DWORD Lun, SCARD_IO_HEADER SendPci, 
				 PUCHAR TxBuffer, DWORD TxLength, 
				 PUCHAR RxBuffer, PDWORD RxLength, 
				 PSCARD_IO_HEADER RecvPci ) {
  
  /* This function performs an APDU exchange with the card/slot specified by
     Lun.  The driver is responsible for performing any protocol specific exchanges
     such as T=0/1 ... differences.  Calling this function will abstract all protocol
     differences.

     SendPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     TxBuffer - Transmit APDU example (0x00 0xA4 0x00 0x00 0x02 0x3F 0x00)
     TxLength - Length of this buffer.
     RxBuffer - Receive APDU example (0x61 0x14)
     RxLength - Length of the received APDU.  This function will be passed
     the size of the buffer of RxBuffer and this function is responsible for
     setting this to the length of the received APDU.  This should be ZERO
     on all errors.  The resource manager will take responsibility of zeroing
     out any temporary APDU buffers for security reasons.
  
     RecvPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     Notes:
     The driver is responsible for knowing what type of card it has.  If the current
     slot/card contains a memory card then this command should ignore the Protocol
     and use the MCT style commands for support for these style cards and transmit 
     them appropriately.  If your reader does not support memory cards or you don't
     want to then ignore this.

     RxLength should be set to zero on error.

     returns:
     
     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
     IFD_RESPONSE_TIMEOUT
     IFD_ICC_NOT_PRESENT
     IFD_PROTOCOL_NOT_SUPPORTED
  */
    Log1(PCSC_LOG_DEBUG, "IFDHTransmitToICC");

	  RecvPci->Protocol = 1;
	  
	  return sendData(TxBuffer, TxLength, RxBuffer, RxLength, 1);
}

RESPONSECODE IFDHControl(DWORD Lun, DWORD dwControlCode, PUCHAR TxBuffer,
						 DWORD TxLength, PUCHAR RxBuffer,
		DWORD RxLength, PDWORD pdwBytesReturned) {

  /* This function performs a data exchange with the reader (not the card)
     specified by Lun.  Here XXXX will only be used.
     It is responsible for abstracting functionality such as PIN pads,
     biometrics, LCD panels, etc.  You should follow the MCT, CTBCS 
     specifications for a list of accepted commands to implement.

     TxBuffer - Transmit data
     TxLength - Length of this buffer.
     RxBuffer - Receive data
     RxLength - Length of the received data.  This function will be passed
     the length of the buffer RxBuffer and it must set this to the length
     of the received data.

     Notes:
     RxLength should be zero on error.
  */
	Log1(PCSC_LOG_DEBUG, "IFDHControl");
			*pdwBytesReturned = 0;
	return IFD_SUCCESS;
}

RESPONSECODE IFDHICCPresence( DWORD Lun ) {

  /* This function returns the status of the card inserted in the 
     reader/slot specified by Lun.  It will return either:

     returns:
     IFD_ICC_PRESENT
     IFD_ICC_NOT_PRESENT
     IFD_COMMUNICATION_ERROR
  */
	//Log1(PCSC_LOG_DEBUG, "IFDHICCPresence");
	int ret = 0;
	ret = readPresence();
	if (ret == 1)
	{
		Log1(PCSC_LOG_DEBUG, "present");
		return IFD_ICC_PRESENT;
	}
	else
	{
		Log1(PCSC_LOG_DEBUG, "not present");
		return IFD_ICC_NOT_PRESENT;
	}
}

