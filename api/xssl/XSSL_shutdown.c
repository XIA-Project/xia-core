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
** @file XSSL_shutdown.c
** @brief implements XSSL_shutdown()
*/

#include "xssl.h"


/**
* @brief Shut down an XSSL connection.
* @warning This function is currently not implemented.
*
* @param ctx
*
* @return 1 on successful bidirectional shutdown
* @return 0 on successfully sending shutdown notification; call XSSL_shutdown()
*	again to confirm the other side has closed as well (look for ret val of 1)
* @return -1 on error
*/
int XSSL_shutdown(XSSL *xssl) {
	(void)xssl;
	WARN("XSSL_shutdown not implemented\n");
	return 1;
}
