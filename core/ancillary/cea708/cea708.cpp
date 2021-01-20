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
#include "hexdump.h"
#include <iostream>
namespace caspar { namespace core { namespace ancillary {

    struct CEA708::impl : boost::noncopyable
    {
        impl(uint8_t* data, size_t size, cea708_format type)
        {
           std::cout << Hexdump(data, size) << std::endl;
        }

        impl(const impl& other)
        {
            //
        }

        std::vector<uint8_t> getData()const
        {
            return std::vector<uint8_t>();
        }
    };

    CEA708::CEA708(uint8_t* data, size_t size, cea708_format type) : impl_(new impl(data, size, type)){}
    CEA708::~CEA708(){}
    std::vector<uint8_t> CEA708::getData()const { return impl_->getData(); }
}}}