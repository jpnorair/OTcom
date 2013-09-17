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
/// @file           otc_socket.h
/// @brief          Definitions and classes for the socket link toolkit
///                 Functions for reading, buffering and parsing socket data
//
/// =========================================================================

#ifndef OTC_SOCKET_H
#define OTC_SOCKET_H

#include <qstring.h>
#include <qevent.h>
#include <qmutex.h>
#include <q3intdict.h>
#include <qglobal.h>
#include <q3socketdevice.h>
#include <q3serversocket.h>
#include <q3valuelist.h>

#include "otc_socket.h"

// OTCOM Socket Protocol is a simple 4 byte header. For data transit on the
// serial it is appended to the raw data. Its purpose is to allow controlling 
// OTCOM through thesocket, but also send/receive data on the serial though 
// the socket (needed to discriminate between internal and external commands)/
//
// +--------+--- ------+----------+----------------+
// |  Sync  |  Length  | Command  |      Data      |
// +--------+--- ------+----------+----------------+
// | 1 Byte | 2 Bytes  | 1 Byte   |     N Bytes    |
// +--------+--- ------+----------+----------------+
// |  0xBB  | (u16) N  |   char   |    (char[])    |
// +--------+--- ------+----------+----------------+
//
// A suggestion for the future would be to extend the OT-NDEF protocol to 
// replace the current one.

#define OTC_PROTOCOL_BAUDRATE_CHANGE_REQUEST             0xA0
#define OTC_PROTOCOL_RECONNECT_COM_PORT                  0xA2
#define OTC_PROTOCOL_FLOWMODE_CHANGE_REQUEST             0xA3
#define OTC_PROTOCOL_KILL_OTCOM                          0xB1
#define OTC_PROTOCOL_STATUS                              0xB3
#define OTC_PROTOCOL_SEND_AS_IS                          0xB5
#define OTC_PROTOCOL_RAW_DATA                            0xB6
#define OTC_PROTOCOL_SYNC                                0xBB
#define OTC_PROTOCOL_STATUS_RESULT                       0x02


class otcHostServer;
class otcHostClient;
class otcCommunicationLinkDevice;


class otcSocketParser
{
protected :

    QMutex          m_bufferMutex;
	unsigned int	m_size;
	unsigned char*	m_buffer;
	int             C_feed;
	int             C_untreated;

	int	            wrap(int i);
	unsigned char   at(int i);
	bool            isInValidRange(int i);
	int             uneatenBytesFrom(int i);

	enum           {HEADER_OK,HEADER_BAD, HEADER_NOT_READY} otc_cbuf_header_type;
	enum           {PACKET_OK,PACKET_BAD, PACKET_NOT_READY} otc_cbuf_packet_status;

	int             headerStatusAtPosition(int i);
	int             packetStatusAtPosition(int p);
	int             eatAsMuchAsPossibleFromSocket(otcHostClient& socket);
	int             treatChangeBaudrateCommandPacket(otcHostClient& client);
    int             treatChangeFlowModeCommandPacket(otcHostClient&);
    int             treatReconnectComPortPacket(otcHostClient& client);
    int             treatKillOtcomPacket(otcHostClient& client);
    int             treatSendAsIsPacket(otcHostClient& client,otcCommunicationLinkDevice& device);
    int             treatStatusPacket(otcHostClient& client);

public :

	otcSocketParser();
	~otcSocketParser();

	void             reinit();
    int              readDataFromClient(otcHostClient& clientin);
	void             dataTreatmentLoop(otcHostClient& clientin,otcCommunicationLinkDevice& device);	
    void             lock();
    void             unlock();
};


class otcHostClient : public Q3SocketDevice
{
	Q_OBJECT

public :
	otcHostClient(otcHostServer* server,int socket,int clientid);

	void              closeConnection();
	int               readData();
	void              treatData(otcCommunicationLinkDevice& device);
    bool              isUp() {return m_isUp;}
    int               getNetID() {return m_netID;}
    qint64            readBlock ( char * data, Q_ULONG maxlen );
    Q_LONG            writeBlock ( const char * data, Q_ULONG len );

protected :
	int m_netID;
	otcSocketParser	  m_parser;
    otcHostServer*    m_parentServer;
	bool              m_isUp;
	QMutex            m_rbMutex;

};


class otcHostClientLink
{
public :
    otcHostClientLink()
    {
        client = NULL;
        next = NULL;
    }

    otcHostClient*     client;
    otcHostClientLink* next;
};

typedef otcHostClientLink otcHostClientList;

#define OTC_EVENT_SOCKET_DYING 8436
class otcDyingSocketEvent : public QEvent
{
public :
    otcDyingSocketEvent(int clientID)
       : QEvent( (QEvent::Type) OTC_EVENT_SOCKET_DYING )
    {
        m_clientID = clientID;
    }

    int clientID()
    {
        return m_clientID;
    }

protected :
    int m_clientID;
};


class otcHostServer : public Q3ServerSocket
{
    Q_OBJECT

protected :
    QMutex            m_mutex;
    void              customEvent(QEvent* event);

public:
    otcHostServer(unsigned short port);
    ~otcHostServer();

    void              newConnection( int socket );
	int               readClients();
	void              treatClients(otcCommunicationLinkDevice& device);
    otcHostClientList* getClientListUnprotected() {return m_clients;}
    void              lock();
    void              unlock();

private:
	int                netIDGenerate();
	int                m_NetIDGen;
	otcHostClientList*  m_clients;

private slots :
	void destroyClient(int clientNetID);
};




#endif // OTC_SOCKET_H

