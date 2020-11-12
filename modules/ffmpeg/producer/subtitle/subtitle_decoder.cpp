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

#include "../../StdAfx.h"

#include "subtitle_decoder.h"

#include "../util/util.h"
#include "../../ffmpeg_error.h"

#include <queue>
#include <tuple>
#include <cstdint>

#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4244)
#endif
extern "C"
{
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libswresample/swresample.h>
}
#if defined(_MSC_VER)
#pragma warning (pop)
#endif

namespace caspar { namespace ffmpeg {

struct subtitle_decoder::implementation : boost::noncopyable
{
	int										index_;
	const spl::shared_ptr<AVCodecContext>	codec_context_;

	cache_aligned_vector<int32_t>			buffer_;

	std::queue<spl::shared_ptr<AVPacket>>	packets_;

public:
	explicit implementation(int stream_index, const spl::shared_ptr<AVFormatContext>& context)
		: index_(stream_index)
		, codec_context_(open_codec(*context, AVMEDIA_TYPE_SUBTITLE, index_, false))
	{
		codec_context_->refcounted_frames = 1;
	}

	void push(const std::shared_ptr<AVPacket>& packet)
	{
		if(!packet)
			return;

		if(packet->stream_index == index_ || packet->data == nullptr)
			packets_.push(spl::make_shared_ptr(packet));
	}

	std::tuple<int64_t, int, std::shared_ptr<AVSubtitle>> poll()
	{
		if(packets_.empty())
			return std::make_tuple(0, 0, nullptr);
		
		auto packet = packets_.front();

		if(packet->data == nullptr)
		{
			packets_.pop();
			avcodec_flush_buffers(codec_context_.get());
			return std::make_tuple(0, 0, nullptr);
		}

		auto subtitle = decode(*packet);

		if(packet->size == 0)
			packets_.pop();
		if (subtitle == nullptr)
			return std::make_tuple(0, 0, nullptr);
		AVSubtitle * sub = subtitle.get();
		sub->pts;
		return std::make_tuple((int64_t)((sub->pts + (float)sub->start_display_time/1000)), index_, subtitle);
	}

	std::shared_ptr<AVSubtitle> decode(AVPacket& pkt)
	{
		auto decoded_frame = create_subtitle();

		int got_frame = 0;
		auto len = THROW_ON_ERROR2(avcodec_decode_subtitle2(codec_context_.get(), decoded_frame.get(), &got_frame, &pkt), "[subtitle_decoder]");

		if (len == 0)
		{
			pkt.size = 0;
			return nullptr;
		}

		pkt.data += len;
		pkt.size -= len;

		if (!got_frame)
			return nullptr;

		return decoded_frame;
	}

	bool ready() const
	{
		return packets_.size() > 1;
	}

	std::wstring print() const
	{
		return L"[subtitle_decoder] " + u16(codec_context_->codec->long_name);
	}
};

subtitle_decoder::subtitle_decoder(int stream_index, const spl::shared_ptr<AVFormatContext>& context) : impl_(new implementation(stream_index, context)){}
void subtitle_decoder::push(const std::shared_ptr<AVPacket>& packet){impl_->push(packet);}
bool subtitle_decoder::ready() const{return impl_->ready();}
std::tuple<int64_t, int, std::shared_ptr<AVSubtitle>> subtitle_decoder::poll() { return impl_->poll(); }
std::wstring subtitle_decoder::print() const{return impl_->print();}

}}
