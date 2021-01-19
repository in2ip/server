/*
* Copyright (c) 2020 in2ip B.V.
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
#pragma once

#include "messages.h"
#include "core/ancillary/bitstream.h"
#include "core/StdAfx.h"

namespace caspar { namespace core { namespace ancillary { namespace scte104 {

    class SpliceNull : public SCTE104Msg
    {
        public:
            void appendData(std::vector<uint8_t> &buf) 
            {
                Bitstream bs = Bitstream(buf);
                bs.write_bytes_msb(opid_splice_null, 2);
                bs.write_bytes_msb(0 , 2);
            }
            scte_104_opid getOpID() { return opid_splice_null; }
    };
}}}}
