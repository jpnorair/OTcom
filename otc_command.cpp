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
/// @file           otc_command.cpp
/// @brief          OT command line parser & action functions
//
/// =========================================================================

#include "otc_command.h"
#include "otc_window.h"
#include "otc_mpipe.h"
#include "otc_alp.h"
#include <qapplication.h>

#include "bintex.h"





/** Local Subroutines <BR>
  * =======================================================================<BR>
  *
  */


/** @brief  Parses a command string from supplied starting point until statement end
  * @param  cmd_string  (const QString&) The command string from OTcom user entry
  * @param  param       (QStringRef*) Parameter reference array, taken from cmd_string
  * @param  front_edge  (int) offset starting point into cmd_string
  * @param  num_params  (int) maximum/expected number of parameters in param[] array
  */
int sub_parse_statement(const QString& cmd_string, QStringRef param[], int front_edge, int num_params) {
    int i;

    for (i=0; i<num_params; i++) {
        int back_edge;

        // Use substring between parameter front edge and delimiter (back edge)
        // Whitespace is OK between parameter and delimiter
        back_edge = cmd_string.indexOf(QRegExp("[,;]"), front_edge);
        if (back_edge < 0) {
            back_edge = cmd_string.length();
        }
        param[i] = cmd_string.midRef(front_edge, (back_edge-front_edge));

        // Bypass Whitespace after delimiter, before next front_edge
        // front_edge will take -1 if back_edge is EOL
        //next        = back_edge+1;
        front_edge  = cmd_string.indexOf(QRegExp("\\S"), back_edge+1);
        //front_edge  = next;

        // Break if Back Edge is EOL or ';'
        if ((cmd_string.length() <= back_edge) || (cmd_string.at(back_edge) == QChar(';'))) {
            break;
        }
    }

    return front_edge;
}




// ---------------------------------------- //
//                                          //
//           GENERAL COMMAND                //
//                                          //
// ---------------------------------------- //

otc_command::otc_command(const QString& mainarg,const QString& syntax,const QString& help)
{
	m_mainArg		= mainarg;
	m_argsSyntax	= syntax;
	m_help			= help;
}

// ---------------------------------------- //
//                                          //
//             OTCOM Internal               //
//             --------------               //
//             :S                           //
//             :V                           //
//                                          //
// ---------------------------------------- //

otc_command_internal::otc_command_internal( const QString& mainarg,
                                            otc_command_internal_id_t id,
                                            const QString& help)
                                            :otc_command(mainarg,"",help)
{
    m_id = id;
}



otc_error_t otc_command_internal::exec(const QString& cmd_string,
                                       otcCommunicationLinkDevice&,
                                       otc_command_parser* parser)
{
    switch (m_id)
    {
        case OTC_COMMAND_INTERNAL_ID_STATUS  :
            parser->log("(otcom internal status query)",NULL,0);
            otcConfig::mainWindow->printStatus();
            break;

        case OTC_COMMAND_INTERNAL_ID_VERBOSE : 
        case OTC_COMMAND_INTERNAL_ID_ECHO_REMOTE : 
        case OTC_COMMAND_INTERNAL_ID_ECHO_LOCAL : 
            parser->toggle(m_id); 
            break;

        default : 
	        return OTC_ERROR_UNKNOWN;
    }

    return OTC_ERROR_NONE;
}



// ---------------------------------------- //
//                                          //
//              ALP NULL                    //
//                                          //
// ---------------------------------------- //

otc_command_null::otc_command_null( const QString& mainarg,
                                    unsigned char cmd,
                                    const QString& help)
                                    :otc_command(mainarg,"",help)
{
    m_cmd = cmd;
    m_log = help;
}



otc_error_t otc_command_null::exec(const QString& cmd_string,
                                   otcCommunicationLinkDevice& device,
                                   otc_command_parser* parser)
{
    // construct message with NULL body
    otc_mpipe_builder msg(0);
    msg.header(OTC_ALP_ID_NULL, m_cmd | parser->echo(), parser->seq());
    msg.footer();

    // write to serial
    parser->log(m_log, msg.start(), msg.len());
    device.writeBlock((char*)msg.start(), msg.len());

	return OTC_ERROR_NONE;
}




