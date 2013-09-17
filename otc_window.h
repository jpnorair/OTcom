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
/// @file           otc_window.h
/// @brief          Definitions ans classes for the OTCOM main window GUI
//
/// =========================================================================

#ifndef OTC_MAIN_WINDOW_H
#define OTC_MAIN_WINDOW_H

#include <qmainwindow.h>
#include <qtimer.h>
#include <q3textedit.h>
#include <qtabwidget.h>
#include <q3textview.h>
#include <qthread.h>
#include <qapplication.h>
#include <qlineedit.h>
#include <q3socket.h>
#include <qsystemtrayicon.h>
#include <QWaitCondition>

#include "otc_command.h"
#include "otc_socket.h"
#include "otc_serial.h"


class otcMainWindow;
class QComboBox;
class QMenu;
class Q3HBox;
class QPushButton;


typedef enum
{
	OTC_EVENT_LOG = 3247,
	OTC_EVENT_DEVICE_READ,
	OTC_EVENT_DEVICE_TREAT,
	OTC_EVENT_CLIENT_READ,
	OTC_EVENT_CLIENT_TREAT,
	OTC_EVENT_CHANGE_BAUDRATE = 65432,
    OTC_EVENT_RECONNECT_COMPORT,
    OTC_EVENT_KILL,
    OTC_EVENT_CLEAN,
    OTC_EVENT_CHANGE_FLOW,
    OTC_EVENT_FLUSH_FIFOS,

} OTC_EVENT_T;


class otcLogMessageEvent : public QEvent
{
   public:
	otcLogMessageEvent( const QString& s )
		: QEvent( (QEvent::Type) OTC_EVENT_LOG ), message(s)
		{}

	const QString& getMessage() const { return message; }
private:
	QString message;
};


class otcDeviceReadEvent : public QEvent
{
public:
	otcDeviceReadEvent()
		: QEvent( (QEvent::Type) OTC_EVENT_DEVICE_READ )
    {}
};


class otcDeviceTreatEvent : public QEvent
{
public :
    otcDeviceTreatEvent()
        : QEvent( (QEvent::Type) OTC_EVENT_DEVICE_TREAT )
    {}
};


class otcClientReadEvent : public QEvent
{
public :
    otcClientReadEvent()
        : QEvent( (QEvent::Type) OTC_EVENT_CLIENT_READ )
    {}
};


class otcClientTreatEvent : public QEvent
{
  public :
    otcClientTreatEvent()
     : QEvent( ((QEvent::Type) OTC_EVENT_CLIENT_TREAT ))
     {
     }
};


class otcBaudrateChangeEvent : public QEvent
{
public:
	otcBaudrateChangeEvent(int baudrate)
		: QEvent( (QEvent::Type) OTC_EVENT_CHANGE_BAUDRATE )
	{
		m_baudrate = baudrate;
	}

    int baudrate() {return m_baudrate;}

private:
	int m_baudrate;
};


class otcFlowModeChangeEvent : public QEvent
{
public:
    otcFlowModeChangeEvent(OTC_FLOW_T mode)
        : QEvent( (QEvent::Type) OTC_EVENT_CHANGE_FLOW )
    {
        m_mode = mode;
    }

    OTC_FLOW_T mode() {return m_mode;}

private:
    OTC_FLOW_T m_mode;
};


class otcReconnectComPortEvent : public QEvent
{
 public:
	otcReconnectComPortEvent()
		: QEvent( (QEvent::Type) OTC_EVENT_RECONNECT_COMPORT )
	{
	}
};


class otcKillEvent : public QEvent
{
 public:
	otcKillEvent()
		: QEvent( (QEvent::Type) OTC_EVENT_KILL )
	{
	}
};


class otcCleanEvent: public QEvent
{
 public:
    otcCleanEvent()
        : QEvent( (QEvent::Type) OTC_EVENT_CLEAN )
    {
    }
};


class otcFlushFifosEvent: public QEvent
{
 public:
    otcFlushFifosEvent()
        : QEvent( (QEvent::Type) OTC_EVENT_FLUSH_FIFOS )
    {
    }
};

#define OTC_BLINK_PERIOD 5
#define OTC_BLINK_TIMEOUT_FACTOR 5

class otcBlinker : public QWidget
{
public :
    otcBlinker(QWidget* parent);

    bool read()
    {
		m_treat_count %= OTC_BLINK_PERIOD;

		if (m_read_count > OTC_BLINK_TIMEOUT_FACTOR * OTC_BLINK_PERIOD)
		{
			m_currentImage = m_imageError;
			m_read_count = OTC_BLINK_PERIOD;
			return FALSE;
		}
		else if (m_read_count == 0) 
		{
			m_currentImage = m_imageRead;
		}

		m_read_count++;
		return TRUE;
    }

