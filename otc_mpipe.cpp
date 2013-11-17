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
otc_mpipe_builder::otc_mpipe_builder(unsigned char bodylen) {
    // allocate msg outbuf
    size = OTC_MPIPE_HEADER_SIZE + OTC_MPIPE_ALP_SIZE + bodylen;
    outbuf  = new unsigned char[size];
}


// Destructor
otc_mpipe_builder::~otc_mpipe_builder() {
    // delete msg outbuf
    delete outbuf;
    outbuf = NULL;
}

void otc_mpipe_builder::header(unsigned char id, unsigned char cmd){
    int payload_len;
    
    // Sync Word
    outbuf[0]   = OTC_MPIPE_SYNC_BYTE_0; 
    outbuf[1]   = OTC_MPIPE_SYNC_BYTE_1; 
    
    // CRC16 Placeholder
    outbuf[2]   = 0;
    outbuf[3]   = 0;
    
    // Message Payload Length
    payload_len = size - OTC_MPIPE_HEADER_SIZE;
    outbuf[4]   = (payload_len >> 8) & 0xFF;
    outbuf[5]   = (payload_len & 0xFF);
    
    // Sequence Number
    outbuf[6]   = seq;
    
    // Control Parameter
    ///@todo JPN: Now "Control" is always zero, but in the future it might have
    ///           other values.
    outbuf[7]   = 0;
    
    // ALP Record/Message
    outbuf[8]   = OTC_MPIPE_SYNC_WORD_CHUNK_NO; 
    
    // ALP Payload Length
    payload_len-= OTC_MPIPE_ALP_SIZE;
    outbuf[9]   = (payload_len & 0xff);
    
    // ALP ID and CMD
    outbuf[10]  = id; 
    outbuf[11]  = cmd;
}




void otc_mpipe_builder::body(unsigned char * data) {
    memcpy(&outbuf[12], data, outbuf[9]);
    
//    DEBUGGING
//    int i;
//    for (i=0; i<len(); i++) {
//        printf("%02X ", outbuf[i]);
//        if ((i & 15) == 0) {
//            printf("\n");
//        }
//    } printf("\n");
}


void otc_mpipe_builder::footer() {
    unsigned short crc = (unsigned short)0xFFFF;

    // CRC
    for (int i=4; i<(size-4); ++i) {
        crc = (crc << 8) ^ crcLut[((crc >> 8) & 0xff) ^ outbuf[i]];
    }
    outbuf[2] = crc & 0xff;  // CRC lo
    outbuf[3] = (crc >> 8);  // CRC hi
}



// ---------------------------------------- //
//                                          //
//           OT-ALP PARSER                 //
//                                          //
// ---------------------------------------- //

// Constructor
otc_mpipe_parser::otc_mpipe_parser() {
    state           = OTC_MPIPE_PARSER_STATE_SYNC;
    superstate      = OTC_MPIPE_SYNC_WORD_CHUNK_NO;
    
    // Allocate Buffers
    ///@note JPN: this is new code, but it seems bad to allocate packet buffers
    ///           every time a packet is detected.
    buffer[OTC_MPIPE_BUFFER_INDEX_CRC].size     = 2;
    alloc( &buffer[OTC_MPIPE_BUFFER_INDEX_CRC] );
    
    buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].size  = 4;
    alloc( &buffer[OTC_MPIPE_BUFFER_INDEX_HEADER] );
    
    buffer[OTC_MPIPE_BUFFER_INDEX_ALP].size  = 4;
    alloc( &buffer[OTC_MPIPE_BUFFER_INDEX_ALP] );
    
    buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size = 256;
    alloc( &buffer[OTC_MPIPE_BUFFER_INDEX_DATA] );
}


// Destructor
otc_mpipe_parser::~otc_mpipe_parser() {
    buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size = 256;

    for (int i=0; i<OTC_MPIPE_BUFFER_INDEX_QTY; ++i) {
        if (buffer[i].size) {
            delete buffer[i].payload;
            buffer[i].payload = NULL;
        }
    }
}


// Free a buffer (not used anymore)
void otc_mpipe_parser::free(otc_mpipe_buffer_t* buffer) {
    if (buffer->size) {
        delete buffer->payload;
        buffer->payload = NULL;
    }
    buffer->size    = 0;
    buffer->offset  = 0;
    buffer->remain  = 0;
}


// Allocate new buffer
void otc_mpipe_parser::alloc(otc_mpipe_buffer_t* buffer) {
    if (buffer->size) {
        buffer->offset  = 0;
        buffer->remain  = buffer->size;
        buffer->payload = new unsigned char[buffer->size];
    }
}


