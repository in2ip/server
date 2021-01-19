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

#include "messages.h"
#include "core/ancillary/bitstream.h"
#include "core/StdAfx.h"

namespace caspar { namespace core { namespace ancillary { namespace scte104 {
    
    enum scte_104_splice_type {
        splice_type_null = 0, //reserved
        start_normal = 1,
        start_immediate = 2,
        end_normal = 3,
        end_immediate = 4,
        cancel = 5
    };

    class SpliceRequest : public SCTE104Msg
    {
        enum scte_104_splice_type splice_type_ = splice_type_null;
        uint32_t event_id_ = 0;
        uint16_t unique_program_id_ = 0;
        uint16_t pre_roll_time_ = 0;//milliseconds
        uint16_t break_duration_ = 0;
        uint8_t avail_num_ = 0;
        uint8_t avails_expected_ = 0;
        uint8_t auto_return_flag_ = 0;
        public:
            SpliceRequest(scte_104_splice_type type,
                       uint32_t event_id,
                       uint16_t unique_program_id,
                       uint16_t pre_roll_time,
                       uint16_t break_duration,
                       uint8_t avail_num,
                       uint8_t avails_expected,
                       uint8_t auto_return_flag)
                : splice_type_(type)
                , event_id_(event_id)
                , unique_program_id_(unique_program_id)
                , break_duration_(break_duration)
                , pre_roll_time_(pre_roll_time)
                , avail_num_(avail_num)
                , avails_expected_(avails_expected)
                , auto_return_flag_(auto_return_flag) {}
            void appendData(std::vector<uint8_t> &buf) 
            {
                Bitstream bs = Bitstream(buf);
                bs.write_bytes_msb(opid_splice, 2);//opID
                bs.write_bytes_msb(14, 2);//data_length

                bs.write_byte(splice_type_);//splice_insert_type
                bs.write_bytes_msb(event_id_, 4);//splice_event_id
                bs.write_bytes_msb(unique_program_id_, 2);//unique_program_id
                bs.write_bytes_msb(pre_roll_time_, 2);//pre_roll_time
                bs.write_bytes_msb(break_duration_, 2);//break_duration
                bs.write_byte(avail_num_);//avail_num
                bs.write_byte(avails_expected_);//avails_expected
                bs.write_byte(auto_return_flag_);//auto_return_flag
            }
            scte_104_opid getOpID() { return opid_splice; }
    };
}}}}
