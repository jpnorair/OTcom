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
/// @file           otc_window.cpp
/// @brief          OTCOM main window GUI, action and event functions
//
/// =========================================================================

#include "otc_window.h"
#include "otc_main.h"

#include <qmessagebox.h>
#include <qapplication.h>
#include <q3popupmenu.h>
#include <qsplitter.h>
#include <qmenubar.h>
#include <qfiledialog.h>
#include <qinputdialog.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <q3vbox.h>
#include <qcheckbox.h>
#include <qshortcut.h>
#include <qpainter.h>
#include <qstatusbar.h>


#ifndef WIN32
#include <unistd.h>
int GetTickCount(void);
#define Sleep(n) usleep((n)*1000)
#endif

// ---------------------------------------- //
//                                          //
//           MAIN WINDOW                    //
//                                          //
// ---------------------------------------- //


otcMainWindow::otcMainWindow(QWidget* parent)
:QMainWindow(parent)
{
	m_hostServer     = NULL;
	m_deviceblinker  = new otcBlinker(this);
	m_clientblinker  = new otcBlinker(this);
	Q3VBox* vbox     = new Q3VBox(this);
	Q3HBox* combox   = new Q3HBox(vbox);
	m_logWidget      = new otcLogWidget(vbox);
    Q3HBox* printbox = new Q3HBox(vbox);
    Q3HBox* cmdbox   = new Q3HBox(vbox);
	combox->setSpacing(4);

    otcConfig::logWidget = m_logWidget;

    // Reconnect menu
    if(OTC_LINK_COM == otcConfig::communicationLink)
	{
		// com port selection  menu
		m_comPortComboBox = new QComboBox(combox);
		m_comPortComboBox->setEditable(FALSE);
		for (int i = 0; i < OTC_COM_PORTS_MAX_NB; ++i)
		{
			m_comPortComboBox->insertItem(QString("Connect COM%1").arg(i));
		}
	}
	m_comPortComboBox->setCurrentText(QString("Connect COM%1").arg(otcConfig::argComPort));

	// Baud rate menu
	QLabel*	labelBaudRate = new QLabel("Baudrate",combox);
	labelBaudRate->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
	m_baudRateComboBox = new QComboBox(combox);
	m_baudRateComboBox->setEditable(FALSE);
	m_baudRateComboBox->insertItem("9600");
	m_baudRateComboBox->insertItem("57600");
	m_baudRateComboBox->insertItem("115200");
	m_baudRateComboBox->insertItem("460800");
	m_baudRateComboBox->insertItem("921600");
	m_baudRateComboBox->setCurrentText(QString("%1").arg(otcConfig::argBaudRate));

	// Flow mode menu
	QLabel* labelFlow = new QLabel("Flow Ctrl",combox);
	labelFlow->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_flowModeComboBox = new QComboBox(combox);
    m_flowModeComboBox->setEditable(FALSE);
    m_flowModeComboBox->insertItem(flowModeString(OTC_FLOW_NONE));
    m_flowModeComboBox->insertItem(flowModeString(OTC_FLOW_HARDWARE));
    m_flowModeComboBox->insertItem(flowModeString(OTC_FLOW_XONXOFF));
    m_flowModeComboBox->setCurrentText(flowModeString(otcConfig::argFlowMode));

	// Command Prompt
    m_commandParser = new otc_command_parser(cmdbox,&m_device);

	// Print mode menu
    m_printModeComboBox = new QComboBox(printbox);
	m_printModeComboBox->setEditable(FALSE);
	m_printModeComboBox->insertItem("Print None");
	m_printModeComboBox->insertItem("Print Raw");
	m_printModeComboBox->insertItem("Print NDEF/OT");
	// m_printModeComboBox->insertItem("Print NDEF (DBG)");
	m_printModeComboBox->setCurrentText(QString("%1").arg(printModeString(otcConfig::argPrintMode)));

	// Clear screen button
	QPushButton* clsButton = new QPushButton("Clear Screen",printbox);
	clsButton->setSizePolicy(QSizePolicy(QSizePolicy::Minimum,QSizePolicy::Maximum));

	// Logging on/off
    m_startRecordingButton = new QPushButton("Start Logging...",printbox);

	// Connect buttons to actions
	connect(clsButton,              SIGNAL(pressed()),                 this, SLOT(clearScreen()));
	connect(m_comPortComboBox,      SIGNAL(activated(const QString&)), this, SLOT(connectComPort(const QString&)));
    connect(m_printModeComboBox,    SIGNAL(activated(const QString&)), this, SLOT(changePrintMode(const QString&)));
	connect(m_baudRateComboBox,     SIGNAL(activated(const QString&)), this, SLOT(changeBaudRate(const QString&)));
    connect(m_flowModeComboBox,     SIGNAL(activated(const QString&)), this, SLOT(changeFlowMode(const QString&)));
    connect(m_startRecordingButton, SIGNAL(clicked()),                 this, SLOT(startStopLogging()));

	// Set parameters for the logbox
	setCentralWidget(vbox);
	m_logWidget->setFocusPolicy(Qt::NoFocus);
	m_logWidget->setTextFormat( Qt::LogText );
	m_logWidget->setFont(QFont("Courier New"));
	m_logWidget->scrollToBottom();
	
	resize(500,400);
    otcConfig::mainWindow = this;

	// Blinkers
	statusBar()->addPermanentWidget(new QLabel("Host "));
	statusBar()->addPermanentWidget(m_deviceblinker);
	statusBar()->addPermanentWidget(new QLabel("Client "));
	statusBar()->addPermanentWidget(m_clientblinker);

	connect(qApp,SIGNAL(aboutToQuit()),this,SLOT(lastCleanUp()));

    m_reconnectionTimer = new QTimer();

    connect(m_reconnectionTimer,SIGNAL(timeout()),this,SLOT(reconnectionTimer()));

	// open COM
    m_hostServer = new otcHostServer(OTC_COM_START_PORT + otcConfig::argComPort);

	// start read-treat threads
	m_readerThread = new otcDeviceReaderThread(this);
	m_readerThread->start();
	m_dataTreatmentThread = new otcDeviceDataTreatmentThread(this);
	m_dataTreatmentThread->start();
	m_clientReaderThread = new otcClientReaderThread(this);
	m_clientReaderThread->start();
	m_clientsDataTreatmentThread = new otcClientsDataTreatmentThread(this);
	m_clientsDataTreatmentThread->start();

	reconnectDevice();
}


