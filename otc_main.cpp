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
/// @file           otc_main.cpp
/// @brief          OTCOM main entry
//
/// =========================================================================

#include "otc_main.h"
#include "otc_window.h"
#include "otc_version.h"

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


// ---------------------------------------- //
//                                          //
//           MAIN                           //
//                                          //
// ---------------------------------------- //


int              otcConfig::argComPort = 0;
OTC_LINK_T       otcConfig::communicationLink = OTC_LINK_COM;
int              otcConfig::argBaudRate = 115200;
OTC_PRINT_MODE_T otcConfig::argPrintMode = OTC_PRINT_MODE_NDEF_PLUS_OT;
OTC_FLOW_T       otcConfig::argFlowMode = OTC_FLOW_NONE;
otcLogWidget*    otcConfig::logWidget = NULL;
otcMainWindow*   otcConfig::mainWindow = NULL;
int              otcConfig::argSocketPort = 1515;


int main( int argc, char *argv[] )
{
	const char title[] = "OTCOM " OTC_VERSION;

    QApplication a( argc, argv );

	switch(argc)
	{
	    case 4 :
	    {
            QString flowstring = QString(argv[3]).lower();
            if(flowstring=="xonxoff")
                otcConfig::argFlowMode = OTC_FLOW_XONXOFF;
            else if(flowstring=="hardware")
                otcConfig::argFlowMode = OTC_FLOW_HARDWARE;
            else
                otcConfig::argFlowMode = OTC_FLOW_NONE;
	    }
        // no break here
	    case 3 :
	    {
            otcConfig::argBaudRate = QString(argv[2]).toUInt();
			if(otcConfig::argBaudRate!= 9600
                    && otcConfig::argBaudRate!= 57600
                    && otcConfig::argBaudRate!= 115200
                    && otcConfig::argBaudRate!= 460800
                    && otcConfig::argBaudRate!= 921600)
                {
                    QMessageBox::critical(NULL, title, "Invalid baudrate!");
                    return 1;
                }
		}
        // no break here
	    case 2 :
		{
            QRegExp ex("COM(\\d+)",false);
            QRegExp ex1("RAWCOM(\\d+)",false);
            if(ex.search(QString(argv[1])) != -1)
            {
                otcConfig::communicationLink = OTC_LINK_COM;
                otcConfig::argComPort = ex.cap(1).toInt();
            }
            else if(ex1.search(QString(argv[1])) != -1)
            {
                otcConfig::communicationLink = OTC_LINK_COM;
                otcConfig::argComPort = ex1.cap(1).toInt();
            }
            else if(QString(argv[1])=="socket")
            {
                otcConfig::communicationLink = OTC_LINK_SOCKET;
            }
			else
			{
                QMessageBox::critical(NULL, title, "Invalid com link parameter");
				return 1;
			}
			break;
	    }
	    case 1 :
	    default :
		{
            otcConfig::communicationLink = OTC_LINK_COM;
            otcConfig::argComPort = 0;
			break;
		}
	}

	#include "icons/otcom.xpm"

	otcMainWindow* mw = new otcMainWindow();
	a.setMainWidget(mw);
    mw->setWindowIcon(*(new QIcon(otcomxpm)));
	mw->setCaption(title);
    mw->show();

	// This is a major problem.
	if(!mw->isHostServerOk())
	{
	    QMessageBox::critical(NULL, title, "Failed to launch TCP server!");
        // TODO : need to allow two otcom to run and conenct to different sockets
		//delete mw;
		//return 0;
	}

    int result = a.exec();
    delete mw;
    return result;
}


