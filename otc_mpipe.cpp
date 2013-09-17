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
/// @file           otc_mpipe.cpp
/// @brief          header of OT Mpipe Protocol (Message Builder + Parser)
///                 Contains definitions and classes for the NDEF builder 
///                 and parser (OT restricted subset).
//
/// =========================================================================
  
#include <q3textedit.h>
#include <string.h>
#include <qstring.h>
#include "otc_main.h"
#include "otc_alp.h"
#include "otc_mpipe.h"

// ---------------------------------------- //
//                                          //
//           MPIPE OT-NDEF BUILDER          //
//                                          //
// ---------------------------------------- //

// Constructor
otc_mpipe_builder::otc_mpipe_builder(unsigned char bodylen)
{
    // allocate msg buffer
    size = OTC_MPIPE_HEADER_SIZE + OTC_MPIPE_FOOTER_SIZE + bodylen;
    buffer  = new unsigned char[size];
}


// Destructor
otc_mpipe_builder::~otc_mpipe_builder()
{
    // delete msg buffer
    delete buffer;
    buffer = NULL;
}

void otc_mpipe_builder::header(unsigned char id, unsigned char cmd)
{
    // void message
    buffer[0] = OTC_MPIPE_SYNC_WORD_CHUNK_NO; 
    buffer[1] = 0; 
    buffer[2] = size - OTC_MPIPE_HEADER_SIZE - OTC_MPIPE_FOOTER_SIZE; 
    buffer[3] = 2; 
    buffer[4] = id; 
    buffer[5] = cmd;
}

void otc_mpipe_builder::body(unsigned char * data)
{
    memcpy(&buffer[6], data, buffer[2]);

    /*
    int i;
    for (i=0; i<len(); i++) {
        printf("%02X ", buffer[i]);
        if ((i & 15) == 0) {
            printf("\n");
        }
    } printf("\n");
    */
}


void otc_mpipe_builder::footer(unsigned short seq)
{
   unsigned short crc = (unsigned short)0xFFFF;

    // add sequence
    buffer[size-4] = (seq & 0xff);   // seq lo
    buffer[size-3] = (seq >> 8);     // seq hi

    // CRC
    for (int i = 0; i < size-2; ++i)
    {
        crc = (crc << 8) ^ crcLut[((crc >> 8) & 0xff) ^ buffer[i]];
    }

    buffer[size-2] = crc & 0xff;  // CRC lo
    buffer[size-1] = (crc >> 8);  // CRC hi
}


// ---------------------------------------- //
//                                          //
//           OT-NDEF PARSER                 //
//                                          //
// ---------------------------------------- //

// Constructor
otc_mpipe_parser::otc_mpipe_parser()
{
    for (int i = 0; i < OTC_MPIPE_BUFFER_INDEX_QTY; ++i)
    {
        buffer[i].size = 0;
        buffer[i].payload = NULL;
    }

    header.flags    = OTC_MPIPE_TNF_UNKNOWN;
    state           = OTC_MPIPE_PARSER_STATE_SYNC;
    superstate      = OTC_MPIPE_SYNC_WORD_CHUNK_NO;
    currentBuffer   = OTC_MPIPE_BUFFER_INDEX_FIRST;
}


// Destructor
otc_mpipe_parser::~otc_mpipe_parser()
{
    for (int i = 0; i < OTC_MPIPE_BUFFER_INDEX_QTY; ++i)
    {
        if (buffer[i].size)
        {
            delete buffer[i].payload;
            buffer[i].payload = NULL;
        }
    }
}

// Allocate new buffer
void otc_mpipe_parser::free(otc_mpipe_buffer_t* buffer)
{
    if (buffer->size)
    {
        delete buffer->payload;
        buffer->payload = NULL;
    }
    buffer->size = 0;
    buffer->offset = 0;
    buffer->remain = 0;
}


// Allocate new buffer
void otc_mpipe_parser::alloc(otc_mpipe_buffer_t* buffer)
{
    if (buffer->size)
    {
        buffer->offset = 0;
        buffer->remain = buffer->size;
        buffer->payload = new unsigned char[buffer->size];
    }
}