// Destructor
otcMainWindow::~otcMainWindow()
{
    m_dataTreatmentThread->stopRunning();
    m_dataTreatmentThread->wait(1000);
    delete m_dataTreatmentThread;

	m_readerThread->stopRunning();
	m_readerThread->wait(1000);
	delete m_readerThread;

	m_clientReaderThread->stopRunning();
	m_clientReaderThread->wait(1000);
	delete m_clientReaderThread;

	m_clientsDataTreatmentThread->stopRunning();
	m_clientsDataTreatmentThread->wait(1000);
	delete m_clientsDataTreatmentThread;

}

// ---------------------------------------- //
//                                          //
//           ACTIVITTY STATUS               //
//                                          //
// ---------------------------------------- //

#include "icons/read.xpm"
#include "icons/treat.xpm"
#include "icons/error.xpm"

otcBlinker::otcBlinker(QWidget* parent)
    :QWidget(parent,"",Qt::WNoAutoErase)
{
	QPixmap* readIcon  = new QPixmap(readxpm);
	QPixmap* treatIcon = new QPixmap(treatxpm);
	QPixmap* errorIcon = new QPixmap(errorxpm);

    #define OTC_BLINK_ICON_SIZE 17

    setBackgroundMode( Qt::NoBackground );
    m_redrawPixmap.resize(OTC_BLINK_ICON_SIZE,OTC_BLINK_ICON_SIZE);
    m_redrawPainter = new QPainter(&m_redrawPixmap);
    m_imageRead = new QImage(readIcon->toImage());
    m_imageTreat = new QImage(treatIcon->toImage());
    m_imageError = new QImage(errorIcon->toImage());
    m_currentImage = m_imageRead;
	m_read_count = 0;
	m_treat_count = 0;

    setFixedSize(OTC_BLINK_ICON_SIZE,OTC_BLINK_ICON_SIZE);
}


