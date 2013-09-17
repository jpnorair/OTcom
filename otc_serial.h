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
/// @file           otc_serial.h
/// @brief          Definitions and classes for the serial link toolkit
//
/// =========================================================================

#ifndef OTC_SERIAL_H
#define OTC_SERIAL_H

#include <qmutex.h>
#include <qtextedit.h>
#include <qtcpsocket.h>
#include <q3socket.h>

#include "otc_main.h"
#include "otc_socket.h"
#include "otc_mpipe.h"



#ifndef WIN32

#else // WIN32

#include <windows.h>
// private data
#define BUF_WRITE_SIZE 1024

#endif // WIN32

#define SERIALWAIT_TIMEOUT		3000 // 2s

/* IO functions. */
#define ioprint(x)				printf(x);
#define ioprint1(x,y1)			printf(x,y1);
#define ioprint2(x,y1,y2)		printf(x,y1,y2);
#define ioprint3(x,y1,y2,y3)	printf(x,y1,y2,y3);

#ifdef DEBUG
#define dbgprint(x)				ioprint(x)
#define dbgprint1(x,y1)			ioprint1(x,y1)
#define dbgprint2(x,y1,y2)		ioprint2(x,y1,y2)
#define dbgprint3(x,y1,y2,y3)	ioprint3(x,y1,y2,y3)
#else
#define dbgprint(x)
#define dbgprint1(x,y1)
#define dbgprint2(x,y1,y2)
#define dbgprint3(x,y1,y2,y3)
#endif


class otc_serial
{

public :
        otc_serial();
        ~otc_serial();
        bool                    sopen(char *szPort, int nBaud,OTC_FLOW_T mode,bool timeoutblock);
        void                    sclose(void);
        bool                    baudrate(int nBaud);
        bool                    flowmode(OTC_FLOW_T mode);
        int                     sread(void *buffer, unsigned int len);
        int                     swrite(const char *buffer, unsigned int len);
        void                    flush(void);
        void                    dbgbreak(void);
        bool                    hasComOpened() {return m_bOpened;}

protected :
        bool                    m_bOpened;
        int	                    m_nBaud;

#ifndef WIN32

        int                     m_serialHandle;

#else // WIN32

        HANDLE                  hComDev;
        bool                    bCommandActive;
        bool                    bEventWaiting;
        OVERLAPPED              osReader;
        OVERLAPPED              osWriter;
        OVERLAPPED              osStatus;
        int                     buf_rd_idx;
        int                     buf_wr_idx;
        bool                    buf_serial_active;
        /* no init members bellow */
        CRITICAL_SECTION        cs;
        HANDLE	                hReadReadyEvent;
        bool                    bSerialClosing;
        DCB                     dcb;
        char                    buffer[BUF_WRITE_SIZE];

#endif //WIN32

};

class otcCommunicationLinkDevice
{
    public :
        otcCommunicationLinkDevice(){};
        bool isOpen();
        void flush();
        void dbgBreak();
        bool changeBaudRate(int nBaud);
        bool changeFlowMode(OTC_FLOW_T mode);
        int  readBlock (unsigned char *buffer, unsigned int len);
        int  writeBlock(const char *buffer, unsigned int len);
        bool socketOpen(int port);
        bool serialOpen(char *szPort, int nBaud, OTC_FLOW_T mode, bool timeoutblock);
        void close();
        void lock();
        void unlock();

    protected :
        otc_serial              m_serialContext;
        Q3SocketDevice          m_socketServer;
        Q3SocketDevice          m_socketContext;
        QMutex                  m_deviceMutex;
    private :
        int  writeBlockXonXoff(unsigned char* buffer,unsigned int len);

};

void dop();
void dcl();
bool dIsOpen();

class otcHostServer;

class otcDataParser
{
protected :

    QMutex              m_bufferMutex;
	bool			    m_receivedOkPacket;
	unsigned int	    m_size;
	unsigned char*	    m_buffer;
	otc_mpipe_parser    m_ndef;
	int                 C_feed;
	int                 C_untreated;
	
	int                 wrap(int i);
	unsigned char       at(int i);
	bool                isInValidRange(int i);
	int                 uneatenBytesFrom(int i);
	int                 eatAsMuchAsPossibleFromSerial(otcCommunicationLinkDevice& device);
    void                treatSendData(otcHostServer& hostserver);

public :

	otcDataParser();
	~otcDataParser();

	void                reinit();
    int                 readDataFromDevice(otcCommunicationLinkDevice& device);
    void                dataTreatmentLoop(otcHostServer& hostserver);
    void                lock();
    void                unlock();
    QString             getStatus();

};

#endif // OTC_SERIAL_H