// Append new data to buffer
int otc_mpipe_parser::append(otc_mpipe_buffer_t* buffer, unsigned char* data, int available) {
    if (buffer->remain >= available) {
        memcpy(buffer->payload+buffer->offset, data, available);
        buffer->offset += available;
        buffer->remain -= available;
        return 0;
    }
    else {
        memcpy(buffer->payload+buffer->offset, data, buffer->remain);
        return (available - buffer->remain);
    }
}


void otc_mpipe_parser::crcbyte(unsigned char byte) {
    crc = (crc << 8) ^ crcLut[((crc >> 8) & 0xff) ^ byte];
}


// CRC check
void otc_mpipe_parser::crccheck() { 
    unsigned short rxcrc;
    
    // buffers
    crc = (unsigned short)0xFFFF;
    for (int i=OTC_MPIPE_BUFFER_INDEX_HEADER; i<OTC_MPIPE_BUFFER_INDEX_QTY; ++i) {
        for (int j=0; j<buffer[i].size; ++j) {
            crcbyte(buffer[i].payload[j]);
        }
    }
    
    rxcrc       = (unsigned short)buffer[OTC_MPIPE_BUFFER_INDEX_CRC].payload[1] \
                | ((unsigned short)buffer[OTC_MPIPE_BUFFER_INDEX_CRC].payload[0] << 8);

    crcStatus   = (bool)(crc == rxcrc);
}




// Parse an NDEF/OT packet (this is a subset of NDEF)
bool otc_mpipe_parser::parse(unsigned char* in, int toread) {
    while (toread != 0) {
        switch (state) {
            case OTC_MPIPE_PARSER_STATE_SYNC:
                state += (*in++ == 0xFF);
                toread--;
                break;
                
            case OTC_MPIPE_PARSER_STATE_SYNC2: 
                state += (((*in++ == 0x55) << 1) - 1);
                toread--;
                break;
                
            case OTC_MPIPE_PARSER_STATE_HEADER: {
                int payload_len;
                toread -= 6;
                if (toread < 0) {
                    return FALSE;
                }
                buffer[OTC_MPIPE_BUFFER_INDEX_CRC].payload[0]   = *in++;
                buffer[OTC_MPIPE_BUFFER_INDEX_CRC].payload[1]   = *in++;
                buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].payload[0]= *in++;
                buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].payload[1]= *in++;
                buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].payload[2]= *in++;
                buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].payload[3]= *in++;
                
                // State progression based on payload size
                payload_len = (int)buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].payload[0] << 8;
                payload_len+= (int)buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].payload[1];
                if (payload_len == 0) {
                    buffer[OTC_MPIPE_BUFFER_INDEX_ALP].size     = 0;
                    buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size    = 0;
                    state = OTC_MPIPE_PARSER_STATE_DONE;
                }
                else {
                    state = OTC_MPIPE_PARSER_STATE_ALP;
                }
            } break;
                
            case OTC_MPIPE_PARSER_STATE_ALP: {
                int payload_len;
                int chunkstate;
                
                toread -= 4;
                if (toread < 0) {
                    return FALSE;
                }
                
                buffer[OTC_MPIPE_BUFFER_INDEX_ALP].size         = 4;
                buffer[OTC_MPIPE_BUFFER_INDEX_ALP].payload[0]   = *in++;
                payload_len                                     = *in++;
                buffer[OTC_MPIPE_BUFFER_INDEX_ALP].payload[1]   = (unsigned char)payload_len;
                buffer[OTC_MPIPE_BUFFER_INDEX_ALP].payload[2]   = *in++;
                buffer[OTC_MPIPE_BUFFER_INDEX_ALP].payload[3]   = *in++;
                
                buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size        = payload_len;
                buffer[OTC_MPIPE_BUFFER_INDEX_DATA].remain      = payload_len;
                
                // default state progression
                state = (payload_len == 0) ? \
                            OTC_MPIPE_PARSER_STATE_DONE : \
                            OTC_MPIPE_PARSER_STATE_DATA;
                            
                // check superstate
                chunkstate = buffer[OTC_MPIPE_BUFFER_INDEX_ALP].payload[0] >> 5;
                switch (superstate) {
                    case OTC_MPIPE_SYNC_WORD_CHUNK_FIRST:
                    case OTC_MPIPE_SYNC_WORD_CHUNK_IMPLICIT:
                    case OTC_MPIPE_SYNC_WORD_CHUNK_CONTINUE: 
                        if ((chunkstate == OTC_MPIPE_SYNC_WORD_CHUNK_IMPLICIT) \
                        ||  (chunkstate == OTC_MPIPE_SYNC_WORD_CHUNK_CONTINUE) \
                        ||  (chunkstate == OTC_MPIPE_SYNC_WORD_CHUNK_LAST) ) {
                            superstate = (otc_mpipe_superstate_t)chunkstate;
                        }
                        else {
                            state = OTC_MPIPE_PARSER_STATE_ERROR;
                        }
                        break;
                    
                    case OTC_MPIPE_SYNC_WORD_CHUNK_LAST:
                    case OTC_MPIPE_SYNC_WORD_CHUNK_NO:
                    default: 
                        if ((chunkstate == OTC_MPIPE_SYNC_WORD_CHUNK_NO) \
                        ||  (chunkstate == OTC_MPIPE_SYNC_WORD_CHUNK_FIRST) ) {
                            superstate = (otc_mpipe_superstate_t)chunkstate;
                        }
                        else {
                            state = OTC_MPIPE_PARSER_STATE_ERROR;
                        }
                        break;
                }
            } break;

            case OTC_MPIPE_PARSER_STATE_DATA:
                toread = append(&buffer[OTC_MPIPE_BUFFER_INDEX_DATA], in, toread);
                if (buffer[OTC_MPIPE_BUFFER_INDEX_DATA].remain == 0) {
                    state = OTC_MPIPE_PARSER_STATE_DONE;
                }
                break;

            case OTC_MPIPE_PARSER_STATE_DONE :
                // enter here only if toread > 0
                print();
                state = OTC_MPIPE_PARSER_STATE_SYNC;        
                break;
            
            case OTC_MPIPE_PARSER_STATE_ERROR :
                // treat errors here
                state       = OTC_MPIPE_PARSER_STATE_SYNC;
                superstate  = OTC_MPIPE_SYNC_WORD_CHUNK_NO;
                break;

            default :
                toread      = 0;
                state       = OTC_MPIPE_PARSER_STATE_SYNC;
                superstate  = OTC_MPIPE_SYNC_WORD_CHUNK_NO;
                break;
        }
    }
            
    if (state == OTC_MPIPE_PARSER_STATE_DONE) {
        // clean finish
        print();
        state = OTC_MPIPE_PARSER_STATE_SYNC;
        return TRUE;
    }

    return FALSE;
}