void otcBlinker::paintEvent(QPaintEvent*)
{
    m_redrawPixmap.fill(backgroundColor());
    m_redrawPainter->drawImage(2,0,*m_currentImage);
	bitBlt(this,0,0,&m_redrawPixmap);
}

// ---------------------------------------- //
//                                          //
//           READ-TREAT THREADS             //
//                                          //
// ---------------------------------------- //

// -----------
// Device Read
// -----------

otcDeviceReaderThread::otcDeviceReaderThread(otcMainWindow* mw)
{
    m_mainWindow = mw;
    m_running = FALSE;
}

otcDeviceReaderThread::~otcDeviceReaderThread() {};

void otcDeviceReaderThread::stopRunning()
{
	m_running = FALSE;
}

void otcDeviceReaderThread::run()
{
	m_running = TRUE;

	int curTime = GetTickCount();
    int lastTime = curTime;

	while(m_running)
	{
	    curTime = GetTickCount();

	    if(curTime - lastTime > 100)
        {
            lastTime = curTime;
            QApplication::postEvent(m_mainWindow, new otcDeviceReadEvent());
        }

	    m_mainWindow->readDataStep();
	}
}

void otcMainWindow::readDataStep()
{
    Sleep(1);
}

// -----------
// Device Treat
// -----------

otcDeviceDataTreatmentThread::otcDeviceDataTreatmentThread(otcMainWindow* mw)
{
    m_mainWindow = mw;
    m_running = FALSE;
}

otcDeviceDataTreatmentThread::~otcDeviceDataTreatmentThread() {};

void otcDeviceDataTreatmentThread::stopRunning()
{
	m_running = FALSE;
}

void otcDeviceDataTreatmentThread::run()
{
	m_running = TRUE;

	int curTime = GetTickCount();
    int lastTime = curTime;

	while(m_running)
	{
	    curTime = GetTickCount();

	    if(curTime - lastTime > 100)
        {
            lastTime = curTime;
            QApplication::postEvent(m_mainWindow, new otcDeviceTreatEvent());
        }

	    m_mainWindow->treatDeviceDataStep();
	}
}

void otcMainWindow::treatDeviceDataStep()
{
    if(!m_hostServer)
    {
        Sleep(1);
        return;
    }

    if (m_parser.readDataFromDevice(m_device) <= 0 )
    {
	    // sleep to calm down OTCOM
        Sleep(1);
	}

    m_parser.dataTreatmentLoop(*m_hostServer);
}

// -----------
// Client Read
// -----------

otcClientReaderThread::otcClientReaderThread(otcMainWindow* mw)
{
    m_mainWindow = mw;
    m_running = FALSE;
}

otcClientReaderThread::~otcClientReaderThread() {};

void otcClientReaderThread::stopRunning()
{
	m_running = FALSE;
}

void otcClientReaderThread::run()
{
	m_running = TRUE;

	int curTime = GetTickCount();
    int lastTime = curTime;

	while(m_running)
	{
	    curTime = GetTickCount();

	    if(curTime - lastTime > 100)
        {
            lastTime = curTime;
            QApplication::postEvent(m_mainWindow, new otcClientReadEvent());
        }

	    m_mainWindow->readClientsDataStep();
	}
}

