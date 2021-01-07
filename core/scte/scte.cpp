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

enum scte_104_opid {
    opid_null,
    opid_splice,
    opid_splice_null
};
#define OPID_SPLICE L"SPLICE"
#define OPID_SPLICE_NULL L"SPLICE_NULL"

enum scte_104_splice_type {
    splice_type_null,
    start_normal,
    start_immediate,
    end_normal,
    end_immediate,
    cancel
};
#define SPLICE_START_NORMAL L"START_NORMAL"
#define SPLICE_START_IMMEDIATE L"START_IMMEDIATE"
#define SPLICE_END_NORMAL L"END_NORMAL"
#define SPLICE_END_IMMEDIATE L"END_IMMEDIATE"
#define SPLICE_CANCEL L"CANCEL"

struct scte_104::impl : boost::noncopyable
{
    enum scte_104_opid opid = opid_null;
    enum scte_104_splice_type splice_type = splice_type_null;
    uint32_t event_id = 0;
    uint16_t unique_program_id = 0;
    uint16_t pre_roll_time = 0;//milliseconds
    uint16_t break_duration = 0;
    uint8_t avail_num = 0;
    uint8_t avails_expected = 0;
    uint8_t auto_return_flag = 0;

    bool started = false;
    timer since_last_splice_cmd;
    timer since_first_frame;
    uint16_t next_remaining_mark;

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
        if (opid_s.empty())
            CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"SCTE without OPID Param "));
        else if (opid_s == OPID_SPLICE_NULL)
            opid = opid_splice_null;
        else if (opid_s == OPID_SPLICE)
            opid = opid_splice;
        else
            CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"SCTE wrong OPID Param "));
        if (opid == opid_splice)
        {
            auto splice_type_s = get_param(L"SPLICE_TYPE", parameters, L"");
            if (splice_type_s.empty())
                CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"SCTE no SPLICE_TYPE param"));
            else if (splice_type_s == SPLICE_START_NORMAL)
                splice_type = start_normal;
            else if (splice_type_s == SPLICE_START_IMMEDIATE)
                splice_type = start_immediate;
            else if (splice_type_s == SPLICE_END_NORMAL)
                splice_type = end_normal;
            else if (splice_type_s == SPLICE_END_IMMEDIATE)
                splice_type = end_immediate;
            else if (splice_type_s == SPLICE_CANCEL)
                splice_type = cancel;
            else
                CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"SCTE wrong SPLICE_TYPE Param "));
            if (splice_type == start_normal || splice_type == end_normal) {
                next_remaining_mark = pre_roll_time = get_param(L"PRE_ROLL_TIME", parameters, uint16_max);
            }
            if (splice_type == start_normal || splice_type == start_immediate) {
                break_duration = get_param(L"BREAK_DURATION", parameters, uint16_max);
                auto_return_flag = contains_param(L"AUTO_RETURN", parameters);
            }
        }
    }

    std::unique_ptr<std::vector<unsigned char>> get_data(bool null)
    {
        CASPAR_LOG(debug) << "Would've send splice data elapsed:" << since_last_splice_cmd.elapsed();
        next_remaining_mark -= 15000;
        if (next_remaining_mark < 45000 || next_remaining_mark > pre_roll_time)
        {
            opid = opid_splice_null;
        }
        since_last_splice_cmd.restart();
        return nullptr;
    }

    std::unique_ptr<std::vector<unsigned char>> tick()
    {
        if (!started)
        {
            since_first_frame.restart();
            auto data = get_data(opid = opid_splice_null);
            started = true;
            return data;
        }
        uint16_t remaining = pre_roll_time - (since_first_frame.elapsed() * 1000);
        if (opid == opid_splice && remaining <= next_remaining_mark)
        {
            return get_data(false);
        }
        if (since_last_splice_cmd.elapsed() >= 1) {
            return get_data(true);
        }
        return nullptr;
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
std::unique_ptr<std::vector<unsigned char>> scte_104::tick(){ return impl_->tick(); }

}}
