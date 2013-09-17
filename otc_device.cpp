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
/// @file           otc_device.cpp
/// @brief          OTCOM device object
//
/// =========================================================================

#include <stdio.h>
#include "otc_main.h"
#include "otc_serial.h"

FILE* fdebug = NULL;

void ddebug(unsigned char* buffer,int len)
{
    if(!fdebug)
        return;

    fwrite(buffer,len,1,fdebug);
}

void dcl()
{
    if(!fdebug)
        return;
    fclose(fdebug);
    fdebug = NULL;
}

void dop()
{
    if(fdebug)
        dcl();
    fdebug = fopen("debug.bin","wb");
    if(!fdebug)
        return;
}

bool dIsOpen()
{
    return (fdebug!=NULL);
}

void otcCommunicationLinkDevice::lock()
{
    m_deviceMutex.lock();
}

void otcCommunicationLinkDevice::unlock()
{
    m_deviceMutex.unlock();
}

bool otcCommunicationLinkDevice::isOpen()
{
    return m_serialContext.hasComOpened();
}

void otcCommunicationLinkDevice::flush() //used for autobauding
{
    lock();
    m_serialContext.flush();
    unlock();
}

void otcCommunicationLinkDevice::dbgBreak()
{
    lock();
    m_serialContext.dbgbreak();
    unlock();
}

bool otcCommunicationLinkDevice::changeBaudRate(int nBaud)
{
    lock();
    bool ret = m_serialContext.baudrate(nBaud);
    unlock();
    return ret;
}

bool otcCommunicationLinkDevice::changeFlowMode(OTC_FLOW_T mode)
{
    lock();
    bool ret = m_serialContext.flowmode(mode);
    unlock();
    return ret;
}

void otcCommunicationLinkDevice::close()
{
    lock();
    m_serialContext.sclose();
    unlock();
}


bool otcCommunicationLinkDevice::serialOpen(char *szPort, int nBaud,OTC_FLOW_T mode,bool timeoutblock)
{
    lock();
    bool ret = m_serialContext.sopen(szPort,nBaud,mode,timeoutblock);
    unlock();
    return ret;
}

#define RBNOTXON        0xEE
#define RBXON           0x11
#define RBNOTXOFF       0xEC
#define RBXOFF          0x13
#define RBNOTBSLASH     0xA3


void unXonXoffizeBuffer(char* buffer,unsigned int datalen,unsigned int& datalenAfter,bool& lastIsBackSlash)
{
    lastIsBackSlash = false;
    datalenAfter = datalen;

    unsigned int copyOffset = 0;

    for(unsigned int i=0;i<datalen;i++)
    {
        if(buffer[i]=='\\')
        {
            if(i==datalen-1)
            {
                //Last one is a backslash. Keep this info, trash the backslash to restore it later
                lastIsBackSlash = true;
                break;
            }

            switch((unsigned char)buffer[i+1])
            {
                case RBNOTBSLASH:
                    buffer[i-copyOffset] = '\\';
                    copyOffset++; i++;
                break;
                case RBNOTXON:
                    buffer[i-copyOffset] = RBXON;
                    copyOffset++; i++;
                break;
                case RBNOTXOFF:
                    buffer[i-copyOffset] = RBXOFF;
                    copyOffset++; i++;
                break;
                default :
                    //MAJOR PROBLEM : not in xon xoff mode
                    //Copy the data as it is
                    buffer[i-copyOffset] = buffer[i];
                break;
            }
        }
        else
        {
            buffer[i-copyOffset] = buffer[i];
        }
    }

    datalenAfter = datalen - copyOffset - (lastIsBackSlash?1:0);
}

#define EP_IN   0x81
#define EP_OUT  0x01

int otcCommunicationLinkDevice::writeBlockXonXoff(unsigned char* buffer,unsigned int len)
{
    //It's ok to use a static. XonXoff is used only for serial and it's thread protected, only one
    //can access this function at a time
    static unsigned char tempBuffer[0x2000];

    unsigned int alreadyDone = 0;
    unsigned int toWrite = 0;
    unsigned int toParse = 0;
    unsigned int writeOffset = 0;

    int ret;

    while(alreadyDone<len)
    {
        toParse = (len-alreadyDone>0x1000)?0x1000:(len-alreadyDone);
        writeOffset = 0;

        for(unsigned int i=0;i<toParse;i++)
        {
            switch(buffer[alreadyDone+i])
            {
                case '\\':
                    tempBuffer[i+writeOffset]='\\';
                    tempBuffer[i+writeOffset+1]=RBNOTBSLASH;
                    writeOffset++; //added one char
                break;
                case RBXON:
                    tempBuffer[i+writeOffset]='\\';
                    tempBuffer[i+writeOffset+1]=RBNOTXON;
                    writeOffset++; //added one char
                break;
                case RBXOFF :
                    tempBuffer[i+writeOffset]='\\';
                    tempBuffer[i+writeOffset+1]=RBNOTXOFF;
                    writeOffset++; //added one char
                break;
                default :
                    tempBuffer[i+writeOffset]=buffer[alreadyDone+i];
                break;
            }
        }

        toWrite = toParse + writeOffset;

        ret = m_serialContext.swrite((char*)tempBuffer,toWrite);
        if(ret<0)
            return ret;

        if(ret<(int)toWrite) //problem
            return alreadyDone;

        alreadyDone += toParse;
    }

    return len;
}


int otcCommunicationLinkDevice::readBlock(unsigned char* buffer, unsigned int len)
{
    static bool     lastIsBackSlash = false;

    int ret = 0;

    if(len==0)
        return 0;

    lock();

    if(!isOpen())
        ret = 0;
    else
    {
		if(otcConfig::argFlowMode == OTC_FLOW_XONXOFF) //XON XOFF
		{
			if(lastIsBackSlash)
			{
				//Restitute last backslash
				((char*)buffer)[0] = '\\';
				len--;
				if(len<=0)
					goto endOfRead;

			}

			ret = m_serialContext.sread(buffer+(lastIsBackSlash?1:0),len);

			if(ret<0)
				goto endOfRead;

			ddebug(buffer+(lastIsBackSlash?1:0),ret);

			unsigned int datalenAfter;
			unXonXoffizeBuffer((char*)buffer,ret+(lastIsBackSlash?1:0),datalenAfter,lastIsBackSlash);
			ret = datalenAfter;
		}
		else
		{
			ret = m_serialContext.sread(buffer,len);
			ddebug(buffer,ret);
		}
    }

endOfRead :

    unlock();
    return ret;
}

int otcCommunicationLinkDevice::writeBlock(const char *buffer, unsigned int len)
{
    int ret = 0;
    lock();

    if(!isOpen())
        ret = 0;
    else
    {
        if(otcConfig::argFlowMode == OTC_FLOW_XONXOFF)
            ret = writeBlockXonXoff((unsigned char*)buffer,len);
        else
            ret = m_serialContext.swrite(buffer,len);
    }
    unlock();
    return ret;
}

