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
#include "../StdAfx.h"

namespace caspar { namespace core { namespace ancillary {

enum ancillary_data_type {
    ancillary_data_none = 0,
    ancillary_data_type_scte_104 = 1
};

class AncillaryData 
{
    public:
        AncillaryData(){};
        virtual ~AncillaryData(){};
        virtual std::vector<uint8_t> getData() = 0;
        virtual ancillary_data_type getType() = 0;
        //DID, SDID
        virtual void getVancID(uint8_t& did, uint8_t& sdid) = 0;
};

class AncillaryContainer final
{
    std::vector<std::shared_ptr<AncillaryData>> ancillary_data_container;
    public:
        AncillaryContainer()
        {
        }

        AncillaryContainer(AncillaryContainer && other) noexcept
        {
            *this = std::move(other);
        }

        AncillaryContainer(const AncillaryContainer & other) noexcept
        {
            ancillary_data_container = other.ancillary_data_container;
        }

        void addData(std::shared_ptr<AncillaryData> data)
        {
            ancillary_data_container.push_back(data);
        }

        //Concats all ancillary data into v210 anc lines
        //exclude: OR ancillary_data_types to exclude
        std::list<std::vector<uint32_t>> getAncillaryAsLines(uint32_t width, ancillary_data_type exclude = ancillary_data_none) const;

        AncillaryContainer& operator=(AncillaryContainer&& other) noexcept
        {
            if (this != &other)
            {
                for (auto & item : other.ancillary_data_container)
                {
                    ancillary_data_container.push_back(std::move(item));
                }
                other.clear();
            }
            return *this;
        }

        void clear()
        {
            ancillary_data_container.clear();
        }
};

}}}
