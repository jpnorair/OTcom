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
/// @file           otc_mpipe.h
/// @brief          OT Mpipe Protocol (Message Builder + Parser)
///                 NDEF builder and parser (OT restricted subset)
//
// =========================================================================
//
/// Please refer to free documentation available on the NFCForum website if you
/// want to learn more about NDEF. There is some limited documentation
/// paraphrased below from the DASH7 Mode 2 spec and NDEF spec.
///
/// OpenTag API Messages follow the Application Subprotocol rubric, which is
/// shown below.  This format is encapsulated in NDEF when transmitted over the
/// API Messaging Interface (usually a 1:1 wireline, like USB/Serial)
/// +----------------+----------------+----------------+----------------+
/// | Directive Len  |  Directive ID  | Directive Cmd  | Directive Data |
/// +----------------+----------------+----------------+----------------+
/// |    2 Bytes     |    1 Byte      |    1 Byte      |    N-4 Bytes   |
/// +----------------+----------------+----------------+----------------+
/// |       N        | encoded value  | encoded value  |   bytestream   |
/// | (unsigned int) |    (char)      |    (char)      |    (char[])    |
/// +----------------+----------------+----------------+----------------+
///
/// When encapsulated in NDEF, the above message format is slightly altered.
/// An NDEF Message consists of multiple "Records," which is an identical 
/// concept to the way a Subprotocol Message can contain multiple (batched)
/// Directives.  But the NDEF format has some additional overhead that we honor
/// (because we like NFC and Android).
/// 
///   07   06   05   04   03   02   01   00        OpenTag Subset Usage
/// +----+----+----+----+----+----+----+----+  =================================
/// | MB | ME | CF | SR | IL |     TNF      |  --> xxx11101 or 0xx10110
/// +----+----+----+----+----+----+----+----+
/// |      TYPE LENGTH (1 Byte: TLEN)       |  --> Always zero
/// +----+----+----+----+----+----+----+----+
/// |  PAYLOAD LENGTH (1 or 4 Bytes: PLEN)  |  --> Always one byte, 0-255
/// +----+----+----+----+----+----+----+----+
/// |    ID LENGTH (0 or 1 Bytes: IDLEN)    |  --> When IL=1, value always is 2
/// +----+----+----+----+----+----+----+----+
/// |          TYPE (TLEN Bytes)            |  --> Never present
/// +----+----+----+----+----+----+----+----+
/// |           ID (IDLEN Bytes)            |  --> When IL=1, Two bytes
/// +----+----+----+----+----+----+----+----+
/// |         PAYLOAD (PLEN Bytes)          |  --> 0-255 bytes
/// +----+----+----+----+----+----+----+----+
/// 
/// For Application Layer Protocol directives packed inside NDEF records:
///
/// - Initial TNF uses the UNKNOWN setting, with TLEN=0 and no TYPE Field
/// - Subsequent TNFs (if chunking) use UNCHANGED
/// - the ID is always a 2 byte value containing the Directive ID and Cmd
/// - Directive Length is subtracted by 4 and stored in Payload Length field
/// 
/// Thus defined, an OT-NDEF packet header can take the following values:
///
/// - no chunking : 11011101 (any)
/// - chunking : 10111101 (start), 00110110 (continue), 01010110 (end)
///
// =========================================================================

#ifndef __OTC_MPIPE_H__
#define __OTC_MPIPE_H__

// ----------------------------------
//
// CRC table (CRC16 CCITT value)
//
// ----------------------------------

