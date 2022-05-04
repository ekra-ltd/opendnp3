/*
 * Copyright 2013-2022 Step Function I/O, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Step Function I/O
 * LLC (https://stepfunc.io) under one or more contributor license agreements. 
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Step Function I/O LLC license
 * this file to you under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may obtain
 * a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef OPENDNP3CLR_MASTER_SESSION_ADAPTER_H
#define	OPENDNP3CLR_MASTER_SESSION_ADAPTER_H

#include <opendnp3/master/IMasterSession.h>

#include "MasterOperationsAdapter.h"

#include <memory>

using namespace Automatak::DNP3::Interface;

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Threading::Tasks;

namespace Automatak
{
    namespace DNP3
    {
        namespace Adapter
        {

            private ref class MasterSessionAdapter sealed : IMasterSession, MasterOperationsAdapter
            {
            public:

                MasterSessionAdapter(std::shared_ptr<opendnp3::IMasterSession> proxy);
                
                MasterSessionAdapter::~MasterSessionAdapter()
                {
                    this->!MasterSessionAdapter();
                }

                MasterSessionAdapter::!MasterSessionAdapter()
                {
                    delete proxy;
                }

                /// --- implement IMasterSession ----
                virtual void BeginShutdown();

                virtual Interface::IStackStatistics^ GetStackStatistics();

            private:

                std::shared_ptr<opendnp3::IMasterSession>* proxy;
            };

        }
    }
}

#endif