// Append new data to buffer
int otc_mpipe_parser::append(otc_mpipe_buffer_t* buffer, unsigned char* data, int available)
{
    if (buffer->remain >= available)
    {
        memcpy(buffer->payload+buffer->offset,data,available);
        buffer->offset += available;
        buffer->remain -= available;
        return 0;
    }
    else
    {
        memcpy(buffer->payload+buffer->offset,data,buffer->remain);
        return (available - buffer->remain);
    }
}


void otc_mpipe_parser::crcbyte(unsigned char byte)
{
    crc = (crc << 8) ^ crcLut[((crc >> 8) & 0xff) ^ byte];
}

// CRC check
void otc_mpipe_parser::crccheck() 
{ 
    crc = (unsigned short)0xFFFF;

    // header
    crcbyte((unsigned char)header.flags);

    // type length
    crcbyte(buffer[OTC_MPIPE_BUFFER_INDEX_TYPE].size);

    // data length
    crcbyte(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size);

    // id length
    if (header.idLength)
    {
        crcbyte(buffer[OTC_MPIPE_BUFFER_INDEX_ID].size);
    }

    // buffers
    for (int i = 0; i < OTC_MPIPE_BUFFER_INDEX_CRC; ++i)
    {
        for (int j = 0; j < buffer[i].size; ++j)
        {
            crcbyte(buffer[i].payload[j]);
        }
    }
    
    unsigned short
    receivedCrc = (unsigned short)buffer[OTC_MPIPE_BUFFER_INDEX_CRC].payload[1] |
                 ((unsigned short)buffer[OTC_MPIPE_BUFFER_INDEX_CRC].payload[0] << 8);

    crcStatus = (bool)(crc == receivedCrc);
}


// Detect NDEF/OT packet start
bool otc_mpipe_parser::sync(unsigned char* in)
{
    return  ((*in == OTC_MPIPE_SYNC_WORD_CHUNK_NO) || (*in == OTC_MPIPE_SYNC_WORD_CHUNK_FIRST) || 
                    (*in == OTC_MPIPE_SYNC_WORD_CHUNK_CONTINUE) || (*in == OTC_MPIPE_SYNC_WORD_CHUNK_LAST));
}


