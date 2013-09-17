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
/// @file           otc_socket.cpp
/// @brief          Socket link toolkit
///                 Functions for reading, buffering and parsing socket data
//
/// =========================================================================

#include <qevent.h>
#include <qmutex.h>
#include <qstring.h>
#include "otc_socket.h"
#include "otc_main.h"
#include "otc_serial.h"
#include "otc_window.h"
#include "otc_mpipe.h"

// ---------------------------------------- //
//                                          //
//           SOCKET HOST CLIENT             //
//                                          //
// ---------------------------------------- //

otcHostClient::otcHostClient(otcHostServer* parentServer,int socket,int clientid)
:Q3SocketDevice()
{
    m_parentServer = parentServer;
	m_netID = clientid;

	setSocket(socket,Q3SocketDevice::Stream);
	setBlocking(false);

	m_isUp = true;

    setReceiveBufferSize(49152);
    setSendBufferSize(49152);
}

void otcHostClient::closeConnection()
{
 	close();
    m_isUp = false;

	otcDyingSocketEvent* e = new otcDyingSocketEvent(m_netID);
	QApplication::postEvent(m_parentServer,e);
}

void readData();

int otcHostClient::readData()
{
    if(!isUp())
        return 0;

	int data = m_parser.readDataFromClient(*this);
	if(data < 0)
	{   // Close on error
        closeConnection();
        return 0;
	}

	return data;
}

void otcHostClient::treatData(otcCommunicationLinkDevice& device)
{
    m_parser.dataTreatmentLoop(*this,device);
}

qint64 otcHostClient::readBlock ( char * data, Q_ULONG maxlen )
{
    qint64 res;
    m_rbMutex.lock();
    res = Q3SocketDevice::readBlock(data,maxlen);
    if ( (-1 == res) && (Q3SocketDevice::error() == Q3SocketDevice::NoError) && isValid() )
    {
        res = 0;
    }
    m_rbMutex.unlock();

    return res;
}

Q_LONG otcHostClient::writeBlock ( const char * data, Q_ULONG len )
{
    Q_LONG res;
    m_rbMutex.lock();
    res = Q3SocketDevice::writeBlock(data,len);
    if(res<=0 && len>0)
    {
        int err = error();
        otcConfig::logText("URGH! Write failed on client socket... maybe it's full or the client is going down.");
        otcConfig::logText(QString("Len was : %1,res is %2").arg(len).arg(res));
        otcConfig::logText(QString("Error code is : %1").arg(err));
    }
    m_rbMutex.unlock();

    return res;
}

// ---------------------------------------- //
//                                          //
//           SOCKET HOST SERVER             //
//                                          //
// ---------------------------------------- //


otcHostServer::otcHostServer(unsigned short port)
:Q3ServerSocket(port)
{
	// Client ID 0 will be used for OTCOM GUI itself
	m_NetIDGen=1;		
	m_clients = NULL;
}

otcHostServer::~otcHostServer()
{

}

int otcHostServer::netIDGenerate()
{
	// Client ID 0 will be used for OTCOM GUI itself
	return m_NetIDGen++; 
}

void otcHostServer::newConnection(int socket)
{
    lock();
	int newid = netIDGenerate();

	otcHostClient *s             = new otcHostClient(this,socket,newid);
    otcHostClientLink* newlink   = new otcHostClientLink();
    newlink->client             = s;

    // Insert new client
    if(!m_clients)
        m_clients = newlink;
    else
    {
        newlink->next   = m_clients;
        m_clients       = newlink;
    }

	otcConfig::logText(QString("Client with id %1 has connected.").arg(newid));
	unlock();
}

void otcHostServer::customEvent(QEvent* e)
{
    if(e->type() == OTC_EVENT_SOCKET_DYING)
    {
        destroyClient(((otcDyingSocketEvent*)e)->clientID());
    }
    else
        Q3ServerSocket::customEvent(e);
}


void otcHostServer::destroyClient(int clientId)
{
    lock();
	otcHostClientLink* c = m_clients;
	otcHostClientLink* prec = NULL;

    while(c)
    {
        if(c->client->getNetID() == clientId)
        {
            if(prec)
                prec->next = c->next;
            else
                m_clients = c->next;

            delete c->client;
            delete c;

            break;
        }

        prec = c;
        c = c->next;
    }

	otcConfig::logText(QString("Client with id %1 was disconnected.").arg(clientId));
    unlock();
}

int otcHostServer::readClients()
{
    int total = 0;

    lock();
	if(true)
	{
        otcHostClientLink* c = m_clients;
        while(c)
        {
            total += c->client->readData();
            c = c->next;
        }
	}
    unlock();

	return total;
}

