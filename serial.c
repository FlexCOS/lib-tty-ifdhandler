/*****************************************************************
/
/ File   :   serial.c
/ Author :   Kristian Beilke kbeilke@fu-berlin.de
/ Date   :   March 05, 2011
/ Purpose:   Handles the serial port under linux.
/ License:   See file LICENSE
/
******************************************************************/

#include "serial.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>


struct termios  config;

int com_sock; /* socket for communication */
fd_set socks; /* needed for select call to check for closed connection */
struct timeval timeout; /* needed for select */


/* clean up */
void closeSerialPort()
{
	if (com_sock > 0)
	{
		close(com_sock);
		com_sock = -1;
	}
}

/* not implemented */
RESPONSECODE getDevice()
{
	return IFD_COMMUNICATION_ERROR;
}

/* entry point with serial device as string */
RESPONSECODE getSerialPortByName(char* strAddr)
{	
	com_sock = open(strAddr, O_RDWR | O_NOCTTY | O_NDELAY);
	if (com_sock == -1)
	{
		Log2(PCSC_LOG_CRITICAL, "opening serial port %s failed", strAddr);
		return IFD_COMMUNICATION_ERROR;
	}

	if (!isatty(com_sock))
	{
		Log2(PCSC_LOG_CRITICAL, "not a serial port: %s", strAddr);
		return IFD_COMMUNICATION_ERROR;

	}
	if (tcgetattr(com_sock, &config) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "failed to get options for serial port %s", strAddr);
		return IFD_COMMUNICATION_ERROR;
	}
	
	// Input flags - Turn off input processing
	// convert break to null byte, no CR to NL translation,
	// no NL to CR translation, don't mark parity errors or breaks
	// no input parity check, don't strip high bit off,
	// no XON/XOFF software flow control

	config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
                    INLCR | PARMRK | INPCK | ISTRIP | IXON);

	// Output flags - Turn off output processing
	// no CR to NL translation, no NL to CR-NL translation,
	// no NL to CR translation, no column 0 CR suppression,
	// no Ctrl-D suppression, no fill characters, no case mapping,
	// no local output processing
	
	config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
                     ONOCR | OFILL | OPOST);
	
	// Turn off character processing
	// clear current char size mask, no parity checking,
	// no output processing, force 8 bit input

	config.c_cflag &= ~(CSIZE | PARENB);
	config.c_cflag |= CS8;

	// One input byte is enough to return from read()
	// Inter-character timer off

	config.c_cc[VMIN]  = 2;
	config.c_cc[VTIME] = 0;

	// Communication speed (simple version, using the predefined
	// constants)

	if(cfsetispeed(&config, B115200) < 0 || cfsetospeed(&config, B115200) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "failed to set speed to serial port %s", strAddr);
		return IFD_COMMUNICATION_ERROR;
	}

	// Finally, apply the configuration

	if(tcsetattr(com_sock, TCSAFLUSH, &config) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "failed to set options for serial port %s", strAddr);
		return IFD_COMMUNICATION_ERROR;
	}
	
	return IFD_SUCCESS;
}

