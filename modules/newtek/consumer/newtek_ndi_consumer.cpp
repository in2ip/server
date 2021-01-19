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
#include "core/ancillary/ancillary.h"

#include <common/assert.h>
#include <common/diagnostics/graph.h>
#include <common/future.h>
#include <common/param.h>
#include <common/timer.h>
#include <boost/rational.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <boost/crc.hpp>

#include <atomic>

#include "../util/ndi.h"
extern "C"
{
            #include "base64.h"
	        #include <libswscale/swscale.h>
	        #include <libavcodec/avcodec.h>
	        #include <libavformat/avformat.h>
            #include <libavutil/imgutils.h>
}


namespace caspar { namespace newtek {


		int crc16(const std::wstring& str)
		{
			boost::crc_16_type result;
			result.process_bytes(str.data(), str.length());
			return result.checksum();
		}

struct newtek_ndi_consumer : public boost::noncopyable
{
    const std::wstring      name_;
    const bool              allow_fields_;
    const std::wstring      failover_;
    const std::wstring      groups_;

    core::monitor::subject               monitor_subject_;
    core::video_format_desc              format_desc_;
    core::audio_channel_layout           channel_layout_ = core::audio_channel_layout::invalid();
    core::audio_channel_layout           out_channel_layout_ = core::audio_channel_layout::invalid();
    std::unique_ptr<core::audio_channel_remapper>	channel_remapper_;

    int                                  channel_index_;
    NDIlib_v4*                           ndi_lib_;
    NDIlib_video_frame_v2_t              ndi_video_frame_;
    NDIlib_audio_frame_interleaved_32s_t ndi_audio_frame_ = { 0 };
    struct AVRational                    timebase_channel_;
    struct AVRational                    timebase_ndi_;
    spl::shared_ptr<diagnostics::graph>  graph_;
    executor                             executor_;
    caspar::timer                        frame_tick_timer_;
    caspar::timer                        ndi_tick_timer_;
    caspar::timer                        ndi_consume_timer_;
    int                                  frame_no_;
    int64_t                              timebase_frame_no_;
    int64_t                              start_fps_measure_frame_no_;
    std::atomic<bool>                    is_sending_;
    std::atomic<bool>                    started_;
    int64_t                              ndi_start_time_;
    int64_t                              last_print_time;
    int64_t                              ticktime_;
    int64_t                              next_tick_;
    std::thread                        thread_;
    std::atomic<int64_t>                        current_encoding_delay_;
    caspar::semaphore									ready_for_new_frames_	{ 0 };
    std::unique_ptr<SwsContext, std::function<void(SwsContext*)>>	sws_;
    std::vector<uint8_t, tbb::cache_aligned_allocator<uint8_t>>     send_frame_buffer_;
    tbb::concurrent_bounded_queue<core::const_frame>	frame_buffer_;

    std::unique_ptr<NDIlib_send_instance_t, std::function<void(NDIlib_send_instance_t*)>> ndi_send_instance_;

