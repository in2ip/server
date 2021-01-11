#include <cstdint>
#include <vector>

static inline void write_vanc_pkt_10bit(std::vector<std::uint8_t>&inbuf, std::vector<std::uint16_t>&outbuf,uint8_t did,uint8_t sdid)
{
    outbuf.reserve(inbuf.size() + 7);//vanc header (3) + sdid + (1) + did (1) + count (1) + checksum (1) = 7;
    outbuf.push_back(0x000);
    outbuf.push_back(0x3ff);
    outbuf.push_back(0x3ff);
    outbuf.push_back(did);
    outbuf.push_back(sdid);
    outbuf.push_back(inbuf.size());
    for (uint8_t word : inbuf)
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
}