void otcHostServer::treatClients(otcCommunicationLinkDevice& device)
{
    lock();
	if(true)
	{
        otcHostClientLink* c = m_clients;
        while(c)
        {
            c->client->treatData(device);
            c = c->next;
        }
	}
	unlock();
}

void otcHostServer::lock()
{
    m_mutex.lock();
};

void otcHostServer::unlock()
{
    m_mutex.unlock();
};


// ---------------------------------------- //
//                                          //
//           SOCKET READ ENGINE             //
//                                          //
// ---------------------------------------- //

void otcSocketParser::lock()
{
    m_bufferMutex.lock();
}

void otcSocketParser::unlock()
{
    m_bufferMutex.unlock();
}

otcSocketParser::otcSocketParser()
{
	m_size			= 4*0x10000; 
	m_buffer		= new unsigned char[m_size];
	C_feed = 0;
	C_untreated = 0;
}

void otcSocketParser::reinit()
{
    lock();
 	C_feed = 0;
	C_untreated = 0;
    unlock();
}

otcSocketParser::~otcSocketParser()
{
	delete m_buffer;
	m_buffer = NULL;
}

int	otcSocketParser::wrap(int i)
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

unsigned char otcSocketParser::at(int i)
{
	i = wrap(i);
	return m_buffer[i];
}

bool otcSocketParser::isInValidRange(int i)
{
	i = wrap(i);

	if(C_feed >= C_untreated)
		return (i>=C_untreated && i < C_feed);
	else
		return (i<C_feed || i>= C_untreated);
}

int otcSocketParser::uneatenBytesFrom(int i)
{
	i = wrap(i);

	if(!isInValidRange(i))
		return 0;

	if(C_feed >= C_untreated) //we assume that i is in valid range
		return C_feed - i;
	else if(i<C_feed)
		return C_feed - i;
	else
		return C_feed + m_size - i;
}

int otcSocketParser::headerStatusAtPosition(int i)
{
	int ueb = uneatenBytesFrom(i);

	if(ueb==0)
		return HEADER_NOT_READY;

	if(at(i)!= OTC_PROTOCOL_SYNC) //check this first, so we could advance
		return HEADER_BAD;

	if(ueb<4) //then, check this to know if we can check the rest
		return HEADER_NOT_READY;

	switch(at(i+3))
	{
		case OTC_PROTOCOL_BAUDRATE_CHANGE_REQUEST :
		case OTC_PROTOCOL_FLOWMODE_CHANGE_REQUEST:
		case OTC_PROTOCOL_STATUS :
		case OTC_PROTOCOL_KILL_OTCOM :
		case OTC_PROTOCOL_RECONNECT_COM_PORT :
		case OTC_PROTOCOL_SEND_AS_IS :
			return HEADER_OK;
		default :
			return HEADER_BAD;
	}

	return HEADER_BAD;
}

int otcSocketParser::packetStatusAtPosition(int p)
{
	int head = headerStatusAtPosition(p);

	switch(head)
	{
	case HEADER_BAD:
		return PACKET_BAD;

	case HEADER_NOT_READY :
		return PACKET_NOT_READY;

	default :
		break; //continue parsing
	}

	//The header seems ok, now calculate the packet len
	int packetlen = 256 * at(p+1) + at(p+2); //let the compilo optimize this

	int ueb = uneatenBytesFrom(p);
	if(ueb < packetlen + 4) //Real packet len is : packetlen(for data) + 4 for header
		return PACKET_NOT_READY;

	return PACKET_OK;
}

int otcSocketParser::eatAsMuchAsPossibleFromSocket(otcHostClient& socket)
{
	if(C_feed < C_untreated)
	{
		int bytesavail = C_untreated - C_feed - 1; //keep 1 empty cell between both

		if(bytesavail<0) bytesavail = 0;

		int read = socket.readBlock((char*)(m_buffer+C_feed),bytesavail);
		if(read<0)
            return -1; //error

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

		read1 = socket.readBlock((char*)(m_buffer+C_feed),avail1);
		if(read1<0)
            return -1; //error

		C_feed +=read1;
		C_feed = wrap(C_feed);

		if(avail1==read1) //need to continue reading
		{
			read2 = socket.readBlock((char*)(m_buffer+C_feed),avail2);
			if(read2<0)
                return -1; //error

            C_feed+=read2;
			C_feed =  wrap(C_feed);
		}

		return read1+read2;
	}
}

int otcSocketParser::treatChangeBaudrateCommandPacket(otcHostClient&)
{
	int	realpacketlen = 8;

	int baudrateReq = at(C_untreated+4)
		|(at(C_untreated+5)<<8)
		|(at(C_untreated+6)<<16)
		|(at(C_untreated+7)<<24);

	if(baudrateReq!=otcConfig::argBaudRate)
	{
        QApplication::postEvent(otcConfig::mainWindow,new otcBaudrateChangeEvent(baudrateReq));
	}

	return realpacketlen;
}

