/*
** Copyright 2013 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
** @file XSSL_write.c
** @brief implements XSSL_write()
*/

#include "xssl.h"


/**
* @brief Write data to an XSSL connection.
*
* @param xssl
* @param buf Data to send.
* @param num Number of bytes to send.
*
* @return Number of bytes written to the connection on success.
* @return 0 or <0 on failure
*/
int XSSL_write(XSSL *xssl, const void *buf, int num) {

}
