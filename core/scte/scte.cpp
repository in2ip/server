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

#include "scte.h"
#include "common/param.h"
#include "common/except.h"
#include <cstdint>
#include <vector>
namespace caspar { namespace core {

void split_param(const std::wstring& in, std::list<std::wstring> &tokens)
{
    std::wstring currentparam;
    for (size_t index; index < in.length(); index++)
    {
        if (in[index] == L'=' || in[index] == L',' || in[index] == L' ')
        {
            if (!currentparam.empty())
            {
                tokens.push_back(currentparam);
                currentparam.clear();
            }
            continue;
        }

        currentparam += in[index];
    }
    if (!currentparam.empty())
    {
        tokens.push_back(currentparam);
        currentparam.clear();
    }
}

struct scte_104::impl : boost::noncopyable
{
    enum scte_104_opid opid = opid_null;
    enum scte_104_splice_type splice_type = splice_type_null;
    uint32_t event_id = 0;
    uint16_t unique_program_id = 0;
    uint16_t pre_roll_time = 0;
    uint16_t break_duration = 0;
    uint8_t avail_num = 0;
    uint8_t avails_expected = 0;
    uint8_t auto_return_flag = 0;
    impl(const std::wstring &scte_string)
    {
        CASPAR_LOG(info) << scte_string;
        constexpr auto uint32_max = std::numeric_limits<uint32_t>::max();
        constexpr auto uint16_max = std::numeric_limits<uint16_t>::max();
        constexpr auto uint8_max = 255U;
        std::list<std::wstring> tokens;
        split_param(scte_string, tokens);
        std::vector<std::wstring> parameters(tokens.begin(), tokens.end());
        auto opid_s				= get_param(L"OPID", 			parameters, L"");
        CASPAR_LOG(info) << opid_s;
        if (opid_s.empty())
            CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"SCTE without OPID Param ") << nested_exception(std::current_exception()));
        if (opid_s.find(L"SPLICE_NULL", 0) == 0) {
            opid = opid_splice_null;
        } else if (opid_s.find(L"SPLICE", 0) == 0) {
            auto splice_type_s		= get_param(L"SPLICE_TYPE", 	parameters, L"");

        } else {
            CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"SCTE wrong OPID Param ") << nested_exception(std::current_exception()));
        }
        
    }
};

scte_104::scte_104(const std::wstring &scte_string) :
    impl_(new impl(scte_string)){}
scte_104::~scte_104(){}
scte_104::scte_104(scte_104&& other) : impl_(std::move(other.impl_)){}
scte_104& scte_104::operator=(scte_104&& other)
{
	impl_ = std::move(other.impl_);
	return *this;
}

}}