// ---------------------------------------- //
//                                          //
//              ALP RAW                     //
//                                          //
// ---------------------------------------- //

otc_command_raw::otc_command_raw(   const QString& mainarg,
                                    unsigned char cmd,
                                    const QString& help)
                                    :otc_command(mainarg,"",help)
{
    m_cmd = cmd;
    m_log = help;
}



otc_error_t otc_command_raw::alptemp(QString s)
{
    /*
    m_bodyptr   = m_body;
    m_bodylen   = 0;
    s           = s + ",,";

    QStringList list = s.split(",");

    if(!list[0].startsWith("0x")) {
        list[0] = "0x0" + list[0];
    }
    if(!list[1].startsWith("0x")) {
        list[1] = "0x0" + list[1];
    }

    // Get ID and CMD from first two template arguments
    m_id    = (unsigned char)strtoul(list[0].latin1(),NULL,0);
    m_cmd  |= (unsigned char)strtoul(list[1].latin1(),NULL,0);

    // Last template argument is a BinTex string, which is the payload
    int size;
    size        = bintex_ss((unsigned char*)list[2].toUtf8().data(), m_bodyptr, 256-10);
    m_bodylen  += size;
    m_bodyptr  += size;
    
    return (size < 0) ? OTC_ERROR_SYNTAX : OTC_ERROR_NONE;
    */
    return OTC_ERROR_SYNTAX;
}





otc_error_t otc_command_raw::exec(const QString& cmd_string,
                                  otcCommunicationLinkDevice& device,
                                  otc_command_parser* parser  )
{
    int front_edge;
    QStringRef param[3];

    // Initialize message globals for new message
    m_bodyptr   = m_body;
    m_bodylen   = 0;

    param[0].clear();
    param[1].clear();
    param[2].clear();

    front_edge  = cmd_string.indexOf(QChar(' '));                   //go past command
    front_edge  = cmd_string.indexOf(QRegExp("\\S"), front_edge);   //bypass trailing whitespace

    /// There must be at least two parameters, and there are often three.
    /// The third parameter is the BinTex expression
    sub_parse_statement(cmd_string, param, front_edge, 3);

    /// If Dir ID or Dir Cmd are not present, the ALP command cannot work!
    if ((param[0].length() == 0) || (param[1].length() == 0)) {
        return OTC_ERROR_SYNTAX;
    }

    /// Convert from Hex, and return syntax error if the supplied values
    /// cannot be identified as valid hex.
    int hexval;
    bool ok = true;
    hexval  = param[0].toUtf8().toInt(&ok, 16);
    m_id    = (unsigned char)(hexval & 0xFF);
    hexval  = param[1].toUtf8().toInt(&ok, 16);
    m_cmd  |= (unsigned char)(hexval & 0xFF);

    if (ok != true) {
        return OTC_ERROR_SYNTAX;
    }

    /// Now, the BinTex data input, the final [optional] parameter.
    if (param[2].length() != 0) {
        int bytes_out;
        bytes_out   = bintex_ss((unsigned char*)param[2].toUtf8().data(),
                                m_bodyptr,
                                256-m_bodylen);
        m_bodyptr  += bytes_out;
        m_bodylen  += bytes_out;
    }

    // we do not treat multipackets for now
    if (m_bodylen > 255)
        return OTC_ERROR_MSG_TOO_LONG;

    // construct message with NULL body
    otc_mpipe_builder msg((unsigned char)m_bodylen);
    msg.header(m_id, m_cmd | parser->echo(), parser->seq());
    msg.body(m_body);
    msg.footer();

    // write to serial
    parser->log(m_log, msg.start(), msg.len());
    device.writeBlock((char*)msg.start(), msg.len());

    return OTC_ERROR_NONE;
}




