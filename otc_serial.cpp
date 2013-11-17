/// @copyright
//
/// =========================================================================
/// Copyright 2012 WizziLab
///
/// Licensed under the Apache License, Version 2.0 (the License);
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
/// http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an AS IS BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
/// =========================================================================
//
/// @endcopyright
//
/// @file           otc_serial.cpp
/// @brief          Serial link toolkit
///                 Functions for reading, buffering and parsing serial data
///
/// @todo JPN: Update the socket features and the treatment of socket packets
///            to use MPipe2.
/// =========================================================================
  
#ifndef WIN32

#include <iostream>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */

#else

#include <windows.h>
#include "stdio.h"

#endif

#include <qstring.h>
#include "otc_main.h"
#include "otc_serial.h"


// Constructor... ok, this comment might not be very useful.
otc_serial::otc_serial()
{
	m_bOpened =  false;
    m_nBaud = 0;

    dbgprint("Hello, I'm the serial context.\n");

#ifndef WIN32

    m_serialHandle = 0;

#else

    hComDev =  NULL;
	bCommandActive =  false;
	bEventWaiting =  false;
	memset(&osReader,0,sizeof(OVERLAPPED));
	memset(&osWriter,0,sizeof(OVERLAPPED));
	memset(&osStatus,0,sizeof(OVERLAPPED));
	buf_rd_idx =  0;
	buf_wr_idx =  0;
	buf_serial_active =  false;

	osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osReader.hEvent == NULL)
	{
		ioprint("Error creating overlapped event for read.\n");
		return;
	}

	osWriter.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osWriter.hEvent == NULL)
	{
		ioprint("Error creating overlapped event for write.\n");
		CloseHandle(osReader.hEvent);
		return;
	}

	osStatus.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osStatus.hEvent == NULL)
	{
		ioprint("Error creating overlapped event for com event wait.\n");
		CloseHandle(osReader.hEvent);
		CloseHandle(osWriter.hEvent);
		return;
	}

	hReadReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hReadReadyEvent == NULL)
	{
		ioprint("Error creating event for com read event wait.\n");
		CloseHandle(osReader.hEvent);
		CloseHandle(osWriter.hEvent);
		CloseHandle(osStatus.hEvent);
		return;
	}

	InitializeCriticalSection (& cs);

#endif // WIN32
}


// Destructor... ok, this comment might not be very useful.
otc_serial::~otc_serial()
{
    if (m_bOpened) sclose();

#ifndef WIN32
#else // WIN32

    CloseHandle(osReader.hEvent);
    CloseHandle(osWriter.hEvent);
    CloseHandle(osStatus.hEvent);
    CloseHandle(hReadReadyEvent);
    DeleteCriticalSection (&cs);

#endif // WIN32
}



// Open the serial port identified by the string szPort at nBaud rate.
bool otc_serial::sopen(char *szPort, int nBaud, OTC_FLOW_T mode, bool timeoutblock=true)
{
    if (m_bOpened) return true;
    m_nBaud = nBaud;

#ifndef WIN32

    struct termios options;

    m_serialHandle = open(szPort, O_RDWR | O_NOCTTY | O_NDELAY);

    // Error check.
    if (m_serialHandle == -1)
    {
        ioprint1("Failed to open %s.\n", szPort);
        return false;
    }
    m_bOpened = true;

    if (timeoutblock)
    {
        // Enable the blocking read.
        fcntl(m_serialHandle, F_SETFL, 0);
    }
    else
    {
        // Disable the blocking read.
        fcntl(m_serialHandle, F_SETFL, FNDELAY);
    }

    // Get the current options for the port, change a few things and set them.
    tcgetattr(m_serialHandle, &options);
    cfsetispeed(&options, B115200); // set a default rate, then updated with baudrate
    cfsetospeed(&options, B115200);
    ioprint1("get options.c_iflag= %08o.\n", options.c_iflag);
    ioprint1("get options.c_oflag= %08o.\n", options.c_oflag);
    ioprint1("get options.c_cflag= %08o.\n", options.c_cflag);
    ioprint1("get options.c_lflag= %08o.\n", options.c_lflag);
#ifndef __APPLE__
    options.c_iflag &= ~(IUCLC);
#endif  // ! __APPLE__
    options.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);
    options.c_iflag |= 0;
    if (mode==OTC_FLOW_XONXOFF)
        options.c_iflag |= IXON | IXOFF | IXANY;

