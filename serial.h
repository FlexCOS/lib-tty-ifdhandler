/*****************************************************************
/
/ File   :   serial.h
/ Author :   Kristian Beilke kbeilke@fu-berlin.de
/ Date   :   March 05, 2011
/ Purpose:   Handles the serial port under linux.
/ License:   See file LICENSE
/
******************************************************************/

#ifndef _sereial_h_
#define _serial_h_

#include <pcsclite.h>

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */

#include <pcsclite.h>
#include <ifdhandler.h>
#include <debuglog.h>

#ifdef __cplusplus
extern "C"
{
#endif

void closeSerialPort();
RESPONSECODE getDevice();
RESPONSECODE getSerialPortByName(char*);
RESPONSECODE sendData(PUCHAR, DWORD, PUCHAR, PDWORD, int);
void readUID(PDWORD, PUCHAR);
int readPresence();

#ifdef __cplusplus
}
#endif

#endif