// ---------------------------------------- //
//                                          //
//          ALP FILE DATA READ              //
//                                          //
// ---------------------------------------- //

otc_command_file::otc_command_file( const QString& mainarg,
                                    unsigned char cmd,
                                    otc_command_file_template_t temp,
                                    const QString& help)
                                    :otc_command(mainarg,"",help)
{
    m_cmd       = cmd;
    m_template  = temp;
    m_log       = help;
}



//otc_error_t otc_command_file::filetype(QString& s)
//{
//    return OTC_ERROR_NONE;
//}



otc_error_t otc_command_file::filetemp(QString s)
{
    /*
    unsigned short tmp, size = 0;
    s = s + ",,,";
    QStringList list = s.split(",");;

    switch (m_template)
    {
        case OTC_COMMAND_FILE_TEMPLATE_WRITE :
            // TODO : only raw supported?
            //memcpy(m_bodyptr + 5, list[3], list[3].length());
            //size = list[3].length();

            unsigned char* bodyptr;
            bodyptr     = m_bodyptr+5;
            size        = bintex_ss((unsigned char*)list[2].toUtf8().data(), m_bodyptr, 256-10);
            if (size < 0) {
                return OTC_ERROR_SYNTAX;
            }
            // no break

        case OTC_COMMAND_FILE_TEMPLATE_READ :

            if(!list[2].startsWith("0x")) list[2] = "0x0" + list[2];
            tmp = (unsigned short)strtoul(list[2].latin1(),NULL,0);
                m_bodyptr[3] = (tmp/256) & 0xFF ;            // length hi
                m_bodyptr[4] = (unsigned char)(tmp & 0xFF);  // length lo

            if(!list[1].startsWith("0x")) list[1] = "0x0" + list[1];
            tmp = (unsigned short)strtoul(list[1].latin1(),NULL,0);
                m_bodyptr[1] = (tmp/256) & 0xFF ;            // length hi
                m_bodyptr[2] = (unsigned char)(tmp & 0xFF);  // length lo

            
            size += 4;
            // no break

        case OTC_COMMAND_FILE_TEMPLATE_SHORT :
            
            if(!list[0].startsWith("0x")) list[0] = "0x0" + list[0];
            m_bodyptr[0] = (unsigned char)strtoul(list[0].latin1(),NULL,0);
            size += 1;

        default :
            break;
    }

    m_bodyptr += size;
    m_bodylen += size;
*/
    return OTC_ERROR_NONE;
}