void otc_mpipe_parser::print(void) {
    crccheck();
    
    // update msg type variables
    if (buffer[OTC_MPIPE_BUFFER_INDEX_ALP].size) {
        id  = buffer[OTC_MPIPE_BUFFER_INDEX_ALP].payload[2];
        cmd = buffer[OTC_MPIPE_BUFFER_INDEX_ALP].payload[3];
    }

    // start message
    if ((superstate == OTC_MPIPE_SYNC_WORD_CHUNK_NO) || (superstate == OTC_MPIPE_SYNC_WORD_CHUNK_FIRST)) {
        msg =   (!crcStatus) ?                  "<font color=red>" : 
                (OTC_ALP_CMD_LOG_ECHO == cmd) ? "<font color=gray>" : 
                                                "<font color=blue>";

        msg += QString("[ %1 ]  [ ").arg(buffer[OTC_MPIPE_BUFFER_INDEX_HEADER].payload[2]);

        ///@todo Make more internal ID writeouts, not only for LOG
        if (OTC_ALP_ID_LOG != id) {
            // print id
            msg += QString("%1 ").arg(QString("%1").arg(id,2,16).upper().replace(' ','0'));
            msg += QString("%1 ").arg(QString("%1").arg(cmd,2,16).upper().replace(' ','0'));
        }
        else {
            // Interpret as OT internal message (string or raw)
            msg += (OTC_ALP_CMD_LOG_ECHO == cmd)? "ECHO " : "LOG ";
        }

        msg += "]  [ ";
    }

    //message cursor
    int i = 0;  

    // Special handling for Logger packets
    if (id == OTC_ALP_ID_LOG) {
    
        // Special case: handle "Message" part of Logger payload (UTF8)
        if (cmd & 4) {
            // count through space terminator
            while ( (i < buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size) && \
                    (buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i++] != ' ') ) { }

            // Load UTF8 message label into beginning
            for (int j=0; j<i; j++) {
                msg += QString(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[j]).replace('\n',' ');
            }
        }

        // loop through main data payload
        for (; i<buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size; i++) {
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
    }
    
    // Not Logger: print raw data.... 
    else {
        for (; i<buffer[OTC_MPIPE_BUFFER_INDEX_DATA].size; i++) {
                msg += QString("%1 ").arg(QString("%1")
                                        .arg(buffer[OTC_MPIPE_BUFFER_INDEX_DATA].payload[i],2,16)
                                            .upper().replace(' ','0'));
        }
    }
    
    // end message
    if ((superstate == OTC_MPIPE_SYNC_WORD_CHUNK_NO) || (superstate == OTC_MPIPE_SYNC_WORD_CHUNK_LAST)) {
        msg += "]</font>";
        otcConfig::logText(msg);
    }
}


