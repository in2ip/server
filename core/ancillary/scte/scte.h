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

#include "../ancillary.h"
#include "messages/messages.h"

namespace caspar { namespace core { namespace ancillary {

class SCTE104AncData : public AncillaryData
{
    public:
        std::vector<uint8_t> getData();
        void getVancID(uint8_t& did, uint8_t& sdid) { did = 0x41; sdid = 0x07; }
        ancillary_data_type getType() { return ancillary_data_type_scte_104; }
        void addMsg(std::shared_ptr<scte104::SCTE104Msg> msg);
    private:
        std::list<std::shared_ptr<scte104::SCTE104Msg>> messages;
};

}}}