  public:
    newtek_ndi_consumer(std::wstring name, bool allow_fields, const core::audio_channel_layout& out_channel_layout, std::wstring failover, std::wstring groups,
    const core::video_format_desc& format_desc, const core::audio_channel_layout& channel_layout, int channel_index)
        : name_(name)
        , groups_ (groups)
        , failover_(failover)
        , allow_fields_(allow_fields)
        , frame_no_(0)
        , timebase_frame_no_(0)
        , channel_index_(channel_index)
        , out_channel_layout_(out_channel_layout)
        , executor_(print())
        , sws_(sws_getContext(format_desc.width, format_desc.height, AV_PIX_FMT_BGRA, format_desc.width, format_desc.height, AV_PIX_FMT_UYVY422, SWS_POINT, NULL, NULL, NULL), [](SwsContext * ctx) { sws_freeContext(ctx); })
        , send_frame_buffer_(av_image_get_buffer_size(AV_PIX_FMT_BGRA, format_desc.width, format_desc.height, 16))
    {

        frame_buffer_.set_capacity(8);

        ndi_lib_ = ndi::load_library();
        graph_->set_text(print());
        graph_->set_color("buffered-frames", diagnostics::color(0.5f, 1.0f, 0.2f));
        graph_->set_color("frame-tick", diagnostics::color(0.0f, 0.6f, 0.9f));
        graph_->set_color("ndi-tick", diagnostics::color(1.0f, 1.0f, 0.1f));
        graph_->set_color("ndi-consume-time", diagnostics::color(1.0f, 1.0f, 1.0f));
        graph_->set_color("dropped-frame", diagnostics::color(1.0f, 0.0f, 0.0f));
        diagnostics::register_graph(graph_);
        current_encoding_delay_ = 0;
        start_fps_measure_frame_no_ = 0;
        started_ = false;
        format_desc_   = format_desc;
        channel_layout_ = channel_layout;
        out_channel_layout_ = get_adjusted_layout(channel_layout_);

        channel_remapper_.reset(new core::audio_channel_remapper(channel_layout_, out_channel_layout_));

        timebase_channel_.num = format_desc_.duration;
        timebase_channel_.den = format_desc_.time_scale;
        timebase_ndi_.num = 10000000LL;
        timebase_ndi_.den = 1;

        ndi_video_frame_.xres                 = format_desc_.width;
        ndi_video_frame_.yres                 = format_desc_.height;
        ndi_video_frame_.frame_rate_N         = format_desc_.time_scale;
        ndi_video_frame_.frame_rate_D         = format_desc_.duration;
        ndi_video_frame_.FourCC               = NDIlib_FourCC_type_BGRA;
        ndi_video_frame_.line_stride_in_bytes = format_desc_.width * 4;
        ndi_video_frame_.frame_format_type    = NDIlib_frame_format_type_progressive;
        if (format_desc_.field_count == 2 && allow_fields_) {
            ndi_video_frame_.frame_format_type = NDIlib_frame_format_type_interleaved;
        }


        ndi_audio_frame_.reference_level = 0;
        ndi_audio_frame_.no_channels = out_channel_layout_.num_channels;
        ndi_audio_frame_.sample_rate = format_desc_.audio_sample_rate;

        thread_ = std::thread([this]{run();});
        ticktime_ = av_rescale_q(1000000LL, timebase_channel_, (AVRational){1,1});
        CASPAR_LOG(warning) << "Ticktime is " << ticktime_;
        graph_->set_text(print());

        for (int i = 0; i < 7; i++) {
            auto frame = core::const_frame::empty();
            frame_buffer_.try_push(frame);
        }
    }

    ~newtek_ndi_consumer() {
        is_sending_ = false;
        frame_buffer_.try_push(core::const_frame::empty());
        thread_.join();
    }

    int64_t GetTimeUsec() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
    	int64_t now =  ts.tv_sec * 1000000LL + ts.tv_nsec /1000;
        return now;
    }