void otcMainWindow::readClientsDataStep()
{
    // Read data from clients
    if(!m_hostServer)
    {
        Sleep(1);
        return;
    }

    if (m_hostServer->readClients() == 0)
	{
        Sleep(1);
	}
}

// -----------
// Client Treat
// -----------

otcClientsDataTreatmentThread::otcClientsDataTreatmentThread(otcMainWindow* mw)
{
    m_mainWindow = mw;
    m_running = FALSE;
}

otcClientsDataTreatmentThread::~otcClientsDataTreatmentThread() {};

void otcClientsDataTreatmentThread::stopRunning()
{
	m_running = FALSE;
}

void otcClientsDataTreatmentThread::run()
{
	m_running = TRUE;

	int curTime = GetTickCount();
    int lastTime = curTime;

	while(m_running)
	{
	    curTime = GetTickCount();

	    if(curTime - lastTime > 100)
        {
            lastTime = curTime;
            QApplication::postEvent(m_mainWindow, new otcClientTreatEvent());
        }

	    m_mainWindow->treatClientsDataStep();
	}
}

void otcMainWindow::treatClientsDataStep()
{
    if(!m_hostServer)
    {
        Sleep(1);
        return;
    }

    m_hostServer->treatClients(m_device);
    Sleep(1);
}

// ---------------------------------------- //
//                                          //
//           ACTIONS                        //
//                                          //
// ---------------------------------------- //

// -----------
// Clear Screen
// -----------

void otcMainWindow::clearScreen()
{
	m_logWidget->clear();
}

// -----------
// Connect
// -----------

bool otcMainWindow::connectComPort(const QString& s)
{
    QRegExp ex("Connect COM(\\d+)",false);
    if(ex.search(QString(s)) != -1)
	{
		int comPort = ex.cap(1).toInt();
		if (otcConfig::argComPort != comPort)
		{
			otcConfig::argComPort = comPort;
		}
		else
		{
			m_logWidget->append(QString("Reconnecting COM%1").arg(comPort));
		}
	}

	return reconnectDevice();
}

bool otcMainWindow::reconnectDevice()
{
	closeDevice();
	bool success = connectToDevice();

    m_reconnectionTimer->start(1000);
	return success;
}

void otcMainWindow::reconnectionTimer()
{
    if(!m_hostServer)
        return;
}

