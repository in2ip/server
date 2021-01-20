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
#include "../StdAfx.h"

namespace caspar { namespace core { namespace ancillary {

enum ancillary_data_type {
    ancillary_data_none = 0,
    ancillary_data_type_scte_104 = 1,
    ancillary_data_type_cea708 = 2
};

class AncillaryData 
{
    public:
        AncillaryData(){};
        virtual ~AncillaryData(){};
        virtual std::vector<uint8_t> getData()const = 0;
        virtual ancillary_data_type getType()const = 0;
        virtual void getVancID(uint8_t& did, uint8_t& sdid)const = 0;
};

class AncillaryContainer final
{
    public:
        AncillaryContainer();
        ~AncillaryContainer();
        AncillaryContainer(const AncillaryContainer& other);
        AncillaryContainer& operator=(const AncillaryContainer& other);
        AncillaryContainer(AncillaryContainer&& other);
        AncillaryContainer& operator=(AncillaryContainer&& other);

        void addData(std::shared_ptr<AncillaryData> data);

        //Concats all ancillary data into v210 anc lines
        //exclude: OR ancillary_data_types to exclude
        std::list<std::vector<uint32_t>> getAncillaryAsLines(uint32_t width, ancillary_data_type exclude = ancillary_data_none) const;

        void clear();
        
        void appendFrom(AncillaryContainer& other);

        private:
            struct impl;
            spl::unique_ptr<impl> impl_;
};

}}}
