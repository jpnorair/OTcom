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
/// @file           otc_mpipe2.h
/// @brief          OT MPipe2 Protocol (Message Builder + Parser)
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
// CRC table (CRC16 value)
//
// ----------------------------------

#include "crc/crc16_table.h"

const unsigned short crcLut[] = { 
    CRCx00, CRCx01, CRCx02, CRCx03, CRCx04, CRCx05, CRCx06, CRCx07, 
    CRCx08, CRCx09, CRCx0A, CRCx0B, CRCx0C, CRCx0D, CRCx0E, CRCx0F, 
    CRCx10, CRCx11, CRCx12, CRCx13, CRCx14, CRCx15, CRCx16, CRCx17, 
    CRCx18, CRCx19, CRCx1A, CRCx1B, CRCx1C, CRCx1D, CRCx1E, CRCx1F, 
    CRCx20, CRCx21, CRCx22, CRCx23, CRCx24, CRCx25, CRCx26, CRCx27, 
    CRCx28, CRCx29, CRCx2A, CRCx2B, CRCx2C, CRCx2D, CRCx2E, CRCx2F, 
    CRCx30, CRCx31, CRCx32, CRCx33, CRCx34, CRCx35, CRCx36, CRCx37, 
    CRCx38, CRCx39, CRCx3A, CRCx3B, CRCx3C, CRCx3D, CRCx3E, CRCx3F, 
    CRCx40, CRCx41, CRCx42, CRCx43, CRCx44, CRCx45, CRCx46, CRCx47, 
    CRCx48, CRCx49, CRCx4A, CRCx4B, CRCx4C, CRCx4D, CRCx4E, CRCx4F, 
    CRCx50, CRCx51, CRCx52, CRCx53, CRCx54, CRCx55, CRCx56, CRCx57, 
    CRCx58, CRCx59, CRCx5A, CRCx5B, CRCx5C, CRCx5D, CRCx5E, CRCx5F, 
    CRCx60, CRCx61, CRCx62, CRCx63, CRCx64, CRCx65, CRCx66, CRCx67, 
    CRCx68, CRCx69, CRCx6A, CRCx6B, CRCx6C, CRCx6D, CRCx6E, CRCx6F, 
    CRCx70, CRCx71, CRCx72, CRCx73, CRCx74, CRCx75, CRCx76, CRCx77, 
    CRCx78, CRCx79, CRCx7A, CRCx7B, CRCx7C, CRCx7D, CRCx7E, CRCx7F, 
    CRCx80, CRCx81, CRCx82, CRCx83, CRCx84, CRCx85, CRCx86, CRCx87, 
    CRCx88, CRCx89, CRCx8A, CRCx8B, CRCx8C, CRCx8D, CRCx8E, CRCx8F, 
    CRCx90, CRCx91, CRCx92, CRCx93, CRCx94, CRCx95, CRCx96, CRCx97, 
    CRCx98, CRCx99, CRCx9A, CRCx9B, CRCx9C, CRCx9D, CRCx9E, CRCx9F, 
    CRCxA0, CRCxA1, CRCxA2, CRCxA3, CRCxA4, CRCxA5, CRCxA6, CRCxA7, 
    CRCxA8, CRCxA9, CRCxAA, CRCxAB, CRCxAC, CRCxAD, CRCxAE, CRCxAF, 
    CRCxB0, CRCxB1, CRCxB2, CRCxB3, CRCxB4, CRCxB5, CRCxB6, CRCxB7, 
    CRCxB8, CRCxB9, CRCxBA, CRCxBB, CRCxBC, CRCxBD, CRCxBE, CRCxBF, 
    CRCxC0, CRCxC1, CRCxC2, CRCxC3, CRCxC4, CRCxC5, CRCxC6, CRCxC7, 
    CRCxC8, CRCxC9, CRCxCA, CRCxCB, CRCxCC, CRCxCD, CRCxCE, CRCxCF, 
    CRCxD0, CRCxD1, CRCxD2, CRCxD3, CRCxD4, CRCxD5, CRCxD6, CRCxD7, 
    CRCxD8, CRCxD9, CRCxDA, CRCxDB, CRCxDC, CRCxDD, CRCxDE, CRCxDF, 
    CRCxE0, CRCxE1, CRCxE2, CRCxE3, CRCxE4, CRCxE5, CRCxE6, CRCxE7, 
    CRCxE8, CRCxE9, CRCxEA, CRCxEB, CRCxEC, CRCxED, CRCxEE, CRCxEF, 
    CRCxF0, CRCxF1, CRCxF2, CRCxF3, CRCxF4, CRCxF5, CRCxF6, CRCxF7, 
    CRCxF8, CRCxF9, CRCxFA, CRCxFB, CRCxFC, CRCxFD, CRCxFE, CRCxFF
}; 

// ----------------------------------
//
// General Definitions
//
// ----------------------------------