/* wrap data, transmit to the device and get answer */
RESPONSECODE sendData(PUCHAR TxBuffer, DWORD TxLength, 
				 PUCHAR RxBuffer, PDWORD RxLength, int wait)
{
	RESPONSECODE rv;
	if (com_sock < 0)
	{
		return IFD_COMMUNICATION_ERROR;
	}

	char apdu[TxLength + 2];
	int j, response_length, n = 0;
	size_t i = 0;
	int n2 = 0;
		
	// always use first 2 bytes byte as length of following data
	apdu[0] = TxLength >> 8 & 0xFF;
	apdu[1] = TxLength & 0xFF;

	for (i = 0; i < TxLength; ++i)
	{
		apdu[i + 2] = *(TxBuffer + i);
	}

	//// check if write is possible
	//FD_ZERO(&socks);
	//FD_SET(com_sock, &socks);
	////sock_max = s + 1;
	//n = select(com_sock + 1, (fd_set *) 0, &socks, (fd_set *) 0, &timeout);
	//if (n < 1)
	//{
	//	return IFD_COMMUNICATION_ERROR;
	//}

    	i = write(com_sock, &apdu, TxLength + 2);

	if (i < TxLength + 2)
	{
		Log3(PCSC_LOG_CRITICAL, "failed to write %d bytes, only wrote %d", TxLength, i);
		return IFD_COMMUNICATION_ERROR;
	}

	Log2(PCSC_LOG_DEBUG, "wrote %d data bytes", TxLength + 2);
	char buffer[65538];
	
sleep(1);
	n = 0;
	if (wait > 0)
	{
		// use select to recognize broken connection
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		FD_ZERO(&socks);
		FD_SET(com_sock, &socks);
		n = select(com_sock + 1, &socks, (fd_set *) 0, (fd_set *) 0, &timeout);
		
		if (n < 0)
		{
			Log1(PCSC_LOG_ERROR, "did not find any data");
			return IFD_COMMUNICATION_ERROR;
		}
		else if (n == 0 || !FD_ISSET(com_sock, &socks))
		{
			Log1(PCSC_LOG_ERROR, "zero byte response");
			*RxLength = 1;
			*RxBuffer = 0;
			return IFD_SUCCESS;
		}
		else
		{
			n = read(com_sock, &buffer, sizeof(buffer));
		}
	}
	else
	{
		n = read(com_sock, &buffer, sizeof(buffer));
	}
	
	if (n > 0) 
	{
		if (n == 1)
		{
			Log1(PCSC_LOG_DEBUG, "1 byte error response");
			// handle errors
			return IFD_COMMUNICATION_ERROR;
	    	}
		
	    	Log2(PCSC_LOG_DEBUG, "got %d response bytes", n);
	    	// unwrap response
		response_length = (int) (0xFF & buffer[0]) << 8;
		response_length += (int) (0xFF & buffer[1]);

	   	if (response_length != n - 2)
	    	{
			*RxLength = response_length;
			
			i = 0;
			n2 = n;
			
			for (j = 2; j < n2; ++i)
			{
				*(RxBuffer + i) = buffer[j];
				++j;
			}
			
		    	// try to read more
			while (n - 2 < response_length)
			{	
				n2 = 0;
				
				Log1(PCSC_LOG_DEBUG, "rereceive");
				
				n2 = read(com_sock, buffer, sizeof(buffer));
				
				if (n2 > 0)
				{
					Log2(PCSC_LOG_DEBUG, "got %d additional response bytes", n2);
					n += n2;
					
					for (j = 0; j < n2; ++i)
					{
						*(RxBuffer + i) = buffer[j];
						++j;
					}
				}
				else
				{
					Log3(PCSC_LOG_ERROR, "response length wrong, is: %d  should be: %d",
						n - 2, response_length);
		    		return IFD_COMMUNICATION_ERROR;
				}
			}
			
		    Log3(PCSC_LOG_DEBUG, "concated response length is: %d  should be: %d",
				i, response_length);
			
			rv = IFD_SUCCESS;
	    }
	    else
	    {
		    *RxLength = response_length;
		    for (i = 0; i < response_length; ++i)
		    {
			    *(RxBuffer + i) = buffer[2 + i];
		    }
		    //Log_Xxd(PCSC_LOG_INFO, "RAPDU: ", *RxBuffer, *RxLength);
		    rv = IFD_SUCCESS;
	    }
	}
	else
	{
	    Log1(PCSC_LOG_ERROR,
			 "no response, meaning eof, reader not usable anymore\n");
		closeSerialPort();
	    rv = IFD_COMMUNICATION_ERROR;
	}
	return rv;
}

/* get the uid from the device and return it as an atr */
void readUID(PDWORD Length, PUCHAR Value)
{
	//cmd[0] = CMD_BYTE;
	//cmd[1] = ATS_BYTE;
	//DWORD rxLength;
	//int i ,j;
	char atr[MAX_ATR_SIZE - 4];
	
	// send the ats cmd
	//if (sendData(&cmd, sizeof(cmd), &atr, &rxLength, 0) == IFD_SUCCESS)
	//{
		// construct standart contactless ATR
		*(Value + 0) = 0x3B; //TS direct convention
		*(Value + 1) = 0x8A; //T0 TD1 available, 8 historical bytes
		*(Value + 2) = 0x80; //TD1 TD2 follows, protocol T0
		*(Value + 3) = 0x01; //TD2 no Tx3, protocol T1
		/*j = 4;*/
		char crc = 0x8A ^ 0x80 ^ 0x01;
		/*for (i = 0; i < rxLength; ++i)
		{
			*(Value + i + j) = atr[i];
			crc ^= atr[i];
		}*/

		*(Value + 4) = 0x53;
		crc ^= 0x53;
		*(Value + 5) = 0x4f;
		crc ^= 0x4f;
		*(Value + 6) = 0x53;
		crc ^= 0x53;
		*(Value + 7) = 0x53;
		crc ^= 0x53;
		*(Value + 8) = 0x45;
		crc ^= 0x45;
		*(Value + 9) = 0x02;
		crc ^= 0x02;
		*(Value + 10) = 0x03;
		crc ^= 0x03;
		*(Value + 11) = 0x25;
		crc ^= 0x25;
		*(Value + 12) = 0x01;
		crc ^= 0x01;
		*(Value + 13) = 0x03;
		crc ^= 0x03;
		*(Value + 14) = crc;
		*Length = 15;
/*	}
	else
	{
		*Length = 0;
	}*/
}

/* answer to the pcscd polling */
int readPresence()
{
	return 1;/*
	// construct presence cmd
	cmd[0] = CMD_BYTE;
	cmd[1] = PRS_BYTE;
    DWORD rxLength = 0;
	char response[1];
	
	// send cmd
	if (sendData(&cmd, sizeof(cmd), &response, &rxLength, 1) == IFD_SUCCESS)
	{
		// if icc present the response will be just 0x1
		if (rxLength == 1)
		{
			return (int) response[0];
		}
	}
	return 0;*/
}
