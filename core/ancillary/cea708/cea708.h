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

#pragma once

#include "../ancillary.h"

namespace caspar { namespace core { namespace ancillary {

    enum cea708_format {
        raw_pkts,
        atsc53,
        cdp
    };

    class CEA708 : public AncillaryData
    {
        public:
            CEA708(uint8_t* data, size_t size, cea708_format type);
            ~CEA708();
            std::vector<uint8_t> getData()const;
            void getVancID(uint8_t& did, uint8_t& sdid)const { did = 0x61; sdid = 0x01; }
            ancillary_data_type getType()const { return ancillary_data_type_cea708; }
        private:
            struct impl;
            spl::unique_ptr<impl> impl_;
    };

}}}
