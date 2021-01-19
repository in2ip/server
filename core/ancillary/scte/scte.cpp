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

#include "scte.h"
#include "core/ancillary/bitstream.h"
#include "messages/splicenull.h"
#include "messages/splicerequest.h"

namespace caspar { namespace core { namespace ancillary {
    void write_scte_104_hdr(std::vector<std::uint8_t>&buf, size_t size)
    {
        Bitstream bs = Bitstream(buf);

        bs.write_byte(0x08);//2010 Payload Descriptor (not part of SCTE-104, but here to ease conversion to VANC)

        bs.write_bytes_msb(0xFFFF, 2);//reserved
        bs.write_bytes_msb(0, 2);//messageSize
        bs.write_byte(0);//protocol_version
        bs.write_byte(0);//AS_index
        bs.write_byte(0);//message_number
        bs.write_bytes_msb(0, 2);//DPI_PID_index
        bs.write_byte(0);//SCE35_protocol_version
        bs.write_byte(0);//timestamp.type
        bs.write_byte(size);//num_ops
    }

    void set_scte_104_length(std::vector<std::uint8_t>&buf)
    {
        std::size_t size = buf.size() -1;// minus one because of inclusion of 2010 PD
        uint8_t *data = buf.data();
        data[3] = size >> 8;
        data[4] = size & 0xff;
    }

    std::vector<uint8_t> SCTE104AncData::getData() {
        std::vector<uint8_t> out;
        write_scte_104_hdr(out, messages.size());
        for (auto msg : messages)
        {
            msg.get()->appendData(out);
        }
        set_scte_104_length(out);
        return out;
    }

    void SCTE104AncData::addMsg(std::shared_ptr<scte104::SCTE104Msg> msg)
    {
        messages.push_back(std::move(msg));
    }

}}}