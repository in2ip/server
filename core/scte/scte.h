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
    
    class scte_104 final
    {
        scte_104(const scte_104&);
        scte_104& operator=(const scte_104&);

    public:
        explicit scte_104(const std::wstring& scte_string);
        ~scte_104();
        scte_104(scte_104&& other);
        scte_104& operator=(scte_104&& other);
        std::unique_ptr<std::vector<unsigned char>> tick();

    private:
	    struct impl;
	    spl::unique_ptr<impl> impl_;
    };
}}