    void WaitNextTick() {
        struct timespec ts;
        ts.tv_sec = (next_tick_ / 1000000LL);
        ts.tv_nsec = ((next_tick_ % 1000000) * 1000);
        clock_nanosleep( CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
        next_tick_ += ticktime_;
    }

    void run() 
    {
        ensure_gpf_handler_installed_for_thread("ndi-send-thread");
        int retcode;
        int policy;

        pthread_t threadID = (pthread_t) thread_.native_handle();

        struct sched_param param;

        pthread_getschedparam(threadID, &policy, &param);

        policy = SCHED_FIFO;
        param.sched_priority = 2;

        if (retcode = pthread_setschedparam(threadID, policy, &param)) {
            CASPAR_LOG(error) << "Failed to set RT prio for NDI thread to 2";
        }

        NDIlib_send_create_t NDI_send_create_desc;

        auto tmp_name                   = u8(name_);
        NDI_send_create_desc.p_ndi_name = tmp_name.c_str();
        if (!groups_.empty()) {
            auto tmp_groups             = u8(groups_);
            NDI_send_create_desc.p_groups = tmp_groups.c_str();
        }
        NDI_send_create_desc.clock_audio = false;
        NDI_send_create_desc.clock_video = false;

        if (!failover_.empty()) {
            auto tmp_failover = u8(failover_);
            NDIlib_source_t NDI_failover_source {};
            NDI_failover_source.p_ndi_name = tmp_failover.c_str();
            ndi_lib_->NDIlib_send_set_failover(*ndi_send_instance_, &NDI_failover_source); 
        }

        ndi_send_instance_ = {new NDIlib_send_instance_t(ndi_lib_->NDIlib_send_create(&NDI_send_create_desc)),
                        [this](auto p) { this->ndi_lib_->NDIlib_send_destroy(*p); }};

        is_sending_ = true;
        /*while (!started_) {
            usleep(500);
        }*/

        ndi_start_time_ = last_print_time =GetTimeUsec();
        next_tick_ = ndi_start_time_ + ticktime_;
        while (is_sending_)
        {
            ready_for_new_frames_.release();
            graph_->set_value("ndi-tick", ndi_tick_timer_.elapsed() * format_desc_.fps * 0.5);
            ndi_tick_timer_.restart();
            ndi_send_frame();
            WaitNextTick();
        }
    }

    std::future<bool> send(core::const_frame frame)
    {
        graph_->set_value("frame-tick", frame_tick_timer_.elapsed() * format_desc_.fps * 0.5);
        frame_tick_timer_.restart(); 
        if (!frame_buffer_.try_push(frame)) {
		    graph_->set_tag(diagnostics::tag_severity::WARNING, "dropped-frame");
	    }
        frame_no_++;
        /*if (frame_no_ <= 6) {
            if (frame_no_ == 6) {
                started_ = true;
                return make_ready_future(true);
            }
            else 
                return make_ready_future(true);
        }*/
            
        graph_->set_value("buffered-frames", static_cast<double>(frame_buffer_.size() + 0.001) / frame_buffer_.capacity());
        auto send_completion = spl::make_shared<std::promise<bool>>();

		ready_for_new_frames_.acquire(1, [send_completion]
		{
			send_completion->set_value(true);
		});

		return send_completion->get_future();
    }

     bool ndi_send_frame()
    {
        ndi_consume_timer_.restart();
        auto frame = core::const_frame::empty();
        frame_buffer_.pop(frame);

        //int64_t timecode = av_rescale_q(timebase_frame_no_, timebase_channel_, timebase_ndi_);
        //ndi_audio_frame_.timecode = timecode;
        //ndi_video_frame_.timecode = timecode;
        int64_t now = GetTimeUsec();

        if ((now - last_print_time) > (60 * 1000000LL) ) {
            double fpssixtysec = (double)((timebase_frame_no_ - start_fps_measure_frame_no_)* 1000000LL)/(double)(now - last_print_time);
            double fpssincestart = (double)(timebase_frame_no_ *1000000LL)/(double)(now - ndi_start_time_);
            CASPAR_LOG(warning) << std::fixed << "{\"channel-"<< channel_index_ 
                << "\": { \"ndi_name\": \"" << name_ <<"\""
                << ", \"fps_since_start\":" << std::setprecision(6) << fpssincestart 
                << ", \"fps_last_sixty_sec\": " << fpssixtysec 
                << ", \"frames_sent\": " << timebase_frame_no_ 
                <<", \"buffered_frames\": " <<frame_buffer_.size() << "}}";
            start_fps_measure_frame_no_ = timebase_frame_no_;
            last_print_time = now;
        }

        //uint8_t * src_data[AV_NUM_DATA_POINTERS];
        //int src_linesize[AV_NUM_DATA_POINTERS];
        //uint8_t * dest_data[AV_NUM_DATA_POINTERS];
        //int dst_linesize[AV_NUM_DATA_POINTERS];
        //av_image_fill_arrays(src_data, src_linesize, frame.image_data().begin(), AV_PIX_FMT_BGRA, format_desc_.width, format_desc_.height, 1);
        //av_image_fill_arrays(dest_data, dst_linesize, &send_frame_buffer_.front(), AV_PIX_FMT_UYVY422, format_desc_.width, format_desc_.height, 16);
        //sws_scale(sws_.get(), src_data, src_linesize, 0, format_desc_.height, dest_data, dst_linesize);
        //ndi_video_frame_.p_data = &send_frame_buffer_.front();

        core::audio_buffer a_data = frame.audio_data();
        uint8_t* v_data = const_cast<uint8_t*>(frame.image_data().begin());
        if (frame.image_data().empty()) {
            CASPAR_LOG(error) << "Empty v_data, assuming startup buffer fill";
            v_data = (uint8_t*)calloc(4, format_desc_.height * format_desc_.width);
        }
        if (a_data.empty()) {
            CASPAR_LOG(error) << "Empty a_data, assuming startup buffer fill";
            spl::shared_ptr<core::mutable_audio_buffer>buf(new core::mutable_audio_buffer(format_desc_.audio_cadence[timebase_frame_no_ % format_desc_.audio_cadence.size()] * channel_layout_.num_channels, 0));
            a_data = core::audio_buffer(buf->data(), buf->size(), true, std::move(buf));
        }

        std::stringstream metadata;
        boost::property_tree::ptree metadata_tree;
        bool write_meta = false;
        for (auto& vanc_line : frame.ancillary().getAncillaryAsLines(1920))
        {
            CASPAR_LOG(debug) << "Adding VANC line to metadata";
            char *base64_vanc = (char*)calloc(Base64encode_len(vanc_line.size() * 4) +1, 1);
            Base64encode(base64_vanc, (const char *)vanc_line.data(), (vanc_line.size() * 4));
            CASPAR_LOG(debug) << base64_vanc;
            metadata_tree.put("VANC_DATA", base64_vanc);
            free(base64_vanc);
            write_meta = true;
        }

        if (write_meta)
            write_xml(metadata, metadata_tree);

        char *test = (char *)calloc(metadata.str().size() +1, 1);
        memcpy(test, metadata.str().c_str(), metadata.str().size());
        ndi_video_frame_.p_metadata = test;

        ndi_video_frame_.p_data = v_data;
        ndi_lib_->NDIlib_send_send_video_v2(*ndi_send_instance_, &ndi_video_frame_);
        auto audio_buffer = channel_remapper_->mix_and_rearrange(a_data);
        ndi_audio_frame_.p_data = const_cast<int*>(audio_buffer.data());
        ndi_audio_frame_.no_samples = static_cast<int>(audio_buffer.size() / out_channel_layout_.num_channels);
        ndi_lib_->NDIlib_util_send_send_audio_interleaved_32s(*ndi_send_instance_, &ndi_audio_frame_);
        current_encoding_delay_ = frame.get_age_millis();
        timebase_frame_no_++;
        graph_->set_value("ndi-consume-time", ndi_consume_timer_.elapsed() * format_desc_.fps * 0.5);
        free(test);
        return true;
    }

    std::wstring print() const
    {
        return L"NewTek NDI[" + std::to_wstring(static_cast<long long>(channel_index_)) + L":" + name_ + L"]";
    }

    boost::property_tree::wptree info() const
    {
        boost::property_tree::wptree info;
        info.add(L"type", L"NDI Consumer");
        info.add(L"name", name_);
        if (!failover_.empty()) {
            info.add(L"failover-source",failover_);
        }
        if (!groups_.empty()) {
            info.add(L"groups", groups_);
        }
        info.add(L"connected-clients", ndi_lib_->NDIlib_send_get_no_connections(*ndi_send_instance_, 0));
        return info;
    }
    
    int buffer_depth() const
    {
        return 5;
    }

    int64_t presentation_frame_age_millis() const
    {
        return current_encoding_delay_;
    }

    bool has_synchronization_clock() const { return true; }

    core::audio_channel_layout get_adjusted_layout(const core::audio_channel_layout& in_layout) const
	{
		auto adjusted = out_channel_layout_ == core::audio_channel_layout::invalid() ? in_layout : out_channel_layout_;

		if (adjusted.num_channels == 1) // Duplicate mono-signal into both left and right.
		{
			adjusted.num_channels = 2;
			adjusted.channel_order.push_back(adjusted.channel_order.at(0)); // Usually FC -> FC FC
		} 
		return adjusted;
	}
};

