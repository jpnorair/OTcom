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
/// @file           otc_command.h
/// @brief          Classes & definitinons for OT command line parser & action functions
//
/// =========================================================================

#ifndef __OTC_COMMAND_H__
#define __OTC_COMMAND_H__

#include <qstring.h>
#include <qstringlist.h>
#include <q3textedit.h>
#include <qmap.h>
#include <qmutex.h>
#include <qlineedit.h>
#include <q3dict.h>

#include "otc_main.h"
#include "otc_alp.h"
#include "otc_serial.h"

// ---------------------------------------- //
//                                          //
//           GENERAL COMMAND                //
//                                          //
// ---------------------------------------- //
class otc_command_parser;


class otc_command {
public :
	otc_command(const QString& mainarg,const QString& argsSyntax,const QString& help);
    virtual ~otc_command() {;};
    
    virtual otc_error_t exec(   const QString& cmd_string,
                                otcCommunicationLinkDevice& device,
                                otc_command_parser* parser) = 0;

    const QString& getMainArgument()	{return m_mainArg;}
    const QString& getSyntax()			{return m_argsSyntax;}
    const QString& getHelp()			{return m_help;}
    const QString& getLastError()		{return m_lastError;}

protected :
	QString m_mainArg;
	QString m_argsSyntax;
	QString m_help;
	QString m_lastError;
};

// ---------------------------------------- //
//                                          //
//             OTCOM Internal               //
//                                          //
// ---------------------------------------- //

typedef enum {
    OTC_COMMAND_INTERNAL_ID_STATUS = 0,
    OTC_COMMAND_INTERNAL_ID_VERBOSE,
    OTC_COMMAND_INTERNAL_ID_ECHO_REMOTE,
    OTC_COMMAND_INTERNAL_ID_ECHO_LOCAL,
    OTC_COMMAND_INTERNAL_ID_QTY
} otc_command_internal_id_t;


class otc_command_internal : public otc_command {
public :
    otc_command_internal(const QString& mainarg, otc_command_internal_id_t id, const QString& help);
    virtual ~otc_command_internal() {;}
    otc_error_t exec(const QString& cmd_string, otcCommunicationLinkDevice& device, otc_command_parser* parser);
private :
    otc_command_internal_id_t m_id;
};

// ---------------------------------------- //
//                                          //
//                ALP NULL                  //
//                                          //
// ---------------------------------------- //

class otc_command_null : public otc_command {
public :
    otc_command_null(const QString& mainarg, unsigned char cmd, const QString& help);
    virtual ~otc_command_null() {;}
    otc_error_t exec(const QString& cmd_string, otcCommunicationLinkDevice& device, otc_command_parser* parser);
private :
    QString m_log;
    unsigned char m_cmd;
};


// ---------------------------------------- //
//                                          //
//                ALP RAW                   //
//                                          //
// ---------------------------------------- //

class otc_command_raw : public otc_command {
public :
    otc_command_raw(const QString& mainarg, unsigned char cmd, const QString& help);
    virtual ~otc_command_raw() {;}
    otc_error_t exec(const QString& cmd_string, otcCommunicationLinkDevice& device, otc_command_parser* parser);
private :
    otc_error_t     alptemp(QString s);
    QString         m_log;
    unsigned char   m_id;
    unsigned char   m_cmd;
    unsigned short  m_bodylen;
    unsigned char   m_body[256];
    unsigned char*  m_bodyptr;
};


// ---------------------------------------- //
//                                          //
//          ALP FILE_DATA                   //
//                                          //
// ---------------------------------------- //

typedef enum {
    OTC_COMMAND_FILE_TEMPLATE_SHORT, 
    OTC_COMMAND_FILE_TEMPLATE_READ,
    OTC_COMMAND_FILE_TEMPLATE_WRITE
} otc_command_file_template_t;


class otc_command_file : public otc_command {
public :
    otc_command_file(const QString& mainarg, unsigned char cmd, otc_command_file_template_t temp, const QString& help);
    virtual ~otc_command_file() {;}
    otc_error_t exec(const QString& cmd_string, otcCommunicationLinkDevice& device, otc_command_parser* parser);
private :
    otc_error_t                     filetype(QString s);
    otc_error_t                     filetemp(QString s);
    QString                         m_log;
    unsigned char                   m_cmd;
    unsigned short                  m_bodylen;
    unsigned char                   m_body[256];
    unsigned char *                 m_bodyptr;
    otc_command_file_template_t     m_template;
};


// ---------------------------------------- //
//                                          //
//           COMMAND PARSER                 //
//                                          //
// ---------------------------------------- //

class otc_command_parser : public QLineEdit {
	friend class otcExecCommand;
	friend class otcHelpCommand;
	friend class otcWincCommand;

	Q_OBJECT

public :
	otc_command_parser(QWidget* parent,otcCommunicationLinkDevice* device);

	void                                exec(const QString& commandline);
    void                                add(otc_command* command, bool fantomCommand = FALSE);
    void                                toggle(otc_command_internal_id_t id);
    void                                log(const QString& str, unsigned char* buffer, unsigned short len);
    otc_alp_resp_t                      echo(void) {return ((m_echo_remote) ? OTC_ALP_RESP_ECHO : OTC_ALP_RESP_NO);};
    int                                 seq(void) {return (m_cmdIndex++);};

protected :

	Q3Dict<otc_command>	                m_commands;
	QStringList			                m_visibleCommands;
	QStringList			                m_commandMemory;
	otcCommunicationLinkDevice*		    m_device;
	int                                 m_cmdIndex;
    bool                                m_verbose;
    bool                                m_echo_remote;
    bool                                m_echo_local;

protected slots :

	void                                parse(void);
};


#endif // __OTC_COMMAND_H__
