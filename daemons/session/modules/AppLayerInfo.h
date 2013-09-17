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

#ifndef APP_LAYER_INFO_H
#define APP_LAYER_INFO_H

#include "session.h"
#include "StackInfo.h"

class AppLayerInfo : public StackInfo {
	public:
		AppLayerInfo(uint32_t attributes);
		bool checkAttribute(SessionAttribute attribute);

	private:
		uint32_t _attributes;
};


#endif /* APP_LAYER_INFO_H */
