/*
* Copyright (c) 2021 in2ip B.V.
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Gijs Peskens <gijs@in2ip.nl>
*/

#include "cea708.h"
namespace caspar { namespace core { namespace ancillary {

    enum cea708_pkt_type {
        NTSC_CC_FIELD_1 = 0,
        NTSC_CC_FIELD_2 = 1,
        DTVCC_PACKET_DATA = 2,
        DTVCC_PACKET_START = 3
    };

    class CEA708PKT
    {
        uint8_t data[3] = { 0 };
        bool cc_valid;
        cea708_pkt_type type;
        public:
            CEA708PKT(uint8_t pkt[])
            {
                memcpy(data, pkt, 3);
                cc_valid = (data[0] >> 2) & 1UL;
                type = static_cast<cea708_pkt_type>(data[0] & 0x03);
            }
    };

    struct CEA708::impl : boost::noncopyable
    {
        std::list<CEA708PKT> pkts;
        impl(uint8_t data[], size_t size, cea708_format type)
        {
            if (type == raw_pkts)
            {
                if ((size % 3) != 0)
                    CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"input data not mod 3"));

                for (int i = 0; i < size;)
                {
                    pkts.emplace_back(CEA708PKT(&data[i]));
                    i += 3;
                }
            } else {
                CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"CEA input type not implemented"));
            }
        
        }

        impl(const impl& other)
        {
            //
        }

        std::vector<uint8_t> getData()const
        {
            return std::vector<uint8_t>();
        }
    };

    CEA708::CEA708(uint8_t* data, size_t size, cea708_format type) : impl_(new impl(data, size, type)){}
    CEA708::~CEA708(){}
    std::vector<uint8_t> CEA708::getData()const { return impl_->getData(); }
}}}
