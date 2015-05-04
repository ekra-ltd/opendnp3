/**
 * Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
 * more contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright ownership.
 * Green Energy Corp licenses this file to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This project was forked on 01/01/2013 by Automatak, LLC and modifications
 * may have been made to this file. Automatak, LLC licenses these modifications
 * to you under the terms of the License.
 */
#ifndef OPENDNP3_WRITEHANDLER_H
#define OPENDNP3_WRITEHANDLER_H

#include "opendnp3/app/parsing/APDUHandlerBase.h"
#include "opendnp3/app/IINField.h"

#include "opendnp3/outstation/IOutstationApplication.h"

#include <openpal/logging/Logger.h>

namespace opendnp3
{

class WriteHandler : public APDUHandlerBase
{
public:

	WriteHandler(openpal::Logger& logger, IOutstationApplication& application, IINField* pWriteIIN_);
	
private:

	virtual IINField ProcessIIN(const HeaderRecord& record, uint16_t index, uint32_t count, const IINValue& value) override final;

	virtual IINField ProcessCount(const HeaderRecord& record, uint16_t pos, uint16_t total, const Group50Var1& time) override final;

	virtual IINField ProcessIndexPrefix(const HeaderRecord& record, const IterableBuffer<IndexedValue<TimeAndInterval, uint16_t>>& meas) override final;

	IOutstationApplication* pApplication;
	IINField* pWriteIIN;

	bool wroteTime;
	bool wroteIIN;
};

}



#endif

