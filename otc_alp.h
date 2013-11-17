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
/// @file           otc_alp.h
/// @brief          Definitions and structures of OT ALP layer
//
/// =========================================================================

#ifndef __OTC_ALP_H__
#define __OTC_ALP_H__


// ---------------------------------------- //
//                                          //
//          ALP ID TYPES                    //
//                                          //
// ---------------------------------------- //

typedef enum {
    OTC_ALP_ID_NULL                 = 0x00,
    OTC_ALP_ID_FILE_DATA            = 0x01,
    OTC_ALP_ID_SENSOR               = 0x02,
    OTC_ALP_ID_DASHFORTH            = 0x03,
    OTC_ALP_ID_LOG                  = 0x04,
    OTC_ALP_ID_SESSION              = 0x80,     ///@todo Session, System, M2QP are going away
    OTC_ALP_ID_SYSTEM               = 0x81,
    OTC_ALP_ID_M2QP                 = 0x82,
} otc_alp_id_t;


// ---------------------------------------- //
//                                          //
//          ALP CMD TYPES                   //
//                                          //
// ---------------------------------------- //

typedef enum {
    OTC_ALP_CMD_FILE_GFB            = 0x10,
    OTC_ALP_CMD_FILE_ISFSB          = 0x20,
    OTC_ALP_CMD_FILE_ISFB           = 0x30,
} otc_alp_cmd_file_block_t;


typedef enum {
    OTC_ALP_CMD_FILE_RD_PERM        = 0x0,
    OTC_ALP_CMD_FILE_RTN_PERM       = 0x1,
    OTC_ALP_CMD_FILE_WR_PERM        = 0x3,
    OTC_ALP_CMD_FILE_RD_DATA        = 0x4,
    OTC_ALP_CMD_FILE_RTN_DATA       = 0x5,
    OTC_ALP_CMD_FILE_WR_DATA        = 0x7,
    OTC_ALP_CMD_FILE_RD_HEADER      = 0x8,
    OTC_ALP_CMD_FILE_RTN_HEADER     = 0x9,
    OTC_ALP_CMD_FILE_DELETE         = 0xa,
    OTC_ALP_CMD_FILE_CREATE         = 0xb,
    OTC_ALP_CMD_FILE_RD_ALL         = 0xc,
    OTC_ALP_CMD_FILE_RTN_ALL        = 0xd,
    OTC_ALP_CMD_FILE_RESTORE        = 0xe,
    OTC_ALP_CMD_FILE_RTN_ERROR      = 0xf
} otc_alp_cmd_file_exec_t;


typedef enum {
    OTC_ALP_CMD_LOG_RAW             = 0x00,
    OTC_ALP_CMD_LOG_UTF8            = 0x01,
    OTC_ALP_CMD_LOG_UTF16           = 0x02,
    OTC_ALP_CMD_LOG_UTF8HEX         = 0x03,
    OTC_ALP_CMD_LOG_RAWMSG          = 0x04,
    OTC_ALP_CMD_LOG_UTF8MSG         = 0x05,
    OTC_ALP_CMD_LOG_UTF16MSG        = 0x06,
    OTC_ALP_CMD_LOG_UTF8HEXMSG      = 0x07,
    OTC_ALP_CMD_LOG_ECHO            = 0x10
} otc_alp_cmd_log_t;


// ---------------------------------------- //
//                                          //
//          ALP RESP TYPES                  //
//                                          //
// ---------------------------------------- //

typedef enum {
    OTC_ALP_RESP_NO                 = 0x00,
    OTC_ALP_RESP_ECHO               = 0x40,
    OTC_ALP_RESP_REQ                = 0x80,
} otc_alp_resp_t; 


#endif // __OTC_ALP_H__
