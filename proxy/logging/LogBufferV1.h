/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */



#ifndef LOG_BUFFER_V1_H
#define LOG_BUFFER_V1_H

struct LogEntryHeaderV1
{
  unsigned timestamp;
  unsigned entry_len;
};

struct LogBufferHeaderV1
{
  unsigned cookie;              // so we can find it on disk
  unsigned version;             // in case we want to change it later
  unsigned format_type;         // SQUID_LOG, COMMON_LOG, ...
  unsigned format_id;           // unique int for CUSTOM_LOG types
  unsigned transposed;          // is the data transposed?
  unsigned byte_count;          // acutal # of bytes for the segment
  unsigned entry_count;         // actual number of entries stored
  unsigned low_timestamp;       // lowest timestamp value of entries
  unsigned high_timestamp;      // highest timestamp value of entries
  unsigned data_offset;         // offset to start of data section
  char symbol_str[256];         // list of field symbols
  char printf_str[512];         // printf string
};

#endif