    bool treat()
    {
		m_read_count  %= OTC_BLINK_PERIOD;

		if (m_treat_count > OTC_BLINK_TIMEOUT_FACTOR * OTC_BLINK_PERIOD)
		{
			m_currentImage = m_imageError;
			m_treat_count = OTC_BLINK_PERIOD;
			return FALSE;
		}
		else if (m_treat_count == OTC_BLINK_PERIOD / 2)
		{
			m_currentImage = m_imageTreat;
		}

		m_treat_count++;
		return TRUE;
    }

protected :
    void paintEvent(QPaintEvent* pe);

 	QPixmap         m_redrawPixmap;
	QPainter*       m_redrawPainter;
	QImage*         m_imageRead;
	QImage*         m_imageTreat;
	QImage*         m_imageError;
	QImage*         m_currentImage;
	unsigned char   m_read_count;
	unsigned char   m_treat_count;

};


class otcLogWidget : public Q3TextEdit
{
public :
    otcLogWidget(QWidget* parent)
        :Q3TextEdit(parent) {};

    void customEvent(QEvent* e);
};


class otcDeviceReaderThread : public QThread
{
public : //Methods

	otcDeviceReaderThread(otcMainWindow* mw);
	~otcDeviceReaderThread();

	void run();
	void stopRunning();

protected : //Attributes
    otcMainWindow*     m_mainWindow;
	bool    	       m_running;
};


class otcClientReaderThread : public QThread
{
public : //Methods

	otcClientReaderThread(otcMainWindow* mw);
	~otcClientReaderThread();

	void run();
	void stopRunning();

protected : //Attributes
    otcMainWindow*     m_mainWindow;
	bool    	       m_running;
};


class otcDeviceDataTreatmentThread : public QThread
{
 public : //Methods

	otcDeviceDataTreatmentThread(otcMainWindow* mw);
	~otcDeviceDataTreatmentThread();

	void run();
	void stopRunning();

protected : //Attributes
    otcMainWindow*     m_mainWindow;
	bool    	       m_running;
};


class otcClientsDataTreatmentThread : public QThread
{
 public : //Methods

	otcClientsDataTreatmentThread(otcMainWindow* mw);
	~otcClientsDataTreatmentThread();

	void run();
	void stopRunning();

protected : //Attributes
    otcMainWindow*     m_mainWindow;
	bool    	       m_running;
};


class otcMainWindow : public QMainWindow
{
	Q_OBJECT

	friend class otcDeviceReaderThread;
	friend class otcClientReaderThread;
	friend class otcDeviceDataTreatmentThread;
	friend class otcClientsDataTreatmentThread;

protected :
    void readDataStep();
    void readClientsDataStep();
    void treatDeviceDataStep();
    void treatClientsDataStep();

public :
	otcMainWindow(QWidget* parent = NULL);
	~otcMainWindow();

    bool isDeviceConnected()	{return m_device.isOpen();}
    bool isHostServerOk()		{return (m_hostServer!=NULL && m_hostServer->ok());}

	void changeBaudrateAndUpdate(int baudrate);
    void changeFlowModeAndUpdate(OTC_FLOW_T mode);
    void printStatus();

protected :
    otcBlinker*                   m_deviceblinker;
    otcBlinker*                   m_clientblinker;
	otcLogWidget*	              m_logWidget;
	QComboBox*	                  m_comPortComboBox;
	QComboBox*	                  m_baudRateComboBox;
	QComboBox*	                  m_printModeComboBox;
    QComboBox*                    m_flowModeComboBox;
    QTimer*                       m_reconnectionTimer;
    QPushButton*                  m_startRecordingButton;
	otc_command_parser*           m_commandParser;
	otcHostServer*	              m_hostServer;
	otcCommunicationLinkDevice	  m_device;
	otcDataParser			      m_parser;
	otcDeviceReaderThread*        m_readerThread;
	otcClientReaderThread*        m_clientReaderThread;
    otcDeviceDataTreatmentThread* m_dataTreatmentThread;
    otcClientsDataTreatmentThread* m_clientsDataTreatmentThread;

	bool                          connectToDevice();
	void                          closeDevice();
	void                          customEvent( QEvent * e );
    void                          hideEvent(QHideEvent * e);
	void                          closeEvent(QCloseEvent*e);
	bool                          secureQuit();
	void                          changeBaudRate (int newbdr);

protected slots:
	void                          lastCleanUp();
	bool                          connectComPort(const QString& s);
	bool                          reconnectDevice();
	void                          changeBaudRate(const QString& s);
    void                          changeFlowMode(const QString& s);
    QString                       flowModeString(OTC_FLOW_T mode);
    OTC_FLOW_T                    flowModeFromString(const QString& s);
    void                          changePrintMode(const QString& s);
	QString                       printModeString(OTC_PRINT_MODE_T mode);
    OTC_PRINT_MODE_T              printModeFromString(const QString& s);
	void                          clearScreen();
    void                          flushFifos();
    void                          startStopLogging();
	void                          slotQuit();
	void                          slotShowOrMinimize();
	void                          reconnectionTimer();
};



#endif // OTC_MAIN_WINDOW_H