#ifndef __APPLE__
    options.c_oflag &= ~(OLCUC);
#endif  // ! __APPLE__
    options.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET | OFILL | OFDEL);
    options.c_oflag |= 0;
    options.c_cflag &= ~(CSTOPB | PARENB | PARODD);
    options.c_cflag &= ~CSIZE; /* Mask the character size bits */
    options.c_cflag |= CS8;    /* Select 8 data bits */
    options.c_cflag |= ( CLOCAL | CREAD | HUPCL );
    // hardware flow control:
    if (mode==OTC_FLOW_HARDWARE)
        options.c_cflag |= CRTSCTS;
    else
        options.c_cflag &= ~CRTSCTS;
    /*non cannonical mode : ~ICANON */
    options.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH | TOSTOP | ECHOCTL | ECHOPRT | ECHOKE | FLUSHO | PENDIN | IEXTEN);
    options.c_lflag |= 0;
    
    ioprint1("set options.c_iflag= %08o.\n", options.c_iflag);
    ioprint1("set options.c_oflag= %08o.\n", options.c_oflag);
    ioprint1("set options.c_cflag= %08o.\n", options.c_cflag);
    ioprint1("set options.c_lflag= %08o.\n", options.c_lflag);
    
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0;

    // define xon xoff char
    options.c_cc[VSTART] = 0x11;
    options.c_cc[VSTOP]  = 0x13;

    tcsetattr(m_serialHandle, TCSANOW, &options);

    // Change the baudrate.
    if (!baudrate(nBaud))
    {
        sclose();
        return false;
    }

#else // WIN32

    hComDev = CreateFileA( szPort, GENERIC_READ | GENERIC_WRITE, 0,
                          NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL );
    if( hComDev == INVALID_HANDLE_VALUE)
    {
        ioprint1("Failed to open %s.\n", szPort);
        return false;
    }

    dcb.DCBlength = sizeof( dcb );
    GetCommState( hComDev, &(dcb) );
    dcb.BaudRate = nBaud;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    //Flow control: HARDWARE:1, XONXOFF:0, NONE:0
    dcb.fOutxCtsFlow = (mode==OTC_FLOW_HARDWARE)?TRUE:FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;

    //Flow control: HARDWARE:0, XONXOFF:1, NONE:0
    if(mode==OTC_FLOW_XONXOFF)
    {
        dcb.fInX    =TRUE; // XON/XOFF
        dcb.fOutX   =TRUE;
        dcb.XonChar =0x11;
        dcb.XoffChar=0x13;
    }
    else
    {
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
    }

    dcb.fNull = FALSE;
    //Flow control: HARDWARE:1, XONXOFF:0, NONE:0
    dcb.fRtsControl = (mode==OTC_FLOW_HARDWARE)?RTS_CONTROL_ENABLE:RTS_CONTROL_DISABLE;//RTS_CONTROL_HANDSHAKE
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if( !SetCommState( hComDev, &dcb ) || !SetupComm( hComDev, 100, 100 ) )
    {
        unsigned int dwError = GetLastError();
        ioprint1("Failed to assign dcb structure error: 0x%x.\n",dwError);
        CloseHandle( hComDev );
        return false;
    }

    COMMTIMEOUTS CommTimeOuts;

    if(!timeoutblock)
    {
        // read timeout is 1bit time + nb_bytes * bytetime
        //	CommTimeOuts.ReadIntervalTimeout = MAXunsigned int; //(1000/s.nBaud)+1 ; // 1ms
        CommTimeOuts.ReadIntervalTimeout = 0; //(1000/s.nBaud)+1 ; // 1ms
        CommTimeOuts.ReadTotalTimeoutMultiplier = 0; //(11000/s.nBaud)+1 ; //10 bits per byte
        CommTimeOuts.ReadTotalTimeoutConstant = 0; //(READ_TIMEOUT/s.nBaud) +1;   // constant value
        // write timeout is 1bit time + nb bytes * byte time
        CommTimeOuts.WriteTotalTimeoutMultiplier = 0; //(100000/s.nBaud)+1;
        CommTimeOuts.WriteTotalTimeoutConstant = 0; //(WRITE_TIMEOUT/s.nBaud) +1;
    }
    else
    {
        //Reuse stacktrace parameters
        CommTimeOuts.ReadIntervalTimeout = (DWORD)-1;
        CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
        CommTimeOuts.ReadTotalTimeoutConstant = 0;
        CommTimeOuts.WriteTotalTimeoutMultiplier = 1;
        CommTimeOuts.WriteTotalTimeoutConstant = 1000;
    }

    if (SetCommTimeouts( hComDev, &CommTimeOuts )==0)
    {
        ioprint("Failed to assign timeouts.\n");
        //unsigned int dwError =
        GetLastError();
        CloseHandle( hComDev );
        return false;
    }

    bSerialClosing = false;
    m_bOpened = true;
    m_nBaud = nBaud;

