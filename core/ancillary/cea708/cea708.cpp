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
#include "core/ancillary/bitstream.h"

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

            cea708_pkt_type getType()const
            {
                return type;
            }

            bool is_valid_cc()const
            {
                return cc_valid;
            }

            uint64_t asUint64()
            {
                return (data[0] << 16 | data[1] << 8 | data[0]);
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

        void appendBack(impl& other)
        {
            //NTSC Field 1 & 2 need to be before DVTCC data, so we look for the first DVTCC pkt and 
            //insert NTSC pkts before that, all other packets we append to the back
            std::list<CEA708PKT>::iterator i;
            bool found = false;
            for (i = pkts.begin(); i != pkts.end(); ++i)
            {
                if (i->getType() != NTSC_CC_FIELD_1 && i->getType() != NTSC_CC_FIELD_2)
                {
                    found = true;
                    break;
                }
            }
            for (auto& pkt: other.pkts)
            {
                if (found && (pkt.getType() == NTSC_CC_FIELD_1 || pkt.getType() == NTSC_CC_FIELD_2))
                {
                    pkts.insert(i, std::move(pkt));
                } else {
                    pkts.push_back(std::move(pkt));
                }
            }
        }

        impl(const impl& other) : pkts(other.pkts)
        {
            //
        }

        std::vector<uint8_t> getData()const
        {
            std::vector<uint8_t> out;
            out.reserve(255);
            Bitstream bs =Bitstream(out);
            bs.write_bytes_msb(0x9669, 2);//cdp_id
            bs.write_byte(0);//cdp_data_count, set it later
            bs.write_bits(0, 4);//cdp_framing_rate, set to 0 for now
            bs.write_bits(0xF,4);//cdp_reserved
            bs.write_bit(0);//cdp_timecode_added
            bs.write_bit(0);//cdp_data_block_added
            bs.write_bit(0);//cdp_service_info_added
            bs.write_bit(0);//cdp_service_info_start
            bs.write_bit(0);//cdp_service_info_changed
            bs.write_bit(0);//cdp_service_info_end
            bs.write_bit(1);//cdp_contains_captions
            bs.write_bit(0);//cdp_reserved
            bs.write_bytes_msb(0, 2);//cdp_counter sequence counter, set to 0 for now
            bs.write_byte(0x72);//ccdata_id
            bs.write_bits(0x07, 3);//marker bits
            bs.write_bits(pkts.size(), 5);
            for (auto pkt: pkts)
            {
                bs.write_bytes_msb(pkt.asUint64(), 3);
            }
            bs.write_byte(0x74);//cbp_footer_id
            bs.write_bytes_msb(0, 2);//cdp_counter sequence counter, set to 0 for now
            bs.write_byte(0);//checksum
            out.data()[2] = out.size();
            uint8_t checksum = 0;
            for (auto val : out)
            {
                checksum += val;
            }
            out.back() = ~checksum +1;
            return out;
        }
    };

    CEA708::CEA708(uint8_t* data, size_t size, cea708_format type) : impl_(new impl(data, size, type)){}
    CEA708::~CEA708(){}
    std::vector<uint8_t> CEA708::getData()const { return impl_->getData(); }
    void CEA708::appendBack(CEA708& other) { impl_->appendBack(*other.impl_); }
}}}