// Parse an NDEF/OT packet (this is a subset of NDEF)
bool otc_mpipe_parser::parse(unsigned char* in, int toread)
{
    while (toread)
    {
        switch (state)
        {
            case OTC_MPIPE_PARSER_STATE_SYNC :

                state = OTC_MPIPE_PARSER_STATE_START;
                
            case OTC_MPIPE_PARSER_STATE_START :

                // looking for sync word
                if (sync(in))
                {
                    // these two sizes are fixed
                    free(&buffer[OTC_MPIPE_BUFFER_INDEX_SEQ]);                
                    buffer[OTC_MPIPE_BUFFER_INDEX_SEQ].size = 2;
                    free(&buffer[OTC_MPIPE_BUFFER_INDEX_CRC]);                
                    buffer[OTC_MPIPE_BUFFER_INDEX_CRC].size = 2;

                    header.flags      = (unsigned int)*in;
                    currentBuffer     = OTC_MPIPE_BUFFER_INDEX_TYPE;
                    state             = OTC_MPIPE_PARSER_STATE_TYPELEN;

                    // check superstate
                    switch (superstate)
                    {
                        case OTC_MPIPE_SYNC_WORD_CHUNK_FIRST :
                        case OTC_MPIPE_SYNC_WORD_CHUNK_CONTINUE :

                            if ((*in == OTC_MPIPE_SYNC_WORD_CHUNK_CONTINUE) || (*in == OTC_MPIPE_SYNC_WORD_CHUNK_LAST))
                            {
                                superstate = (otc_mpipe_superstate_t)*in;
                            }
                            else
                            {
                                state = OTC_MPIPE_PARSER_STATE_ERROR;
                            }
                            break;

                        case OTC_MPIPE_SYNC_WORD_CHUNK_NO : 
                        case OTC_MPIPE_SYNC_WORD_CHUNK_LAST :
                        default :

                            if ((*in == OTC_MPIPE_SYNC_WORD_CHUNK_NO) || (*in == OTC_MPIPE_SYNC_WORD_CHUNK_FIRST))
                            {
                                superstate = (otc_mpipe_superstate_t)*in;
                            }
                            else
                            {
                                state = OTC_MPIPE_PARSER_STATE_ERROR;
                            }
                            break;
                        }
                }
                toread--;
                in++;
                break;

            case OTC_MPIPE_PARSER_STATE_TYPELEN :

                // looking for sync word 0x00
                toread--;
                if (*in++ != 0)
                {
                    state = OTC_MPIPE_PARSER_STATE_ERROR;
                    return FALSE;
                }
                buffer[OTC_MPIPE_BUFFER_INDEX_TYPE].size = 0;
                state = OTC_MPIPE_PARSER_STATE_DATALEN;
                break;
                
            case OTC_MPIPE_PARSER_STATE_DATALEN :

                free(&buffer[OTC_MPIPE_BUFFER_INDEX_TYPE]);               
                buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size = (*in++);
                state = OTC_MPIPE_PARSER_STATE_IDLEN;
                toread--;
                break;

            case OTC_MPIPE_PARSER_STATE_IDLEN : 

                // check ID len
                free(&buffer[OTC_MPIPE_BUFFER_INDEX_ID]);             
                buffer[OTC_MPIPE_BUFFER_INDEX_ID].size = 0;
                if (header.idLength)
                {
                    if (*in++ != 2)
                    {
                        state = OTC_MPIPE_PARSER_STATE_ERROR;
                        return FALSE;
                    }

                    buffer[OTC_MPIPE_BUFFER_INDEX_ID].size = 2;
                    state = OTC_MPIPE_PARSER_STATE_READ_FIRST;
                    toread--;
                }
                break;

            case OTC_MPIPE_PARSER_STATE_READ_FIRST :

                alloc(&buffer[currentBuffer]);
                if (buffer[currentBuffer].size)
                {
                    state = OTC_MPIPE_PARSER_STATE_READ_CONTINUE;
                }
                else
                {
                    currentBuffer++;
                    state = (OTC_MPIPE_BUFFER_INDEX_QTY == currentBuffer) ? OTC_MPIPE_PARSER_STATE_DONE : OTC_MPIPE_PARSER_STATE_READ_FIRST;
                }
                break;

            case OTC_MPIPE_PARSER_STATE_READ_CONTINUE :
                
                toread = append(&buffer[currentBuffer],in,toread);
                if ((toread) || (buffer[currentBuffer].remain == 0))
                {
                    in += buffer[currentBuffer].remain;
                    currentBuffer++;
                    state = (OTC_MPIPE_BUFFER_INDEX_QTY == currentBuffer) ? OTC_MPIPE_PARSER_STATE_DONE : OTC_MPIPE_PARSER_STATE_READ_FIRST;
                }
                break;

            case OTC_MPIPE_PARSER_STATE_DONE :
            
                // enter here only if toread > 0
                print();
                state = OTC_MPIPE_PARSER_STATE_START;        
                break;
            
            case OTC_MPIPE_PARSER_STATE_ERROR :

                // treat errors here
                state = OTC_MPIPE_PARSER_STATE_START;
                superstate = OTC_MPIPE_SYNC_WORD_CHUNK_NO;
                break;

            default :

                toread = 0;
                state = OTC_MPIPE_PARSER_STATE_START;
                superstate = OTC_MPIPE_SYNC_WORD_CHUNK_NO;
                break;
        }
    }
            
    if (state == OTC_MPIPE_PARSER_STATE_DONE)
    {
        // clean finish
        print();
        state = OTC_MPIPE_PARSER_STATE_START;
        return TRUE;
    }
    else 
    {
        return FALSE;
    }
}

