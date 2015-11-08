/*
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
#ifndef OPENDNP3_DECODERIMPL_H
#define OPENDNP3_DECODERIMPL_H

#include <openpal/container/RSlice.h>
#include <openpal/logging/Logger.h>

#include "opendnp3/link/LinkLayerParser.h"
#include "opendnp3/link/IFrameSink.h"

#include "opendnp3/transport/TransportRx.h"

namespace opendnp3
{
	class DecoderImpl;

	// stand-alone DNP3 decoder
	class DecoderImpl final : private IFrameSink
	{
	public:

		DecoderImpl(openpal::Logger logger);		

		void DecodeLPDU(const openpal::RSlice& data);
		void DecodeTPDU(const openpal::RSlice& data);
		void DecodeAPDU(const openpal::RSlice& data);		

	private:
		
		static bool IsResponse(const openpal::RSlice& data);

		/// --- Implement IFrameSink ---
		virtual bool OnFrame(const LinkHeaderFields& header, const openpal::RSlice& userdata) override;


		openpal::Logger logger;		
		LinkLayerParser link;
		TransportRx transportRx;
	};


}

#endif