#endif // WIN32

    ioprint2("Port %s open at %d bauds.\n", szPort, nBaud);

    return m_bOpened;
}



// Close the serial interface.
void otc_serial::sclose(void)
{
    /* Check if the port is opened. */
    if (!m_bOpened) return;

#ifndef WIN32

    close(m_serialHandle);

#else // WIN32

    if (hComDev == NULL) return;

    EnterCriticalSection (&cs);
    bSerialClosing = true;
    SetCommMask(hComDev, 0);
    Sleep(1);//to let the thread waiting on CommEvent out

    m_bOpened=false;
    LeaveCriticalSection (& cs);
    CloseHandle(hComDev);

#endif //WIN32

    m_bOpened = false;
    m_nBaud = 0;
}


// Change the baude rate of the serial interface.
bool otc_serial::baudrate(int nBaud)
{
    /* Check if the port is opened. */
    if (!m_bOpened) return false;

#ifndef WIN32

    struct termios options;

    speed_t speed;

#ifdef __APPLE__
    speed = (speed_t) nBaud;
#else // ! __APPLE__
    /* convert nBaud to POSIX speed_t*/
    switch (nBaud) 
    {
    case 0:
        speed = B0;
        break;
    case 50:
        speed = B50;
        break;
    case 75:
        speed = B75;
        break;
    case 110:
        speed = B110;
        break;
    case 134:
        speed = B134;
        break;
    case 150:
        speed = B150;
        break;
    case 200:
        speed = B200;
        break;
    case 300:
        speed = B300;
        break;
    case 600:
        speed = B600;
        break;
    case 1200:
        speed = B1200;
        break;
    case 1800:
        speed = B1800;
        break;
    case 2400:
        speed = B2400;
        break;
    case 4800:
        speed = B4800;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    case 230400:
        speed = B230400;
        break;
    case 460800:
        speed = B460800;
        break;  
    case 500000:
        speed = B500000;
        break;  
    case 576000:
        speed = B576000;
        break;  
    case 921600:
        speed = B921600;
        break;  
    case 1000000:
        speed = B1000000;
        break; 
    case 1152000:
        speed = B1152000;
        break; 
    case 1500000:
        speed = B1500000;
        break; 
    case 2000000:
        speed = B2000000;
        break; 
    case 2500000:
        speed = B2500000;
        break; 
    case 3000000:
        speed = B3000000;
        break; 
    case 3500000:
        speed = B3500000;
        break; 
    case 4000000:
        speed = B4000000;
        break;
    default:
        // raise?
        speed = B921600;
    }
#endif // ! __APPLE__

    // Get the current options for the port, change the baudrate and set them.
    tcgetattr(m_serialHandle, &options);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    tcsetattr(m_serialHandle, TCSANOW, &options);

#else // WIN32

    if (hComDev == NULL) return false;

    /* change baudrate in s.dcb struct */
    dcb.BaudRate = nBaud;

    if( !SetCommState( hComDev, &dcb )  )
    {
        unsigned int dwError = GetLastError();
        ioprint1("Failed to assign s.dcb structure error: 0x%x.\n",dwError);
        return false;
    }

#endif // WIN32

    m_nBaud = nBaud;

    return true;
}