otc_error_t otc_command_file::exec(const QString& cmd_string,
                                   otcCommunicationLinkDevice& device,
                                   otc_command_parser* parser)
{
    int front_edge;
    int back_edge;
    QStringRef  block;

    // Initialize message globals for new message
    m_bodyptr   = &m_body[0];
    m_bodylen   = 0;

    /// Get file block identifier, then check it
    /// Valid: GFB, ISS/ISSB/ISFSB, ISF/ISFB
    front_edge  = cmd_string.indexOf(QChar(' '));                   //go past command
    front_edge  = cmd_string.indexOf(QRegExp("\\S"), front_edge);   //bypass trailing whitespace
    back_edge   = cmd_string.indexOf(QChar(' '), front_edge);       //go past block param
    block       = cmd_string.midRef(front_edge, (back_edge-front_edge));
    front_edge  = cmd_string.indexOf(QRegExp("\\S"), back_edge+1);  //bypass trailing whitespace

    if (block.length() == 0) {
        return OTC_ERROR_SYNTAX;
    }
    else if (block.startsWith("G")) {
        m_cmd |= OTC_ALP_CMD_FILE_GFB;
    }
    else if (block.startsWith("ISFS") || block.startsWith("ISS")) {
        m_cmd |= OTC_ALP_CMD_FILE_ISFSB;
    }
    else if (block.startsWith("ISF")) {
        m_cmd |= OTC_ALP_CMD_FILE_ISFB;
    }
    else {
        return OTC_ERROR_SYNTAX;
    }

    /// Loop through Parameter lists.  These should be in the form:
    /// a1,a2,a3,[a4]; b1,b2,b3,[b4]; ...
    while (front_edge >= 0) {
        QStringRef  param[4];
        int         hexvalue;
        bool        ok = true;

        param[0].clear();
        param[1].clear();
        param[2].clear();
        param[3].clear();

        front_edge = sub_parse_statement(cmd_string, param, front_edge, 4);

        /// File ID: 1 byte, default=0
        if (param[0].length() == 0) {
            hexvalue = 0;
        }
        else {
            hexvalue = param[0].toUtf8().toInt(&ok, 16);
        }
        *m_bodyptr++    = (unsigned char)(hexvalue & 0xFF);
        m_bodylen      += 1;


        /// Data Offset: 2 bytes, default=0
        /// Data Length: 2 bytes, default=FFFF
        if (m_template != OTC_COMMAND_FILE_TEMPLATE_SHORT) {
            m_bodylen += 4;

            if (param[1].length() == 0) {
                hexvalue = 0;
            }
            else {
                hexvalue = param[1].toUtf8().toInt(&ok, 16);
            }
            *m_bodyptr++ = (unsigned char)((hexvalue / 256) & 0xFF);
            *m_bodyptr++ = (unsigned char)(hexvalue & 0xFF);

            if (param[2].length() == 0) {
                hexvalue = 0xFFFF;
            }
            else {
                hexvalue = param[2].toUtf8().toInt(&ok, 16);
            }
            *m_bodyptr++ = (unsigned char)((hexvalue / 256) & 0xFF);
            *m_bodyptr++ = (unsigned char)(hexvalue & 0xFF);


            /// Now do the BinTex parsing of the write parameter
            if (m_template == OTC_COMMAND_FILE_TEMPLATE_WRITE) {
                int bytes_out;
                bytes_out   = bintex_ss((unsigned char*)param[3].toUtf8().data(),
                                        m_bodyptr,
                                        256-m_bodylen);
                m_bodyptr  += bytes_out;
                m_bodylen  += bytes_out;
                ok         &= (bytes_out >= 0);
            }
        }

        /// Make sure all parameters are valid
        if (ok == false) {
            return OTC_ERROR_SYNTAX;
        }

        /// Make sure body is not too long (no multipackets for now)
        if (m_bodylen > 255) {
            return OTC_ERROR_MSG_TOO_LONG;
        }
    }


    /// construct message with NULL body
    otc_mpipe_builder msg((unsigned char)m_bodylen);
    msg.header(OTC_ALP_ID_FILE_DATA, m_cmd | parser->echo(), parser->seq());
    msg.body(&m_body[0]);
    msg.footer();

    /// write to serial
    parser->log(m_log, msg.start(), msg.len());
    device.writeBlock((char*)msg.start(), msg.len());

	return OTC_ERROR_NONE;
}




// ---------------------------------------- //
//                                          //
//           COMMAND PARSER                 //
//                                          //
// ---------------------------------------- //

