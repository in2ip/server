/*
* Copyright 2013 Sveriges Television AB http://casparcg.com/
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
* Author: Robert Nagy, ronag89@gmail.com
*/

#pragma once

#include <common/memory.h>

#include <boost/noncopyable.hpp>

#include <cstdint>

struct AVPacket;
struct AVFrame;
struct AVFormatContext;

namespace caspar { namespace ffmpeg {

class subtitle_decoder : boost::noncopyable
{
public:
	explicit subtitle_decoder(int stream_index, const spl::shared_ptr<AVFormatContext>& context);

	bool ready() const;
	void push(const std::shared_ptr<AVPacket>& packet);
	std::tuple<int64_t, int, std::shared_ptr<AVSubtitle>> poll();
	std::wstring print() const;
private:
	struct implementation;
	spl::shared_ptr<implementation> impl_;
};

}}