const unsigned short crcLut[] = 
{
    0x0000,    0x1021,    0x2042,    0x3063,    0x4084,    0x50A5,    0x60C6,    0x70E7,
    0x8108,    0x9129,    0xA14A,    0xB16B,    0xC18C,    0xD1AD,    0xE1CE,    0xF1EF,
    0x1231,    0x0210,    0x3273,    0x2252,    0x52B5,    0x4294,    0x72F7,    0x62D6,
    0x9339,    0x8318,    0xB37B,    0xA35A,    0xD3BD,    0xC39C,    0xF3FF,    0xE3DE,
    0x2462,    0x3443,    0x0420,    0x1401,    0x64E6,    0x74C7,    0x44A4,    0x5485,
    0xA56A,    0xB54B,    0x8528,    0x9509,    0xE5EE,    0xF5CF,    0xC5AC,    0xD58D,
    0x3653,    0x2672,    0x1611,    0x0630,    0x76D7,    0x66F6,    0x5695,    0x46B4,
    0xB75B,    0xA77A,    0x9719,    0x8738,    0xF7DF,    0xE7FE,    0xD79D,    0xC7BC,
    0x48C4,    0x58E5,    0x6886,    0x78A7,    0x0840,    0x1861,    0x2802,    0x3823,
    0xC9CC,    0xD9ED,    0xE98E,    0xF9AF,    0x8948,    0x9969,    0xA90A,    0xB92B,
    0x5AF5,    0x4AD4,    0x7AB7,    0x6A96,    0x1A71,    0x0A50,    0x3A33,    0x2A12,
    0xDBFD,    0xCBDC,    0xFBBF,    0xEB9E,    0x9B79,    0x8B58,    0xBB3B,    0xAB1A,
    0x6CA6,    0x7C87,    0x4CE4,    0x5CC5,    0x2C22,    0x3C03,    0x0C60,    0x1C41,
    0xEDAE,    0xFD8F,    0xCDEC,    0xDDCD,    0xAD2A,    0xBD0B,    0x8D68,    0x9D49,
    0x7E97,    0x6EB6,    0x5ED5,    0x4EF4,    0x3E13,    0x2E32,    0x1E51,    0x0E70,
    0xFF9F,    0xEFBE,    0xDFDD,    0xCFFC,    0xBF1B,    0xAF3A,    0x9F59,    0x8F78,
    0x9188,    0x81A9,    0xB1CA,    0xA1EB,    0xD10C,    0xC12D,    0xF14E,    0xE16F,
    0x1080,    0x00A1,    0x30C2,    0x20E3,    0x5004,    0x4025,    0x7046,    0x6067,
    0x83B9,    0x9398,    0xA3FB,    0xB3DA,    0xC33D,    0xD31C,    0xE37F,    0xF35E,
    0x02B1,    0x1290,    0x22F3,    0x32D2,    0x4235,    0x5214,    0x6277,    0x7256,
    0xB5EA,    0xA5CB,    0x95A8,    0x8589,    0xF56E,    0xE54F,    0xD52C,    0xC50D,
    0x34E2,    0x24C3,    0x14A0,    0x0481,    0x7466,    0x6447,    0x5424,    0x4405,
    0xA7DB,    0xB7FA,    0x8799,    0x97B8,    0xE75F,    0xF77E,    0xC71D,    0xD73C,
    0x26D3,    0x36F2,    0x0691,    0x16B0,    0x6657,    0x7676,    0x4615,    0x5634,
    0xD94C,    0xC96D,    0xF90E,    0xE92F,    0x99C8,    0x89E9,    0xB98A,    0xA9AB,
    0x5844,    0x4865,    0x7806,    0x6827,    0x18C0,    0x08E1,    0x3882,    0x28A3,
    0xCB7D,    0xDB5C,    0xEB3F,    0xFB1E,    0x8BF9,    0x9BD8,    0xABBB,    0xBB9A,
    0x4A75,    0x5A54,    0x6A37,    0x7A16,    0x0AF1,    0x1AD0,    0x2AB3,    0x3A92,
    0xFD2E,    0xED0F,    0xDD6C,    0xCD4D,    0xBDAA,    0xAD8B,    0x9DE8,    0x8DC9,
    0x7C26,    0x6C07,    0x5C64,    0x4C45,    0x3CA2,    0x2C83,    0x1CE0,    0x0CC1,
    0xEF1F,    0xFF3E,    0xCF5D,    0xDF7C,    0xAF9B,    0xBFBA,    0x8FD9,    0x9FF8,
    0x6E17,    0x7E36,    0x4E55,    0x5E74,    0x2E93,    0x3EB2,    0x0ED1,    0x1EF0
};

// ----------------------------------
//
// General NDEF Definitions
//
// ----------------------------------

/// NDEF TNF Field Values
/// ---------------------
/// Constants and an enum to define the TNF (Type Name Field).  TNF is a 3 bit
/// id that goes in the same Message/Record byte as the NDEF Control Bits.
/// OpenTag currently uses the "UNKNOWN" TNF exclusively, which means that the
/// Type field is omitted, and the client/server are implicitly programmed to
/// only deal with TNFs of this type.

#define OTC_MPIPE_TNF_MASK               0X07
#define OTC_MPIPE_TNF_EMPTY              0x00
#define OTC_MPIPE_TNF_WELLKNOWN          0x01
#define OTC_MPIPE_TNF_MEDIATYPE          0x02
#define OTC_MPIPE_TNF_ABSOLUTEURI        0x03
#define OTC_MPIPE_TNF_EXTERNAL           0x04
#define OTC_MPIPE_TNF_UNKNOWN            0x05
#define OTC_MPIPE_TNF_UNCHANGED          0x06
#define OTC_MPIPE_TNF_RESERVED           0x07

