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

#include "core/StdAfx.h"

namespace caspar { namespace core { namespace ancillary { namespace scte104 {

    enum scte_104_opid {
        opid_null = 0xFFFF,//reserved
        opid_splice = 0x0101,
        opid_splice_null = 0x0102
    };

    class SCTE104Msg 
    {
        public:
            SCTE104Msg(){};
            virtual ~SCTE104Msg(){};
            virtual void appendData(std::vector<uint8_t>& buf) = 0;
            virtual scte_104_opid getOpID() = 0;
    };

}}}}