bool otc_serial::flowmode(OTC_FLOW_T mode)
{
    /* Check if the port is opened. */
    if (!m_bOpened) return false;

#ifndef WIN32

    struct termios options;

    // Get the current options for the port, change the baudrate and set them.
    tcgetattr(m_serialHandle, &options);
    if (mode==OTC_FLOW_XONXOFF)
    {
        options.c_iflag |= IXON | IXOFF | IXANY;
        // define xon xoff char
        options.c_cc[VSTART] = 0x11;
        options.c_cc[VSTOP]  = 0x13;
    }
    else
        options.c_iflag &= ~(IXON | IXOFF | IXANY);

    // hardware flow control:
    if (mode==OTC_FLOW_HARDWARE)
        options.c_cflag |= CRTSCTS;
    else
        options.c_cflag &= ~CRTSCTS;

    tcsetattr(m_serialHandle, TCSANOW, &options);


#else // WIN32

    if (hComDev == NULL)
        return false;

    //HARDWARE: xonxoff:0, flowcontrol:1
    //XONXOFF:  xonxoff:1, flowcontrol:0
    //NONE:     xonxoff:0, flowcontrol:0

    if(mode==OTC_FLOW_XONXOFF)
    {
        dcb.fInX    =TRUE; // XON/XOFF
        dcb.fOutX   =TRUE;
        dcb.XonChar =0x11;
        dcb.XoffChar=0X13;
    }
    else
    {
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
    }

    if(mode==OTC_FLOW_HARDWARE)
    {
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
    }
    else
    {
        dcb.fOutxCtsFlow = FALSE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
    }

    if( !SetCommState( hComDev, &dcb )  )
    {
        unsigned int dwError = GetLastError();
        ioprint1("Failed to assign s.dcb structure error: 0x%x.\n",dwError);
        return false;
    }
#endif // WIN32

    return true;
}


// Read len bytes for the serial interface.
int otc_serial::sread(void *buffer, unsigned int len)
{
    /* Check if the port is opened. */
    if (!m_bOpened) return 0;

    unsigned long nbRead = 0;

#ifndef WIN32

    // Reads on the serial interface, until we get enough data or a timeout.
    //while (nbRead < len)
    {
        int tmpNbRead;

        if ( 0 >= (tmpNbRead = read(m_serialHandle,
                                    (char*)buffer + nbRead,
                                    len - nbRead)) )
        {
            return nbRead;
        }

        nbRead += tmpNbRead;
    }

#else // WIN32

    if (hComDev == NULL) return 0;

    BOOL bReadStatus;
    unsigned long dwErrorFlags;
    COMSTAT ComStat;
    unsigned int dwError;

    ClearCommError( hComDev, &dwErrorFlags, &ComStat );

    if (dwErrorFlags!=0)
    {
        return -1;
    }

    bReadStatus = ReadFile( hComDev, buffer, len, &nbRead, &osReader );

    if(!bReadStatus)
    {
        dwError = GetLastError();
        if ( dwError!= ERROR_IO_PENDING)
        {
            //D  otcConfig::logText(QString("1.ReadData error: %1 bytes read, error code %2.").arg(nbRead).arg(dwError));
        }
        else
        {	// IO Pending
            unsigned int dwRes = WaitForSingleObject(osReader.hEvent, INFINITE);
            switch(dwRes)
            {
                    // Read completed.
                case WAIT_OBJECT_0:
                    if (!GetOverlappedResult(hComDev, &osReader, &nbRead, FALSE))
                    {
                        // Error in communications; report it.
                        //D  otcConfig::logText(QString("2.ReadData error: %1 bytes read, error code %2.").arg(nbRead).arg(dwError));
                    }
                    break;

                    default:
                    // Error in the WaitForSingleObject; abort. This indicates a problem with the OVERLAPPED structure's event handle.
                    //D  otcConfig::logText(QString("3.ReadData error: %1 bytes read, error code %2.").arg(nbRead).arg(dwError));
                    break;
            }
        }
    }

#endif // WIN32

    return nbRead;
}