/// NDEF header structure
typedef union 
{
    struct
    {
        unsigned int typeName           :3;
        unsigned int idLength           :1;
        unsigned int shortRecord        :1;
        unsigned int chunk              :1;
        unsigned int msgEnd             :1;
        unsigned int msgStart           :1;
    };
    unsigned int flags;

} otc_mpipe_header_flags_t;


/// OT specific sync word
typedef enum
{
    OTC_MPIPE_SYNC_WORD_CHUNK_NO         = 0xDD,
    OTC_MPIPE_SYNC_WORD_CHUNK_FIRST      = 0xBD,
    OTC_MPIPE_SYNC_WORD_CHUNK_CONTINUE   = 0x36,
    OTC_MPIPE_SYNC_WORD_CHUNK_LAST       = 0x56

} otc_mpipe_superstate_t;


// ----------------------------------
//
// MPIPE Builder Specific
//
// ----------------------------------

/// NDEF Header size
#define OTC_MPIPE_HEADER_SIZE            6

/// NDEF Footer size
#define OTC_MPIPE_FOOTER_SIZE            4


/// OT-NDEF Builder Object Class
class otc_mpipe_builder
{
public :
    
    otc_mpipe_builder(unsigned char bodylen);
    ~otc_mpipe_builder();

    void                                header(unsigned char id, unsigned char cmd);
    void                                body(unsigned char * data);
    void                                footer(unsigned short seq);
    int                                 len(void)	{return size;};
    unsigned char *                     start(void)	{return buffer;};

private :   

    unsigned char *                     buffer;
    int                                 size;
    otc_mpipe_superstate_t              superstate;
};


// ----------------------------------
//
// MPIPE Parser Specific
//
// ----------------------------------


/// Parser state
typedef enum 
{
    OTC_MPIPE_PARSER_STATE_SYNC          = 0,
    OTC_MPIPE_PARSER_STATE_START,
    OTC_MPIPE_PARSER_STATE_TYPELEN,
    OTC_MPIPE_PARSER_STATE_DATALEN,
    OTC_MPIPE_PARSER_STATE_IDLEN,
    OTC_MPIPE_PARSER_STATE_READ_FIRST,
    OTC_MPIPE_PARSER_STATE_READ_CONTINUE,
    OTC_MPIPE_PARSER_STATE_READ_TAIL,
    OTC_MPIPE_PARSER_STATE_DONE          = 0x7f,
    OTC_MPIPE_PARSER_STATE_ERROR         = 0x80

} otc_mpipe_parser_state_t;


/// Parser buffer indexes
typedef enum
{
    OTC_MPIPE_BUFFER_INDEX_FIRST         = 0,
    OTC_MPIPE_BUFFER_INDEX_TYPE          = OTC_MPIPE_BUFFER_INDEX_FIRST,
    OTC_MPIPE_BUFFER_INDEX_ID,
    OTC_MPIPE_BUFFER_INDEX_DATA,
    OTC_MPIPE_BUFFER_INDEX_SEQ,
    OTC_MPIPE_BUFFER_INDEX_CRC,
    OTC_MPIPE_BUFFER_INDEX_QTY

} otc_mpipe_buffer_index_t;


/// Buffer class
typedef struct
{
    unsigned char  size;
    unsigned char  offset;
    unsigned char  remain;
    unsigned char* payload;

} otc_mpipe_buffer_t;


/// MPIPE OT-NDEF Parser Object Class
class otc_mpipe_parser
{

public :
    
    otc_mpipe_parser();
    ~otc_mpipe_parser();
    bool                            parse(unsigned char* buffer, int toread);
    bool                            sync(unsigned char* buffer);

private :   

    otc_mpipe_parser_state_t        state;
    otc_mpipe_superstate_t          superstate;
    otc_mpipe_header_flags_t        header;
    otc_mpipe_buffer_t              buffer[OTC_MPIPE_BUFFER_INDEX_QTY];
    unsigned int                    currentBuffer;
    unsigned short                  crc;
    bool                            crcStatus;
    QString                         msg;
    unsigned char                   cmd;
    unsigned char                   id;
    void                            free(otc_mpipe_buffer_t* buffer);
    void                            alloc(otc_mpipe_buffer_t* buffer);
    int                             append(otc_mpipe_buffer_t* buffer, unsigned char* data, int available);
    void                            crcbyte(unsigned char byte);
    void                            crccheck(void); 
    void                            print(void);
};


#endif // __OTC_MPIPE_H__

