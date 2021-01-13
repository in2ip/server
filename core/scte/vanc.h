#include <cstdint>
#include <vector>

#ifndef _WIN32
extern "C" {
	#include "endian.h"
}
#else
	//Safe to assume win32 is LE
	#define htole32(x) (x)
#endif

static inline std::vector<std::uint16_t> write_vanc_pkt_10bit(std::vector<std::uint8_t> const &inbuf, uint8_t did,uint8_t sdid)
{
    std::vector<std::uint16_t> outbuf = std::vector<std::uint16_t>();
    outbuf.reserve(inbuf.size() + 7);//vanc header (3) + sdid + (1) + did (1) + count (1) + checksum (1) = 7;
    outbuf.push_back(0x000);
    outbuf.push_back(0x3ff);
    outbuf.push_back(0x3ff);
    outbuf.push_back(did);
    outbuf.push_back(sdid);
    outbuf.push_back(inbuf.size());
    for (const auto& word : inbuf)
        outbuf.push_back(word);
    
    uint16_t checksum = 0;
    uint16_t *data = outbuf.data();
    //set even parity & inverse bits + calculate checksum
    for (int i= 3; i < outbuf.size(); i++)
    {
        if (__builtin_parity(data[i]))
            data[i] |= 0x100;
        else
            data[i] |= 0x200;
        checksum += data[i];
        checksum &= 0x1ff;
    }
    checksum |= (~checksum & 0x100) << 1;//set the inverse bit
    outbuf.push_back(checksum);
    return outbuf;
}

void inline write_v210_pixel(uint32_t pixel, std::vector<std::uint32_t>&dst)
{
	dst.push_back(htole32(pixel));
}

static void pack_y10_to_v210(uint16_t *src, std::vector<uint32_t>& dst, int width)
{
	size_t len = width /6;//6 Y per 4 pixels
	size_t w;
	for (w = 0; w < len; w++)
	{
		write_v210_pixel(src[w * 6 + 0] << 10, dst);// U0+1, Y0, V0+1
		write_v210_pixel(src[w * 6 + 1] | (src[w * 6 + 2] << 20), dst);//Y1, U2+3, Y3
		write_v210_pixel(src[w * 6 + 3] << 10, dst);// V2+3, Y4, U4+5
		write_v210_pixel(src[w * 6 + 4] | (src[w * 6 + 5] << 20), dst);//Y4, V4+5, Y5
	}
	size_t remaining = width % 6;
	if (remaining)
	{
		write_v210_pixel(src[w * 6 + 0] << 10, dst);// U0+1, Y0, V0+1
		if (remaining == 2)
		{
			write_v210_pixel(src[w * 6 + 1], dst);
		} else if (remaining > 2)
		{
			write_v210_pixel(src[w * 6 + 1] | (src[w * 6 + 2] << 20),  dst);
			if (remaining > 3)
			{
				write_v210_pixel(src[w * 6 + 3] << 10, dst);
				if (remaining > 4)
				{
					write_v210_pixel(src[w * 6 + 4] | (src[w * 6 + 5] << 20), dst);
				}
			}
		}
	}
}

static void pack_uyvy_to_v210(uint16_t *src, std::vector<uint32_t>& dst, int width)
{
	size_t len = width /12;//We fill all datapoints per block of 4 pixels
	size_t w;

	uint32_t pixel = 0;
	for (w = 0; w < len; w++)
	{
		pixel = src[w * 12 + 0] | (src[w * 12 + 1] << 10) | (src[w * 12 + 2] << 20);
		write_v210_pixel(pixel, dst);
		pixel = src[w * 12 + 3] | (src[w * 12 + 4] << 10) | (src[w * 12 + 5] << 20);
		write_v210_pixel(pixel, dst);
		pixel = src[w * 12 + 6] | (src[w * 12 + 7] << 10) | (src[w * 12 + 8] << 20);
		write_v210_pixel(pixel, dst);
		pixel = src[w * 12 + 9] | (src[w * 12 + 10] << 10) | (src[w * 12 + 11] << 20);
		write_v210_pixel(pixel, dst);
	}

	size_t remaining = width % 12;
	if (remaining)
	{

		pixel = src[w * 12 + 0];
		if (remaining > 1)
			pixel |= (src[w * 12 + 1] << 10);
		else
			pixel |= (0x040 << 10);
		if (remaining > 2)
			pixel |= (src[w * 12 + 2] << 20);
		else
			pixel |= (0x200 << 20);
		write_v210_pixel(pixel, dst);
		if (remaining > 3)
		{
			pixel = src[w * 12 + 3];
			if (remaining > 4)
				pixel |= (src[w * 12 + 4] << 10);
			else
				pixel |= (0x040 << 10);
			if (remaining > 5)
				pixel |= (src[w * 12 + 5] << 20);
			else
				pixel |= (0x200 << 20);
			write_v210_pixel(pixel, dst);
			if (remaining > 6)
			{
				pixel = src[w * 12 + 6];
				if (remaining > 7)
					pixel |= (src[w * 12 + 7] << 10);
				else
					pixel |= (0x040 << 10);
				if (remaining > 8)
					pixel |= (src[w * 12 + 8] << 20);
				else
					pixel |= (0x200 << 20);
				write_v210_pixel(pixel, dst);
				if (remaining > 9)
				{
					pixel = src[w * 12 + 9];
					if (remaining > 10)
						pixel |= (src[w * 12 + 10] << 10);
					else
						pixel |= (0x040 << 10);
					pixel |= (0x200 << 20);
					write_v210_pixel(pixel, dst);
				}
			}
		}
	}
}

static inline std::vector<uint32_t> pack_vanc_v210(std::vector<uint16_t>&inbuf, int width)
{
    std::vector<uint32_t> out_buf = std::vector<uint32_t>();
	size_t allign_size = (width + 31) & ~31;//First next 128byte boundary, not sure if needed
	out_buf.reserve(allign_size);
    if (width > 720)
        pack_y10_to_v210(inbuf.data(), out_buf, inbuf.size());
    else
       pack_uyvy_to_v210(inbuf.data(), out_buf, inbuf.size());
	out_buf.resize(allign_size, 0);
    return out_buf;
}