int otcSocketParser::treatChangeFlowModeCommandPacket(otcHostClient&)
{
    int	realpacketlen = 8;

    int mode = at(C_untreated+4)
        |(at(C_untreated+5)<<8)
        |(at(C_untreated+6)<<16)
        |(at(C_untreated+7)<<24);

    if(mode!=otcConfig::argFlowMode)
    {
        QApplication::postEvent(otcConfig::mainWindow,new otcFlowModeChangeEvent((OTC_FLOW_T)mode));
    }

    return realpacketlen;
}

int otcSocketParser::treatReconnectComPortPacket(otcHostClient&)
{
    int realpacketlen = 4;
    QApplication::postEvent(otcConfig::mainWindow,new otcReconnectComPortEvent());
	return realpacketlen;
}

int otcSocketParser::treatKillOtcomPacket(otcHostClient&)
{
    int realpacketlen = 4;
    QApplication::postEvent(otcConfig::mainWindow,new otcKillEvent());
	return realpacketlen;
}

int otcSocketParser::treatStatusPacket(otcHostClient& client)
{
    int realpacketlen = 4;
    bool connected = otcConfig::mainWindow->isDeviceConnected();

    unsigned char statusPacket[5];
    statusPacket[0] = OTC_PROTOCOL_SYNC;
    statusPacket[1] = 0x00;
    statusPacket[2] = 0x01;
    statusPacket[3] = OTC_PROTOCOL_STATUS_RESULT;
    statusPacket[4] = connected?1:0;

    client.writeBlock((char*)statusPacket,5);

    return realpacketlen;
}

int otcSocketParser::treatSendAsIsPacket(otcHostClient& client,otcCommunicationLinkDevice& device)
{
	static unsigned char m_packet[0x10000];
	static otc_mpipe_parser msg;

    int packetlen = 256 * at(C_untreated+1) + at(C_untreated+2);

    for(int i=0;i<packetlen;i++)
        m_packet[i] = at(C_untreated+4+i);

    device.writeBlock((char*)m_packet,packetlen);

	if (OTC_PRINT_MODE_RAW == otcConfig::argPrintMode)
	{
        otcConfig::logText(QString("(BPS) CID : %1, SIZE: %2").arg(client.getNetID()).arg(packetlen));
		QString msg="<font color=blue>";
        for(int cc=0;cc<packetlen;cc++)
        {
            if(cc%16==0 && cc!=0) msg+="\n";
            msg+=QString("%1 ").arg(QString("%1").arg(m_packet[cc],2,16).upper().replace(' ','0'));
        }
        msg+="</font>";
        otcConfig::logText(msg);
	}
	else // if (OTC_PRINT_MODE_NDEF == otcConfig::argPrintMode)
	{
		msg.parse(m_packet,packetlen);
	}

    return packetlen+4;
}

int otcSocketParser::readDataFromClient(otcHostClient& inputClient)
{
    lock();
    int ret = eatAsMuchAsPossibleFromSocket(inputClient);
    unlock();

    return ret;
}

void otcSocketParser::dataTreatmentLoop(otcHostClient& client,otcCommunicationLinkDevice& device)
{
    lock();

	while(uneatenBytesFrom(C_untreated))
	{
		bool gottabreak = false;

		switch(packetStatusAtPosition(C_untreated))
		{
			case PACKET_OK :
			{
				int plen=0;
				switch(at(C_untreated+3))
				{
					case OTC_PROTOCOL_BAUDRATE_CHANGE_REQUEST :
						plen = treatChangeBaudrateCommandPacket(client);
						break;
					case OTC_PROTOCOL_FLOWMODE_CHANGE_REQUEST:
						plen = treatChangeFlowModeCommandPacket(client);
						break;
					case OTC_PROTOCOL_STATUS :
						plen = treatStatusPacket(client);
						break;
					case OTC_PROTOCOL_KILL_OTCOM :
						plen = treatKillOtcomPacket(client);
						break;
					case OTC_PROTOCOL_RECONNECT_COM_PORT :
						plen = treatReconnectComPortPacket(client);
						break;
					case OTC_PROTOCOL_SEND_AS_IS :
						plen = treatSendAsIsPacket(client,device);
						break;
					default:
						plen = 1;
						break;
				}
				C_untreated = wrap(C_untreated+plen);
				break;
			}
			case PACKET_NOT_READY :
				gottabreak = true;
				break;
			case PACKET_BAD :
			{
   			    C_untreated = wrap(C_untreated+1);
				otcConfig::logText(QString("Client with id %1 sent bad data!").arg(client.getNetID()));
				break;
			}
		}

		if(gottabreak)
			break;
	}

    unlock();
}




