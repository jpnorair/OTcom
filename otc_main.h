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
/// @file           otc_main.h
/// @brief          OTCOM configuration definitions and global variables declaration
//
/// =========================================================================

#ifndef OTC_CONFIG_H
#define OTC_CONFIG_H

#include <qstring.h>

#define OTC_COM_PORTS_MAX_NB    8
#define OTC_COM_PORTS_MAP_PATH  "/usr/commap"


typedef enum {
    OTC_LINK_COM = 0,
    OTC_LINK_USB,
    OTC_LINK_SOCKET
} OTC_LINK_T;


typedef enum {
    OTC_FLOW_UNVALID = -1,
    OTC_FLOW_NONE = 0,
    OTC_FLOW_HARDWARE,
    OTC_FLOW_XONXOFF
} OTC_FLOW_T;


typedef enum {
	OTC_PRINT_MODE_INVALID = -1,
	OTC_PRINT_MODE_HIDE = 0,
	OTC_PRINT_MODE_RAW,
	OTC_PRINT_MODE_NDEF,
	OTC_PRINT_MODE_NDEF_PLUS_OT,
	OTC_PRINT_MODE_QTY
} OTC_PRINT_MODE_T;


typedef	enum {
    OTC_ERROR_NONE                          = 0,
    OTC_ERROR_UNKNOWN                       = -1,
    OTC_ERROR_SYNTAX                        = -10,
    OTC_ERROR_MSG_TOO_LONG                  = -11,
    OTC_ERROR_MULTIMSG_CMD_NOT_SUPPORTED    = -12
} otc_error_t;


class QIcon;
class otcLogWidget;
class otcMainWindow;


class otcConfig {
public :
//#   ifdef WIN32
    static int			    argComPort;
//#   else
//    char                    argTtyFile[128];
//#   endif
    static int              argBaudRate;
    static OTC_PRINT_MODE_T argPrintMode;
    static OTC_FLOW_T       argFlowMode;
    static int              argSocketPort;
	static OTC_LINK_T       communicationLink;
	static otcLogWidget*    logWidget;
	static otcMainWindow*   mainWindow;
	static void             logText(const QString& str);
};


// DONT PLAY WITH THE FOLLOWING VALUES !!!!
#define OTC_COM_START_PORT		            7700
#define OTC_USB_READBLOCK_PACKET_MAX_SIZE   1000


#endif // OTC_CONFIG_H
