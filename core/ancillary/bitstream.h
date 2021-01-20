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

#include <cstdint>
#include <vector>

#include <assert.h>

class Bitstream 
{
public:
    Bitstream(std::vector<std::uint8_t>& buf_) : buf(buf_){}
    ~Bitstream()
    {
        write_finalize();
    }

    void inline write_bit(uint_fast8_t bit)
    {
        bit &= 1UL;
        if (scratch_used < 8)
        {
            scratch <<= 1;
            scratch |= bit;
            scratch_used++;
        }

        if (scratch_used == 8)
        {
            buf.push_back(scratch);
            scratch_used = 0;
        }
    }
    void inline write_bits(uint64_t bits, uint32_t bitcount)
    {
        for (int64_t i = (bitcount -1); i >= 0; i--)
        {
            write_bit(bits >> i);
        }
    }

    void inline write_byte(uint8_t byte)
    {
        if (scratch_used == 0)
        {
            buf.push_back(byte & 0xff);
        }
        else
        {
            write_bits(byte, 8);
        }
    }

    void inline write_bytes_msb(uint64_t bytes, size_t count)
    {
        assert(count <= 8);
        if (scratch_used == 0)
        {
            size_t shift = (count -1) * 8;
            while (shift)
            {
                write_byte(bytes >> shift);
                shift -= 8;
            }
            write_byte(bytes);
        }
        else {
            write_bits(bytes, count * 8);
        }
    }

    void inline write_finalize()
    {
        while (scratch_used > 0)
        {
            write_bit(0);
        }
    }

    private:
        std::vector<std::uint8_t>& buf;
        uint8_t scratch = 0;
        std::size_t scratch_used  = 0;
};

