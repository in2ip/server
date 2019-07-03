/*
 * Copyright 2018
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
 * Author: Krzysztof Zegzula, zegzulakrzysztof@gmail.com
 * based on work of Robert Nagy, ronag89@gmail.com
 */

#include "../StdAfx.h"

#include "newtek_ndi_consumer.h"

#include <core/consumer/frame_consumer.h>
#include <core/frame/audio_channel_layout.h>
#include <core/frame/frame.h>
#include <core/mixer/audio/audio_util.h>
#include <core/video_format.h>
#include <core/monitor/monitor.h>


#include <common/assert.h>
#include <common/diagnostics/graph.h>
#include <common/future.h>
#include <common/param.h>
#include <common/timer.h>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

#include "../util/ndi.h"

namespace caspar { namespace newtek {

struct newtek_ndi_consumer : public core::frame_consumer
{
    static std::atomic<int> instances_;
    const int               instance_no_;
    const std::wstring      name_;
    const bool              allow_fields_;

    core::monitor::subject               monitor_subject_;
    core::video_format_desc              format_desc_;
    core::audio_channel_layout           channel_layout_ = core::audio_channel_layout::invalid();
    core::audio_channel_layout           out_channel_layout_ = core::audio_channel_layout::invalid();
    std::unique_ptr<core::audio_channel_remapper>	channel_remapper_;

    int                                  channel_index_;
    NDIlib_v3*                           ndi_lib_;
    std::shared_ptr<uint8_t>             field_data_;
    spl::shared_ptr<diagnostics::graph>  graph_;
    executor                             executor_;
    caspar::timer                        tick_timer_;
    caspar::timer                        frame_timer_;
    int                                  frame_no_;

    std::unique_ptr<NDIlib_send_instance_t, std::function<void(NDIlib_send_instance_t*)>> ndi_send_instance_;

  public:
    newtek_ndi_consumer(std::wstring name, bool allow_fields, const core::audio_channel_layout& out_channel_layout)
        : name_(!name.empty() ? name : default_ndi_name())
        , instance_no_(instances_++)
        , frame_no_(0)
        , allow_fields_(allow_fields)
        , channel_index_(0)
        , out_channel_layout_(out_channel_layout)
        , executor_(print())
    {
        ndi_lib_ = ndi::load_library();
        graph_->set_text(print());
        graph_->set_color("frame-time", diagnostics::color(0.5f, 1.0f, 0.2f));
        graph_->set_color("tick-time", diagnostics::color(0.0f, 0.6f, 0.9f));
        graph_->set_color("dropped-frame", diagnostics::color(0.3f, 0.6f, 0.3f));
        diagnostics::register_graph(graph_);
    }

    ~newtek_ndi_consumer() {}

    // frame_consumer

    void initialize(const core::video_format_desc& format_desc, const core::audio_channel_layout& channel_layout, int channel_index) override
    {
        format_desc_   = format_desc;
        channel_index_ = channel_index;
        channel_layout_ = channel_layout;
        out_channel_layout_ = get_adjusted_layout(channel_layout_);

        channel_remapper_.reset(new core::audio_channel_remapper(channel_layout_, out_channel_layout_));

        NDIlib_send_create_t NDI_send_create_desc;

        auto tmp_name                   = u8(name_);
        NDI_send_create_desc.p_ndi_name = tmp_name.c_str();
        NDI_send_create_desc.clock_audio = false;
        NDI_send_create_desc.clock_video = true;

        ndi_send_instance_ = {new NDIlib_send_instance_t(ndi_lib_->NDIlib_send_create(&NDI_send_create_desc)),
                              [this](auto p) { this->ndi_lib_->NDIlib_send_destroy(*p); }};

        graph_->set_text(print());
        // CASPAR_VERIFY(ndi_send_instance_);
    }

    std::future<bool> schedule_send(core::const_frame frame)
    {
        return executor_.begin_invoke([=]() -> bool
        {
            graph_->set_value("ndi-connections", ndi_lib_->NDIlib_send_get_no_connections(*ndi_send_instance_, 0));

            graph_->set_value("tick-time", tick_timer_.elapsed() * format_desc_.fps * 0.5);
            tick_timer_.restart();
            frame_timer_.restart();

            // AUDIO
            auto audio_buffer = core::audio_32_to_16(frame.audio_data());

            NDIlib_audio_frame_interleaved_16s_t audio_frame = { 0 };
            audio_frame.reference_level = 0;
            audio_frame.sample_rate = format_desc_.audio_sample_rate;
            audio_frame.no_channels = channel_layout_.num_channels;
            audio_frame.timecode = NDIlib_send_timecode_synthesize;
            audio_frame.p_data = audio_buffer.data();
            audio_frame.no_samples = static_cast<int>(audio_buffer.size() / channel_layout_.num_channels);
            ndi_lib_->NDIlib_util_send_send_audio_interleaved_16s(*ndi_send_instance_, &audio_frame);

            // VIDEO
            NDIlib_video_frame_v2_t video_frame = { 0 };
            video_frame.xres = format_desc_.width;
            video_frame.yres = format_desc_.height;
            video_frame.picture_aspect_ratio = static_cast<float>(format_desc_.square_width) / static_cast<float>(format_desc_.square_height);
            video_frame.frame_rate_N = format_desc_.framerate.numerator();
            video_frame.frame_rate_D = format_desc_.framerate.denominator();
            video_frame.timecode = NDIlib_send_timecode_synthesize;

            switch (format_desc_.field_mode) {
                case core::field_mode::progressive:
                    video_frame.frame_format_type = NDIlib_frame_format_type_progressive;
                    break;
                case core::field_mode::upper:
                    video_frame.frame_format_type = NDIlib_frame_format_type_field_0;
                    break;
                case core::field_mode::lower:
                    video_frame.frame_format_type = NDIlib_frame_format_type_field_1;
                    break;
            }

            video_frame.p_data = (uint8_t*)frame.image_data().begin();
            video_frame.FourCC = NDIlib_FourCC_type_BGRA;
            video_frame.line_stride_in_bytes = format_desc_.square_width * 4;

            ndi_lib_->NDIlib_send_send_video_async_v2(*ndi_send_instance_, &video_frame);

            graph_->set_text(print());
            graph_->set_value("frame-time", frame_timer_.elapsed() * format_desc_.fps * 0.5);
            return true;
        });
    }

 
	virtual std::future<bool> send(core::const_frame frame) override
    {
        CASPAR_VERIFY(format_desc_.height * format_desc_.width * 4 == frame.image_data().size());

        if (executor_.size() > 0 || executor_.is_currently_in_task())
        {
            graph_->set_tag(diagnostics::tag_severity::WARNING, "dropped-frame");

            return make_ready_future(true);
        }

        schedule_send(std::move(frame));

        return make_ready_future(true);
    }   