void otc_mpipe_parser::print(void)
{
    crccheck();
    
    // update msg type variables
    if (buffer[OTC_MPIPE_BUFFER_INDEX_ID].size)
    {
        id  = buffer[OTC_MPIPE_BUFFER_INDEX_ID].payload[0];
        cmd = buffer[OTC_MPIPE_BUFFER_INDEX_ID].payload[1];
    }

    // start message
    if ((superstate == OTC_MPIPE_SYNC_WORD_CHUNK_NO) || (superstate == OTC_MPIPE_SYNC_WORD_CHUNK_FIRST))
    {
        msg =   (!crcStatus) ?                  "<font color=red>" : 
                (OTC_ALP_CMD_LOG_ECHO == cmd) ? "<font color=gray>" : 
                                                "<font color=blue>";

        msg += QString("[ %1 ]  [ ").arg(buffer[OTC_MPIPE_BUFFER_INDEX_SEQ].payload[1] + 256 * buffer[OTC_MPIPE_BUFFER_INDEX_SEQ].payload[0]);

        if (OTC_ALP_ID_LOG != id)
        {
            // print id
            for (int i = 0; i < buffer[OTC_MPIPE_BUFFER_INDEX_ID].size; i++)
            {
                msg += QString("%1 ").arg(QString("%1").arg(buffer[OTC_MPIPE_BUFFER_INDEX_ID].payload[i],2,16).upper().replace(' ','0'));
            }

        }
        else
        {
            // Interpret as OT internal message (string or raw)
            msg += (OTC_ALP_CMD_LOG_ECHO == cmd)? "ECHO " : "LOG ";
        }

        msg += "]  [ ";
    }

    //msg +=  (!crcStatus) ?                  "<font color=red>" :
    //        (OTC_ALP_CMD_LOG_ECHO == cmd) ? "<font color=gray>" :
    //                                        "<font color=blue>";

    int i = 0;  //message cursor

    // Handle Message Types
    if (cmd & 4) {
        // count through space terminator
        while ( (i < buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size) && \
                (buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i++] != ' ') ) { }

        // Load UTF8 message label into beginning
        for (int j=0; j<i; j++) {
            msg += QString(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[j]).replace('\n',' ');
        }
    }

    // loop through data
    for (; i < buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size; i++) {
        switch (cmd & 3) {
        case OTC_ALP_CMD_LOG_RAW:
            msg += QString("%1 ").arg(QString("%1")
                                    .arg(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i],2,16)
                                      .upper().replace(' ','0'));
            break;

        case OTC_ALP_CMD_LOG_UTF8:
            msg += QString( buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i]).replace('\n',' ');
            break;

        case OTC_ALP_CMD_LOG_UTF16:
            // Unsupported: I'm generally annoyed by how difficult Qt/C++ makes it to work with data
            i++;
            break;

        case OTC_ALP_CMD_LOG_UTF8HEX:
            // Add some spaces between the numbers
            msg += QString("%1%2 ").arg(QString(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i]),
                                        QString(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i+1]));
            i++;
            break;
        }
    }

    // end message
    if ((superstate == OTC_MPIPE_SYNC_WORD_CHUNK_NO) || (superstate == OTC_MPIPE_SYNC_WORD_CHUNK_LAST))
    {
        msg += "]</font>";
        otcConfig::logText(msg);
    }
}


