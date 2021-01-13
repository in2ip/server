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

namespace caspar { namespace core {

enum ancillary_type {
    anc_type_scte_104
};

class ancillary final
{
    public:
        std::list<std::pair<ancillary_type, std::vector<uint32_t>>> ancillary_data;

        ancillary()
        {
        }

        ancillary(ancillary && other) noexcept
        {
            *this = std::move(other);
        }

        ancillary(const ancillary & other) noexcept
        {
            ancillary_data = other.ancillary_data;
        }

        void add(ancillary_type type, std::vector<uint32_t>& data)
        {
            auto pair = std::make_pair(type, data);
            ancillary_data.push_back(std::move(pair));
        }

        ancillary& operator=(ancillary&& other) noexcept
        {
            if (this != &other)
            {
                for (auto & item : other.ancillary_data)
                {
                    ancillary_data.push_back(std::move(item));
                }

                other.ancillary_data.clear();
            }
            return *this;
        }
};
}}