otc_command_parser::otc_command_parser(QWidget* parent,otcCommunicationLinkDevice* device)
:QLineEdit(parent)
{
	m_device = device;
	m_commands.setAutoDelete(true);
    m_cmdIndex = 0;
    m_verbose  = FALSE;
    m_echo_remote = FALSE;
    m_echo_local = FALSE;
   
    // Internal otcom configuration commands
    add(new otc_command_internal(":S", OTC_COMMAND_INTERNAL_ID_STATUS,       "Shows the status of internal OTCOM data structures."));
    add(new otc_command_internal(":V", OTC_COMMAND_INTERNAL_ID_VERBOSE,      "Toggle command verbose mode."));
    add(new otc_command_internal(":ER", OTC_COMMAND_INTERNAL_ID_ECHO_REMOTE, "Toggle remote echo mode."));
    add(new otc_command_internal(":EL", OTC_COMMAND_INTERNAL_ID_ECHO_LOCAL,  "Toggle local echo mode."));
    
    // Null body commands
    add(new otc_command_null("OT",  OTC_ALP_RESP_NO , "Null command"));
    add(new otc_command_null("OT?", OTC_ALP_RESP_REQ, "Ping modem"));
    
    // Raw ALP commands
    add(new otc_command_raw("ALP",  OTC_ALP_RESP_NO  | 0, "Raw ALP template"));
    add(new otc_command_raw("ALP?", OTC_ALP_RESP_REQ | 0, "Raw ALP with response template"));

    // File data read commands
    add(new otc_command_file("OT+R",    OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_DATA,   OTC_COMMAND_FILE_TEMPLATE_READ,  "Read file data"));
    add(new otc_command_file("OT+R?",   OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_DATA,   OTC_COMMAND_FILE_TEMPLATE_READ,  "Read file data"));
    add(new otc_command_file("OT+RD",   OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_DATA,   OTC_COMMAND_FILE_TEMPLATE_READ,  "Read file data"));
    add(new otc_command_file("OT+RD?",  OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_DATA,   OTC_COMMAND_FILE_TEMPLATE_READ,  "Read file data"));
    add(new otc_command_file("OT+RA",   OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_ALL,    OTC_COMMAND_FILE_TEMPLATE_READ,  "Read file header & data"));
    add(new otc_command_file("OT+RA?",  OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_ALL,    OTC_COMMAND_FILE_TEMPLATE_READ,  "Read file header & data"));

    // File data misc commands
    add(new otc_command_file("OT+RP",   OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_PERM,   OTC_COMMAND_FILE_TEMPLATE_SHORT, "Read file permissions"));
    add(new otc_command_file("OT+RP?",  OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_PERM,   OTC_COMMAND_FILE_TEMPLATE_SHORT, "Read file permissions"));
    add(new otc_command_file("OT+RH",   OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_HEADER, OTC_COMMAND_FILE_TEMPLATE_SHORT, "Read file header"));
    add(new otc_command_file("OT+RH?",  OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RD_HEADER, OTC_COMMAND_FILE_TEMPLATE_SHORT, "Read file header"));
    add(new otc_command_file("OT+DEL",  OTC_ALP_RESP_NO  | OTC_ALP_CMD_FILE_DELETE,    OTC_COMMAND_FILE_TEMPLATE_SHORT, "Delete file"));
    add(new otc_command_file("OT+DEL?", OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_DELETE,    OTC_COMMAND_FILE_TEMPLATE_SHORT, "Delete file with response request"));
    add(new otc_command_file("OT+NEW",  OTC_ALP_RESP_NO  | OTC_ALP_CMD_FILE_CREATE,    OTC_COMMAND_FILE_TEMPLATE_SHORT, "Create file"));
    add(new otc_command_file("OT+NEW?", OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_CREATE,    OTC_COMMAND_FILE_TEMPLATE_SHORT, "Create file with response request"));
    add(new otc_command_file("OT+DEF",  OTC_ALP_RESP_NO  | OTC_ALP_CMD_FILE_RESTORE,   OTC_COMMAND_FILE_TEMPLATE_SHORT, "Restore file to default value"));
    add(new otc_command_file("OT+DEF?", OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_RESTORE,   OTC_COMMAND_FILE_TEMPLATE_SHORT, "Restore file to default value with response request"));

    // File data write commands
    add(new otc_command_file("OT+W",    OTC_ALP_RESP_NO  | OTC_ALP_CMD_FILE_WR_DATA,   OTC_COMMAND_FILE_TEMPLATE_WRITE, "Write file data"));
    add(new otc_command_file("OT+W?",   OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_WR_DATA,   OTC_COMMAND_FILE_TEMPLATE_WRITE, "Write file data with response request"));
    add(new otc_command_file("OT+WD",   OTC_ALP_RESP_NO  | OTC_ALP_CMD_FILE_WR_DATA,   OTC_COMMAND_FILE_TEMPLATE_WRITE, "Write file data"));
    add(new otc_command_file("OT+WD?",  OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_WR_DATA,   OTC_COMMAND_FILE_TEMPLATE_WRITE, "Write file data with response request"));
    add(new otc_command_file("OT+WP",   OTC_ALP_RESP_NO  | OTC_ALP_CMD_FILE_WR_PERM,   OTC_COMMAND_FILE_TEMPLATE_WRITE, "Write file permissions"));
    add(new otc_command_file("OT+WP?",  OTC_ALP_RESP_REQ | OTC_ALP_CMD_FILE_WR_PERM,   OTC_COMMAND_FILE_TEMPLATE_WRITE, "Write file permissions with response request"));

	connect(this, SIGNAL(returnPressed()), this, SLOT(parse()));
}