    struct ndi_consumer_proxy : public core::frame_consumer
		{
            static std::atomic<int> instances_;
            const int               instance_no_;
			const int								index_;
			std::unique_ptr<newtek_ndi_consumer>	consumer_;
            core::audio_channel_layout              out_channel_layout_ = core::audio_channel_layout::invalid();
			const std::wstring						ndi_name_;
			const std::wstring						groups_;
            const std::wstring						failover_;
			const bool								allow_fields_;
			int										channel_index_;
            core::monitor::subject                  monitor_subject_;
            executor								executor_;

		public:

			ndi_consumer_proxy(std::wstring name, bool allow_fields, const core::audio_channel_layout& out_channel_layout, std::wstring failover, std::wstring groups)
				: index_(900 + crc16(name))
                , instance_no_(instances_++)
				, ndi_name_(!name.empty() ? name : default_ndi_name())
				, groups_(groups)
                , out_channel_layout_(out_channel_layout)
                , allow_fields_(allow_fields)
                , failover_(failover)
                , executor_(print())
			{	}

		    void initialize(const core::video_format_desc& format_desc, const core::audio_channel_layout& channel_layout, int channel_index) override
			{
				executor_.invoke([=]
                {
                    consumer_.reset(new newtek_ndi_consumer(ndi_name_, allow_fields_, out_channel_layout_, failover_, groups_, format_desc, channel_layout, channel_index));
                });
			}