#if 0
// Parse an generic NDEF packet
bool otc_mpipe_parser::parse(unsigned char* in, int toread)
{
    while (toread)
    {
        switch (state)
        {
            case OTC_MPIPE_PARSER_STATE_SYNC :

                // need some kind of sync word : that's dirty, but what else?
                state = (*in++ == (unsigned char)0xff) ? OTC_MPIPE_PARSER_STATE_START : OTC_MPIPE_PARSER_STATE_SYNC;
                toread--;
                break;

            case OTC_MPIPE_PARSER_STATE_START :

                header.flags      = (unsigned int)*in++;
                currentBuffer     = OTC_MPIPE_BUFFER_INDEX_TYPE;
                state             = OTC_MPIPE_PARSER_STATE_TYPELEN;
                toread--;
                break;
        
            case OTC_MPIPE_PARSER_STATE_TYPELEN :

                free(&buffer[OTC_MPIPE_BUFFER_INDEX_TYPE]);               
                buffer[OTC_MPIPE_BUFFER_INDEX_TYPE].size = (*in++);
                state = OTC_MPIPE_PARSER_STATE_DATALEN;
                toread--;       
                break;

            case OTC_MPIPE_PARSER_STATE_DATALEN :

                free(&buffer[OTC_MPIPE_BUFFER_INDEX_DATA]);               
                buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size = (*in++) + 4; // TODO : change mpipe in OT not to subtract 4 from payload
                state = OTC_MPIPE_PARSER_STATE_IDLEN;
                toread--;
                break;

            case OTC_MPIPE_PARSER_STATE_IDLEN : 

                free(&buffer[OTC_MPIPE_BUFFER_INDEX_ID]);             
                buffer[OTC_MPIPE_BUFFER_INDEX_ID].size = (header.idLength) ? (*in++) : 0;
                state = OTC_MPIPE_PARSER_STATE_READ_FIRST;
                toread -= header.idLength;
                break;

            case OTC_MPIPE_PARSER_STATE_READ_FIRST :

                alloc(&buffer[currentBuffer]);
                if (buffer[currentBuffer].size)
                {
                    state = OTC_MPIPE_PARSER_STATE_READ_CONTINUE;
                }
                else
                {
                    currentBuffer++;
                    state = (OTC_MPIPE_BUFFER_INDEX_SEQ == currentBuffer) ? OTC_MPIPE_PARSER_STATE_DONE : OTC_MPIPE_PARSER_STATE_READ_FIRST;
                }
                break;

            case OTC_MPIPE_PARSER_STATE_READ_CONTINUE :
                
                toread = append(&buffer[currentBuffer],in,toread);
                if ((toread) || (buffer[currentBuffer].remain == 0))
                {
                    in += buffer[currentBuffer].remain;
                    currentBuffer++;
                    state = (OTC_MPIPE_BUFFER_INDEX_SEQ == currentBuffer) ? OTC_MPIPE_PARSER_STATE_DONE : OTC_MPIPE_PARSER_STATE_READ_FIRST;
                }
                break;

            case OTC_MPIPE_PARSER_STATE_DONE :
            
                print();
                state = OTC_MPIPE_PARSER_STATE_SYNC;     
                break;
            
            case OTC_MPIPE_PARSER_STATE_ERROR :

                // treat errors here
                state = OTC_MPIPE_PARSER_STATE_START;
                break;

            default :

                toread = 0;
                state = OTC_MPIPE_PARSER_STATE_SYNC;
                break;
        }
    }
            
    if (state == OTC_MPIPE_PARSER_STATE_DONE)
    {
        // clean finish
        print();
        state = OTC_MPIPE_PARSER_STATE_SYNC;
        return TRUE;
    }
    else 
    {
        return FALSE;
    }
}

// print for generic message
void otc_mpipe_parser::print(void)
{
    msg = "<font color=green>";
    msg += "[ ";

    for (int i = 0; i < buffer[OTC_MPIPE_BUFFER_INDEX_TYPE].size; i++)
    {
        msg += QString("%1 ").arg(QString("%1").arg(buffer[OTC_MPIPE_BUFFER_INDEX_ID].payload[i],2,16).upper().replace(' ','0'));
    }

    msg += "]  [ ";

    for (int i = 0; i < buffer[OTC_MPIPE_BUFFER_INDEX_ID].size; i++)
    {
        msg += QString("%1 ").arg(QString("%1").arg(buffer[OTC_MPIPE_BUFFER_INDEX_ID].payload[i],2,16).upper().replace(' ','0'));
    }

    msg += "]  [ ";

    for (int i = 0; i < buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size; i++)
    {
        msg += QString("%1 ").arg(QString("%1").arg(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i],2,16).upper().replace(' ','0'));
    }
    msg += "]</font>";

    otcConfig::logText(msg);
}
#endif