void otc_command_parser::add(otc_command* command, bool fantomCommand)
{
	if (command)
	{
		m_commands.insert(command->getMainArgument(),command);
		if(!fantomCommand)
			m_visibleCommands.append(command->getMainArgument());
	}
	else
	{
		m_visibleCommands.append("");
	}
}




void otc_command_parser::parse()
{
	QString cmd = text();

	if(cmd=="")
		return;

	m_commandMemory.prepend(cmd);
	otcConfig::logText("<font color=blue>&gt; "+cmd+"</font>");
	exec(cmd);
	otcConfig::logWidget->scrollToBottom();
	 
	setText("");
}




void otc_command_parser::exec(const QString& command)
{
	//For now, use simply "split" to create list of args
	//Ideally we should do a better parsing, to manage args in ""

    //QStringList argv    = QStringList::split(" ",command);
    int cmd_arg_length  = command.indexOf(QChar(' '), 0);
    QString cmd_arg     = command.left(cmd_arg_length);

    if (cmd_arg.length() > 0)
	{
        otc_command* cmd_type   = m_commands[cmd_arg];
        int res                 = cmd_type->exec(command, *m_device, this);

		switch(res)
		{
            case OTC_ERROR_NONE :
                break;

            case OTC_ERROR_SYNTAX :
                otcConfig::logText("<font color=red>**Syntax Error : </font><font color=darkgreen>" \
                                   + cmd_type->getMainArgument() + " " \
                                   + cmd_type->getSyntax() \
                                   + "</font>");
                break;

            default :
                otcConfig::logText("<font color=red>**Unknown Option : </font><font color=darkgreen>" \
                                   + cmd_type->getMainArgument() + " " \
                                   + cmd_type->getSyntax() \
                                   + "</font>");
                break;
		}
	}
    else if (command == "")
	{}
	else
	{
		otcConfig::logText("<font color=red>**Command Unknown!</font>");
	}
}



void otc_command_parser::toggle(otc_command_internal_id_t id)
{ 
    bool *onoff;
    QString s; 
    switch (id)
    {
        case OTC_COMMAND_INTERNAL_ID_VERBOSE : onoff = &m_verbose; s = "Verbose"; break;
        case OTC_COMMAND_INTERNAL_ID_ECHO_REMOTE : onoff = &m_echo_remote; s = "Echo remote"; break;
        case OTC_COMMAND_INTERNAL_ID_ECHO_LOCAL : onoff = &m_echo_local; s = "Echo local"; break;
        default : return;
    }

    *onoff = (*onoff) ? FALSE : TRUE;
    if (*onoff) 
          otcConfig::logText(s+" on"); 
    else 
          otcConfig::logText(s+" off");
}




void otc_command_parser::log(const QString& str, unsigned char* buffer, unsigned short len)
{
    if (m_verbose) 
        otcConfig::logText(str);
    
    if ((m_echo_local) && (buffer) && (len))
    {
        QString msg = QString("<font color=gray>[ %1 ]  [ SEND ]  [ ").arg(buffer[len-4] + 256 * buffer[len-3]);
        for (int i = 0; i < len; i++)
        {
            msg += QString("%1 ").arg(QString("%1").arg(buffer[i],2,16).upper().replace(' ','0'));
        }
        msg += " ]</font>";
        otcConfig::logText(msg);
    }
}