    std::wstring print() const override
    {
        if (channel_index_) {
            return L"ndi_consumer[" + boost::lexical_cast<std::wstring>(channel_index_) + L"|" + name_ + L"]";
        } else {
            return L"[ndi_consumer]";
        }
    }

    std::wstring name() const override { return L"ndi"; }

    std::wstring default_ndi_name() const
    {
        return L"CasparCG" + (instance_no_ ? L" " + boost::lexical_cast<std::wstring>(instance_no_) : L"");
    }


    boost::property_tree::wptree info() const override
    {
        boost::property_tree::wptree info;
        info.add(L"type", L"NDI Consumer");
        return info;
    }
    
    int buffer_depth() const override
    {
        return 5;
    }

    int64_t presentation_frame_age_millis() const override
    {
        return 0;
    }

    int index() const override { return 900; }

    bool has_synchronization_clock() const override { return false; }

    core::monitor::subject& monitor_output()
    {
       return monitor_subject_;
    }

    core::audio_channel_layout get_adjusted_layout(const core::audio_channel_layout& in_layout) const
	{
		auto adjusted = out_channel_layout_ == core::audio_channel_layout::invalid() ? in_layout : out_channel_layout_;

		if (adjusted.num_channels == 1) // Duplicate mono-signal into both left and right.
		{
			adjusted.num_channels = 2;
			adjusted.channel_order.push_back(adjusted.channel_order.at(0)); // Usually FC -> FC FC
		} else {
                    adjusted.num_channels = 4;
                }
		return adjusted;
	}

 
};

std::atomic<int> newtek_ndi_consumer::instances_(0);

void describe_consumer(core::help_sink& sink, const core::help_repository& repo)
{}

spl::shared_ptr<core::frame_consumer> create_ndi_consumer(const std::vector<std::wstring>&                  params,
							  core::interaction_sink*,
                                                          std::vector<spl::shared_ptr<core::video_channel>> channels)
{
    if (params.size() < 1 || !boost::iequals(params.at(0), L"NDI"))
        return core::frame_consumer::empty();

    CASPAR_LOG(info) << L"create_ndi_consumer";
    std::wstring name         = get_param(L"NAME", params, L"");
    bool         allow_fields = contains_param(L"ALLOW_FIELDS", params);
    
    auto out_channel_layout = core::audio_channel_layout::invalid();
    auto channel_layout = get_param(L"CHANNEL_LAYOUT", params);

    CASPAR_LOG(info) <<L"channel_layout" << channel_layout;
	if (!channel_layout.empty())
	{
		auto found_layout = core::audio_channel_layout_repository::get_default()->get_layout(channel_layout);

		if (!found_layout)
			CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"Channel layout " + channel_layout + L" not found."));

		core::audio_channel_layout out_channel_layout = *found_layout;
	}
    return spl::make_shared<newtek_ndi_consumer>(name, allow_fields, out_channel_layout);
}

spl::shared_ptr<core::frame_consumer>
create_preconfigured_ndi_consumer(const boost::property_tree::wptree& ptree, core::interaction_sink*, std::vector<spl::shared_ptr<core::video_channel>> channels)
{
    auto name         = ptree.get(L"name", L"");
    bool allow_fields = ptree.get(L"allow-fields", true);

    auto channel_layout = ptree.get_optional<std::wstring>(L"channel-layout");

    auto out_channel_layout = core::audio_channel_layout::invalid();

    CASPAR_LOG(info) << L"create_preconfigured_ndi_consumer";
    CASPAR_LOG(info) << L"channel_layout" << *channel_layout;

	if (channel_layout)
	{
		CASPAR_SCOPED_CONTEXT_MSG("/channel-layout")

		auto found_layout = core::audio_channel_layout_repository::get_default()->get_layout(*channel_layout);

		if (!found_layout)
			CASPAR_THROW_EXCEPTION(user_error() << msg_info(L"Channel layout " + *channel_layout + L" not found."));

		auto out_channel_layout = *found_layout;
	}

    return spl::make_shared<newtek_ndi_consumer>(name, allow_fields, out_channel_layout);
}

}} // namespace caspar::newtek