            std::wstring default_ndi_name() const
            {
                return L"CasparCG" + (instance_no_ ? L" " + boost::lexical_cast<std::wstring>(instance_no_) : L"");
            }

			bool has_synchronization_clock() const override
			{
				return consumer_->has_synchronization_clock();
			}

			int buffer_depth() const override
			{
				return consumer_->buffer_depth();
			}

			int64_t presentation_frame_age_millis() const override
			{
				return consumer_->presentation_frame_age_millis();
			}

			std::future<bool> send(core::const_frame frame) override
			{
				return consumer_->send(frame);
			}

			std::wstring print() const override
			{
				return consumer_ ? consumer_->print() : L"NewTek NDI[" + ndi_name_ + L" (not initialized)]";
			}

            std::wstring name() const { return L"ndi"; }

			boost::property_tree::wptree info() const override
			{
                return consumer_->info();
			}

			int index() const override
			{
				return index_;
			}

        	core::monitor::subject& monitor_output()
        	{
         		return monitor_subject_;
         	}
		};


std::atomic<int> ndi_consumer_proxy::instances_(0);

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

    std::wstring failover = get_param(L"FAILOVER_SOURCE", params, L"");
    std::wstring groups = get_param(L"GROUPS", params, L"");
    
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
    return spl::make_shared<ndi_consumer_proxy>(name, allow_fields, out_channel_layout, failover, groups);
}

spl::shared_ptr<core::frame_consumer>
create_preconfigured_ndi_consumer(const boost::property_tree::wptree& ptree, core::interaction_sink*, std::vector<spl::shared_ptr<core::video_channel>> channels)
{
    auto name         = ptree.get(L"name", L"");
    bool allow_fields = ptree.get(L"allow-fields", true);

    auto channel_layout = ptree.get_optional<std::wstring>(L"channel-layout");

    std::wstring failover = ptree.get(L"failover-source", L"");
    std::wstring groups = ptree.get(L"groups", L"");
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

    return spl::make_shared<ndi_consumer_proxy>(name, allow_fields, out_channel_layout, failover, groups);
}

}} // namespace caspar::newtek