// Write len bytes to the serial interface.
int otc_serial::swrite(const char *buffer, unsigned int len)
{
    /* Check if the port is opened. */
    if (!m_bOpened) return 0;

#ifndef WIN32

    int nbWritten;

    nbWritten = write(m_serialHandle, buffer, len);

    // The method does not support error returns.
    if (nbWritten < 0)
    {
        nbWritten = 0;
    }

    return nbWritten;

#else // WIN32

    if (hComDev == NULL) return 0;

    BOOL bWriteStat=false;


    bWriteStat = WriteFile( hComDev, buffer, len,
                           (DWORD*)&len, &osWriter );

    if( !bWriteStat)
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            //D  otcConfig::logText(QString("SendData timeout: %1 bytes sent").arg(len));
        }
        else
        {
            // Write is pending.
            unsigned int dwRes = WaitForSingleObject(osWriter.hEvent, SERIALWAIT_TIMEOUT);
            switch(dwRes)
            {
                    // OVERLAPPED structure's event has been signaled.
                case WAIT_OBJECT_0:
                    if (!GetOverlappedResult(hComDev, &osWriter, (DWORD*)&len, FALSE))
                    {
                        //D otcConfig::logText(QString("SendData timeout: %1 bytes sent").arg(len));
                    }
                    break;

                    case WAIT_TIMEOUT:
                    otcConfig::logText("write: Timeout waiting for osWriter.hEvent.");
                    // cancel pending WaitCommEvent
                    //if(!CancelIo(hComDev)) {
                     //   otcConfig::logText(QString("write: Error in CancelIo (%1)").arg(GetLastError()));
                    //}
                    otcConfig::logText("Timeout out.");
                    return 0;

                    break;
                    default:
                    // An error has occurred in WaitForSingleObject.
                    // This usually indicates a problem with the
                    // OVERLAPPED structure's event handle.
                    //D  otcConfig::logText(QString("SendData Error: %1 bytes sent.").arg(len));
                    break;
            }
        }
    }

    return len;

#endif // WIN32
}



// Send a break on the serial interface.
void otc_serial::dbgbreak(void)
{
    if (!m_bOpened) return;

#ifndef WIN32

    tcsendbreak(m_serialHandle, 1 /* not used */);

#else // WIN32

    EnterCriticalSection (& cs);

    if(SetCommBreak(hComDev))
    {
        Sleep((13000/m_nBaud)+100); // 13 bits + 100 us
		if(!ClearCommBreak(hComDev))
		{
            ioprint1("error clearing break %d", (int)GetLastError());
		}
	}
	else
	{
        ioprint1("error doing break %d", (int)GetLastError());
	}

	LeaveCriticalSection (& cs);

#endif // WIN32
}


// Flush the serial fifo.
void otc_serial::flush()
{
	if (!m_bOpened) return;

#ifndef WIN32

    tcflush(m_serialHandle, TCIOFLUSH);

#else // WIN32

	FlushFileBuffers(hComDev);

#endif // WIN32
}