bool otcMainWindow::connectToDevice()
{
    if(OTC_LINK_COM == otcConfig::communicationLink)
    {
        char pname[128];

        // Build the string of the serial port file to use, depending on the OS.
    #ifdef WIN32
        if(otcConfig::argComPort<10)
            sprintf(pname,"COM%d",otcConfig::argComPort);
        else
            sprintf(pname,"\\\\.\\COM%d",otcConfig::argComPort);
    #else
        sprintf(pname,OTC_COM_PORTS_MAP_PATH"/com%d",otcConfig::argComPort);
    #endif

        m_device.close();
        m_parser.reinit();

        if (!m_device.serialOpen(pname,otcConfig::argBaudRate,otcConfig::argFlowMode,TRUE))
        {
            m_logWidget->append( QString("Could not connect to %1 !").arg(pname));
            return FALSE;
        }

        m_logWidget->append(QString("Connected to com%1 !").arg(otcConfig::argComPort));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void otcMainWindow::closeDevice()
{
	m_device.close();
}

// -----------
// Print Mode
// -----------

void otcMainWindow::changePrintMode(const QString& s)
{
    otcConfig::argPrintMode = printModeFromString(s);
	m_logWidget->append("Mode changed to "+s);
	if (OTC_PRINT_MODE_NDEF == otcConfig::argPrintMode)
	{
		m_logWidget->append("Format [ Type ] [ Id ] [ Payload ]");
	}
	else if (OTC_PRINT_MODE_NDEF_PLUS_OT == otcConfig::argPrintMode)
	{
		m_logWidget->append("Format [ Seq nb ] [ Id ] [ Payload ]");
	}
}

QString otcMainWindow::printModeString(OTC_PRINT_MODE_T mode)
{
    switch(mode)
    {
        case OTC_PRINT_MODE_HIDE:
            return "Print None";
        case OTC_PRINT_MODE_RAW:
            return "Print Raw";
        case OTC_PRINT_MODE_NDEF:
            return "Print NDEF (DBG)";
        case OTC_PRINT_MODE_NDEF_PLUS_OT:
            return "Print NDEF/OT";
		default:
            return "Mode does not exist!";
    }
}

OTC_PRINT_MODE_T otcMainWindow::printModeFromString(const QString& mode)
{
    if(mode=="Print None")
        return OTC_PRINT_MODE_HIDE;
    else if(mode=="Print Raw")
        return OTC_PRINT_MODE_RAW;
    else if(mode=="Print NDEF (DBG)")
        return OTC_PRINT_MODE_NDEF;
    else if(mode=="Print NDEF/OT")
        return OTC_PRINT_MODE_NDEF_PLUS_OT;
    else
        return OTC_PRINT_MODE_INVALID;
}

// -----------
// Baudrate
// -----------

void otcMainWindow::changeBaudRate(const QString& s)
{
	int baudrate = s.toInt();
	changeBaudrateAndUpdate(baudrate);
}

void otcMainWindow::changeBaudrateAndUpdate(int baudrate)
{
    changeBaudRate(baudrate);
    m_baudRateComboBox->blockSignals(TRUE);
    m_baudRateComboBox->setCurrentText(QString("%1").arg(baudrate));
    m_baudRateComboBox->blockSignals(FALSE);
}

void otcMainWindow::changeBaudRate(int newbdr)
{
	otcConfig::argBaudRate = newbdr;

	if(m_device.changeBaudRate(newbdr))
		m_logWidget->append("Baudrate changed to : "+QString("%1").arg(newbdr) );
	else
		m_logWidget->append("Could not change baudrate!");
}

// -----------
// Flow Control
// -----------

void otcMainWindow::changeFlowModeAndUpdate(OTC_FLOW_T mode)
{
    switch(mode)
    {
        case OTC_FLOW_HARDWARE:
        case OTC_FLOW_NONE:
        case OTC_FLOW_XONXOFF:

            if(m_device.changeFlowMode(mode))
            {
                m_logWidget->append("Flow mode changed to : "+flowModeString(mode));
                otcConfig::argFlowMode = mode;
                m_flowModeComboBox->blockSignals(TRUE);
                m_flowModeComboBox->setCurrentText(flowModeString(mode));
                m_flowModeComboBox->blockSignals(FALSE);
            }
            else
                m_logWidget->append("Could not change flow mode!");
            break;
        default:
            m_logWidget->append("Asked to change to a wrong flow mode!");
            break;
    }
}

void otcMainWindow::changeFlowMode(const QString& s)
{
    changeFlowModeAndUpdate(flowModeFromString(s));
}

QString otcMainWindow::flowModeString(OTC_FLOW_T mode)
{
    switch(mode)
    {
        case OTC_FLOW_XONXOFF:
            return "Xon / Xoff";
        case OTC_FLOW_HARDWARE:
            return "Hardware";
        case OTC_FLOW_NONE:
            return "None";
        default:
            return "Mode does not exist!";
    }
}

OTC_FLOW_T otcMainWindow::flowModeFromString(const QString& mode)
{
    if(mode=="Xon / Xoff")
        return OTC_FLOW_XONXOFF;
    else if(mode=="Hardware")
        return OTC_FLOW_HARDWARE;
    else if(mode=="None")
        return OTC_FLOW_NONE;
    else
        return OTC_FLOW_UNVALID;
}


// -----------
// Log
// -----------

void otcMainWindow::startStopLogging()
{
    if(dIsOpen())
    {
        dcl();
        m_startRecordingButton->setText("Start Logging...");
    }
    else
    {
        dop();
        m_startRecordingButton->setText("Stop Logging...");
    }
}


void otcConfig::logText(const QString& str)
{
    otcLogMessageEvent* e = new otcLogMessageEvent(str);
    QApplication::postEvent(logWidget,e);
}


void otcMainWindow::printStatus()
{
    otcConfig::logText(m_parser.getStatus());
}

void otcMainWindow::flushFifos()
{
    m_device.flush();
    m_parser.reinit();
}


// -----------
// Quit
// -----------

bool otcMainWindow::secureQuit()
{
    if(m_hostServer->getClientListUnprotected())
	{
        if(QMessageBox::question(
            this,
			tr("Warning !"),
            tr("Some clients are still connected to this OTCOM !\nDo you still want to close it ?"),
            tr("&Yes"), tr("&No"),
            QString::null, 0, 1 ) )
		{
			return FALSE;
		}

	}
	qApp->quit();
	return TRUE;
}

void otcMainWindow::slotQuit()
{
	secureQuit();
}

void otcMainWindow::lastCleanUp()
{
   closeDevice();
}

// ---------------------------------------- //
//                                          //
//           GUI EVENT MANAGER              //
//                                          //
// ---------------------------------------- //

void otcMainWindow::customEvent(QEvent* e)
{
    switch((int)e->type())
    {
        case OTC_EVENT_DEVICE_READ :
        {
			if (!m_deviceblinker->read())
			{
				m_logWidget->append("Device data treatment thread does not respond...");
			}
            m_deviceblinker->update();
		}
        break;
        case OTC_EVENT_DEVICE_TREAT :
        {
			if (!m_deviceblinker->treat())
			{
				m_logWidget->append("Device data read thread does not respond...");
			}
            m_deviceblinker->update();
        }
        break;
        case OTC_EVENT_CLIENT_READ :
        {
			if (!m_clientblinker->read())
			{
				m_logWidget->append("Client data treatment thread does not respond...");
			}
            m_clientblinker->update();
		}
        break;
        case OTC_EVENT_CLIENT_TREAT :
        {
			if (!m_clientblinker->treat())
			{
				m_logWidget->append("Client data read thread does not respond...");
			}
            m_clientblinker->update();
        }
        break;
		
		case OTC_EVENT_CHANGE_BAUDRATE :
        {
            changeBaudrateAndUpdate(((otcBaudrateChangeEvent*)e)->baudrate());
        }
        break;
        case OTC_EVENT_CHANGE_FLOW :
        {
            changeFlowModeAndUpdate(((otcFlowModeChangeEvent*)e)->mode());
        }
        break;
        case OTC_EVENT_RECONNECT_COMPORT :
        {
            reconnectDevice();
        }
        break;
        case OTC_EVENT_KILL:
        {
            qApp->quit();
        }
        break;
        case OTC_EVENT_CLEAN:
        {
            clearScreen();
        }
        break;
        case OTC_EVENT_FLUSH_FIFOS:
        {
            flushFifos();
        }
        break;
        default :
            QMainWindow::customEvent(e);
    }
}

void otcMainWindow::closeEvent(QCloseEvent*e)
{
	if(secureQuit())
		e->accept();
	else
		e->ignore();
}

void otcLogWidget::customEvent(QEvent* e)
{
    if((int)e->type() == OTC_EVENT_LOG)
    {
        append(((otcLogMessageEvent*)e)->getMessage());
        if(length()>500000)
        {
            clear();
            append("OTCOM secure clean...");
        }
    }
    else
        Q3TextEdit::customEvent(e);
}


void otcMainWindow::slotShowOrMinimize()
{
	if(isVisible())
	{
		showMinimized();
		hide();
	}
	else
	{
		showNormal();
		show();
		setActiveWindow();
                if(m_commandParser->isVisible())
                    m_commandParser->setFocus();
	}
}

void otcMainWindow::hideEvent(QHideEvent*) { };