/// MPipe2 Sync Word
/// ----------------
/// The beginning of each MPipe2 packet is FF55
#define OTC_MPIPE_SYNC_WORD         0xFF55
#define OTC_MPIPE_SYNC_BYTE_0       0xFF
#define OTC_MPIPE_SYNC_BYTE_1       0x55



/// ALP/NDEF TNF Field Values
/// -------------------------
/// Constants and an enum to define the TNF (Type Name Field).  TNF is a 3 bit
/// ID that goes in the same Message/Record byte as the NDEF Control Bits.
/// MPipe2 ignores this value, using OTC_MPIPE_TNF_EMPTY (0).

#define OTC_MPIPE_TNF_MASK               0x07
#define OTC_MPIPE_TNF_EMPTY              0x00
#define OTC_MPIPE_TNF_WELLKNOWN          0x01
#define OTC_MPIPE_TNF_MEDIATYPE          0x02
#define OTC_MPIPE_TNF_ABSOLUTEURI        0x03
#define OTC_MPIPE_TNF_EXTERNAL           0x04
#define OTC_MPIPE_TNF_UNKNOWN            0x05
#define OTC_MPIPE_TNF_UNCHANGED          0x06
#define OTC_MPIPE_TNF_RESERVED           0x07

/// ALP/NDEF header structure
typedef union  {
    struct {
        unsigned int typeName           :3;
        unsigned int idLength           :1;
        unsigned int shortRecord        :1;
        unsigned int chunk              :1;
        unsigned int msgEnd             :1;
        unsigned int msgStart           :1;
    };
    unsigned int flags;

} otc_mpipe_header_flags_t;







// ----------------------------------
//
// MPIPE Builder Specific
//
// ----------------------------------

/// MPIPE Header size
#define OTC_MPIPE_HEADER_SIZE       8

/// MPIPE ALP size
#define OTC_MPIPE_ALP_SIZE          4


/// MPIPE Builder Object Class
class otc_mpipe_builder {
public :
    
    otc_mpipe_builder(unsigned char bodylen);
    ~otc_mpipe_builder();

    void                                header(unsigned char id, unsigned char cmd, unsigned char seq);
    void                                body(unsigned char * data);
    void                                footer();
    int                                 len(void)	{return size;};
    unsigned char *                     start(void)	{return buffer;};

private :   

    unsigned char *                     outbuf;
    int                                 size;
    //otc_mpipe_superstate_t              superstate;
};





// ----------------------------------
//
// MPIPE Parser Specific
//
// ----------------------------------

typedef enum {
    OTC_MPIPE_SYNC_WORD_CHUNK_IMPLICIT   = 0,
    OTC_MPIPE_SYNC_WORD_CHUNK_CONTINUE   = 1,
    OTC_MPIPE_SYNC_WORD_CHUNK_LAST       = 2,
    OTC_MPIPE_SYNC_WORD_CHUNK_FIRST      = 5,
    OTC_MPIPE_SYNC_WORD_CHUNK_NO         = 6
} otc_mpipe_superstate_t;


/// Parser state
typedef enum {
    OTC_MPIPE_PARSER_STATE_SYNC          = 0,
    OTC_MPIPE_PARSER_STATE_SYNC2,
    OTC_MPIPE_PARSER_STATE_HEADER,
    OTC_MPIPE_PARSER_STATE_ALP,
    OTC_MPIPE_PARSER_STATE_DATA,
    OTC_MPIPE_PARSER_STATE_DONE          = 0x7f,
    OTC_MPIPE_PARSER_STATE_ERROR         = 0x80
} otc_mpipe_parser_state_t;


/// Parser buffer indexes
typedef enum {
    OTC_MPIPE_BUFFER_INDEX_FIRST        = 0,
    OTC_MPIPE_BUFFER_INDEX_CRC          = OTC_MPIPE_BUFFER_INDEX_FIRST,
    OTC_MPIPE_BUFFER_INDEX_HEADER,
    OTC_MPIPE_BUFFER_INDEX_ALP,
    OTC_MPIPE_BUFFER_INDEX_DATA,
    OTC_MPIPE_BUFFER_INDEX_QTY
} otc_mpipe_buffer_index_t;


/// Buffer class
typedef struct {
    unsigned char  size;
    unsigned char  offset;
    unsigned char  remain;
    unsigned char* payload;
} otc_mpipe_buffer_t;


/// MPIPE Parser Object Class
class otc_mpipe_parser {

public :
    otc_mpipe_parser();
    ~otc_mpipe_parser();
    bool                            parse(unsigned char* buffer, int toread);
    bool                            sync(unsigned char* buffer);

private :   
    otc_mpipe_parser_state_t        state;
    otc_mpipe_superstate_t          superstate;
    //otc_mpipe_header_flags_t        header;
    otc_mpipe_buffer_t              buffer[OTC_MPIPE_BUFFER_INDEX_QTY];
    //unsigned int                    currentBuffer;
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