#ifndef WIN32
#include "sys/time.h"
int GetTickCount(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif


// ---------------------------------------- //
//                                          //
//           SERIAL READ ENGINE             //
//                                          //
// ---------------------------------------- //

void otcDataParser::lock()
{
    m_bufferMutex.lock();
}

void otcDataParser::unlock()
{
    m_bufferMutex.unlock();
}

otcDataParser::otcDataParser()
{
	m_size			= 4*0x10000; 
	m_buffer		= new unsigned char[m_size];
	C_feed = 0;
	C_untreated = 0;
}

void otcDataParser::reinit()
{
    lock();
	C_feed = 0;
	C_untreated = 0;
	unlock();
}

otcDataParser::~otcDataParser()
{
	delete m_buffer;
	m_buffer = NULL;
}

int otcDataParser::wrap(int i)
{
	int iw = 0;
	if(i<0)
	{
		iw = m_size - ( (-i)%m_size);
		if(iw==(int)m_size)
			iw = 0;
	}
	else
		iw = i%m_size;

	return iw;
}

unsigned char otcDataParser::at(int i)
{
	i = wrap(i);
	return m_buffer[i];
}

bool otcDataParser::isInValidRange(int i)
{
	i = wrap(i);

	if(C_feed >= C_untreated)
		return (i>=C_untreated && i < C_feed);
	else
		return (i<C_feed || i>= C_untreated);
}

int otcDataParser::uneatenBytesFrom(int i)
{
	i = wrap(i);

	if(!isInValidRange(i))
		return 0;

	// we assume that i is in valid range
	if(C_feed >= C_untreated) 
		return C_feed - i;
	else if(i<C_feed)
		return C_feed - i;
	else
		return C_feed + m_size - i;
}

int otcDataParser::eatAsMuchAsPossibleFromSerial(otcCommunicationLinkDevice& device)
{
	if(C_feed < C_untreated)
	{
		int bytesavail = C_untreated - C_feed - 1; // keep 1 empty cell between both

		if(bytesavail<0) bytesavail = 0;

		int read = device.readBlock(m_buffer+C_feed,bytesavail);
		if(read<=0)	read = 0;
		C_feed += read; //advance
		C_feed =  wrap(C_feed);
		return read;
	}
	else
	{
		int avail1 = m_size - C_feed;
		int avail2 = C_untreated;

        if(!avail2) avail1--; //keep 1 cell empty
        else        avail2--;

        if(avail1<0) avail1 = 0;
        if(avail2<0) avail2 = 0;

		int read1=0,read2=0;

		read1 = device.readBlock(m_buffer+C_feed,avail1);

        if(read1<=0) read1 = 0;
        C_feed +=read1;
        C_feed = wrap(C_feed);

        if(avail1==read1) //need to continue reading
        {
            read2 = device.readBlock(m_buffer+C_feed,avail2);
            if(read2<=0) read2 = 0;
            C_feed+=read2;
            C_feed =  wrap(C_feed);
        }

        return read1+read2;
	}
}


QString otcDataParser::getStatus()
{
    QString ret =QString("Feed: %1, Treated: %2.\n").arg(C_feed).arg(C_untreated);
    ret += "Not treated content:\n";
    ret += "<font color=blue>\n";

    int notTreated = 0;
    if(C_feed >= C_untreated)
        notTreated = C_feed - C_untreated;
    else
        notTreated = C_feed + m_size - C_untreated;

    for(int uu=0;uu<notTreated;uu++)
    {
        if(uu%16==0 && uu!=0)
            ret+="\n";

        ret += QString("%1 ").arg(QString("%1").arg(at(C_untreated+uu),2,16).upper().replace(' ','0'));
    }
    ret += "</font>";

    return ret;
}

int otcDataParser::readDataFromDevice(otcCommunicationLinkDevice& device)
{
    lock();
    int ret = eatAsMuchAsPossibleFromSerial(device);
    unlock();

    return ret;
}

void otcDataParser::treatSendData(otcHostServer& hostserver)
{
    static unsigned char m_customHeader[4];

    int tosend = 0;

    if(C_feed < C_untreated)
        tosend = m_size - C_untreated; 
    else
        tosend = C_feed - C_untreated;

    int plen = tosend+4;

    if(tosend==0)
        return;

    hostserver.lock();

    // Send data to all clients.
    otcHostClientLink* c = hostserver.getClientListUnprotected();
    while(c)
    {
        otcHostClient* client = c->client;

        if(client)
        {
			// TODO : remove this header
            m_customHeader[0] = OTC_PROTOCOL_SYNC;
            m_customHeader[1] = ((tosend&0xFF00)>>8);
            m_customHeader[2] = ((tosend&0x00FF));
            m_customHeader[3] = OTC_PROTOCOL_RAW_DATA;

            client->writeBlock((char*)m_customHeader,4);
            client->writeBlock(((char*)m_buffer)+C_untreated,plen);
        }
        c = c->next;
    }

	if (otcConfig::argPrintMode == OTC_PRINT_MODE_RAW)
	{
        QString msg="<font color=blue>";
        for(int cc=0;cc<tosend;cc++)
        {
            if (m_ndef.sync(m_buffer+C_untreated+cc))
            {
                msg+="\n";
            }
            msg+=QString("%1 ").arg(QString("%1").arg(m_buffer[C_untreated+cc],2,16).upper().replace(' ','0'));
        }
        msg+="</font>";
        otcConfig::logText(msg);
	}
	else // if (otcConfig::argPrintMode == OTC_PRINT_MODE_NDEF_PLUS_OT)
	{
		m_ndef.parse(m_buffer+C_untreated,tosend);
	}

    // Update buffer markers
    if(C_feed < C_untreated)
        C_untreated = 0;
    else
        C_untreated = C_feed;

    hostserver.unlock();
}


void otcDataParser::dataTreatmentLoop(otcHostServer& hostserver)
{
    lock();
    treatSendData(hostserver);
    unlock();
}

