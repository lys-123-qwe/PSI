
#include "TPsiMain.h"
#include "cryptoTools/Network/Endpoint.h"
#include "libPSI/TPsiReceiver.h"
#include "libPSI/TPsiSender.h"
#include "cryptoTools/Common/Defines.h"
#include "libOTe/TwoChooseOne/IknpOtExtReceiver.h"
#include "libOTe/TwoChooseOne/IknpOtExtSender.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Crypto/PRNG.h"
#include <fstream>
#include <algorithm>
#include "boost/format.hpp"
#include <stdlib.h>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include<time.h>
using namespace osuCrypto;

using namespace std;


static int hexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string blockToHex(const block& value)
{
    static const char* digits = "0123456789abcdef";
    const unsigned char* raw = (const unsigned char*)&value;
    std::string text(32, '0');
    for (int i = 0; i < 16; ++i)
    {
        text[2 * i] = digits[raw[i] >> 4];
        text[2 * i + 1] = digits[raw[i] & 0x0F];
    }
    return text;
}

// 读取真实集合：每行一个 32 位十六进制串（16 字节），空行忽略。
static bool loadRealSet(const char* path, u64 setSize, const char* tag, std::vector<block>& out)
{
    std::ifstream in(path);
    if (!in)
    {
        std::cerr << "无法打开 " << tag << " 输入文件: " << path << std::endl;
        return false;
    }
    std::string line;
    u64 lineno = 0;
    while (std::getline(in, line))
    {
        ++lineno;
        size_t begin = 0;
        size_t end = line.size();
        while (begin < end && std::isspace((unsigned char)line[begin])) ++begin;
        while (end > begin && std::isspace((unsigned char)line[end - 1])) --end;
        if (begin == end) continue;
        if (end - begin != 32)
        {
            std::cerr << tag << " 输入第 " << lineno << " 行不是 32 位十六进制" << std::endl;
            return false;
        }
        block value;
        unsigned char* raw = (unsigned char*)&value;
        for (int i = 0; i < 16; ++i)
        {
            int hi = hexNibble(line[begin + 2 * i]);
            int lo = hexNibble(line[begin + 2 * i + 1]);
            if (hi < 0 || lo < 0)
            {
                std::cerr << tag << " 输入第 " << lineno << " 行含非十六进制字符" << std::endl;
                return false;
            }
            raw[i] = (unsigned char)((hi << 4) | lo);
        }
        out.push_back(value);
    }
    if (out.empty())
    {
        std::cerr << tag << " 输入文件没有有效元素: " << path << std::endl;
        return false;
    }
    if (out.size() > setSize)
    {
        std::cerr << tag << " 输入元素 " << out.size() << " 条，超过本次容量 " << setSize
                  << "（请提高 n_log 或先抽样）" << std::endl;
        return false;
    }
    std::cout << tag << " 真实输入:" << out.size() << " 条（容量 " << setSize << "）" << std::endl;
    return true;
}

// 有 MYPSI_IN 时用真实元素填充前 n 个位置，不足部分用随机元素补齐；
// 否则沿用原来的“共同元素 + 随机元素”模拟方式。
static void fillPartySet(std::vector<block>& dst, u64 setSize, u64 common, PRNG& prng, const char* tag)
{
    const char* path = std::getenv("MYPSI_IN");
    if (path && *path)
    {
        std::vector<block> real;
        if (!loadRealSet(path, setSize, tag, real)) std::exit(2);
        for (u64 i = 0; i < setSize; ++i)
        {
            dst[i] = (i < real.size()) ? real[i] : prng.get<block>();
        }
        return;
    }
    PRNG prngTest(_mm_set_epi32(8888888, 8888888, 888888, 88888888));
    for (u64 i = 0; i < common; ++i)
    {
        dst[i] = prngTest.get<block>() + i;
    }
    for (u64 i = common; i < setSize; ++i)
    {
        dst[i] = prng.get<block>();
    }
}


void TPsiSend(int flag, int setNum, int threshold, int commonIn)

{
    int mNumItems;
    if (int(setNum) == 8)
    {
        mNumItems = 256;
    }
    else if (int(setNum) == 9)
    {
        mNumItems = 512;
    }
    else if (int(setNum) == 10)
    {
        mNumItems = 1024;
    }
    else if (int(setNum) == 11)
    {
        mNumItems = 2048;
    }
    else if (int(setNum) == 12)
    {
        mNumItems = 4096;
    }
    else if (int(setNum) == 13)
    {
        mNumItems = 8192;
    }
    else if (int(setNum) == 14)
    {
        mNumItems = 16384;
    }
    else if (int(setNum) == 15)
    {
        mNumItems = 32768;
    }
    else if (int(setNum) == 16)
    {
        mNumItems = 65536;
    }
    else if (int(setNum) == 17)
    {
        mNumItems = 131072;
    }
    else
    {
        mNumItems = 512;
    }
    std::cout << "集合元素的个数为:" << "" << mNumItems << "" << "个" << std::endl;
    int mNumThreads = 1;
    int mThreshold = threshold;
    // int k = 50;
    if (flag == 1)
    {
        PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987043));

        int setSize = mNumItems;
        int tt = mNumThreads;
        u64 thr = mThreshold;
        u64 common = commonIn;
        if (commonIn < 0) common = 0;
        if (common > (u64)setSize) common = setSize;

        //auto chls = params.getChannels(tt);

        for (u64 jj = 0; jj < 1; jj++)

        {
            int SetSize = setSize + setSize - thr;//n+n-t

            std::vector<block> sendSet(SetSize);
            /*sendSet[0] = _mm_set_epi32(4253465, 3434565, 234435, 23987045);
              sendSet[1] = _mm_set_epi32(4253465, 3434565, 234435, 23987046);
              sendSet[2] = _mm_set_epi32(4253465, 3434565, 234435, 23987047);
              sendSet[3] = _mm_set_epi32(4253465, 3434565, 234435, 23987048);

              sendSet[4] = _mm_set_epi32(4253465, 3434565, 234435, 23987040);
              sendSet[5] = _mm_set_epi32(4253465, 3434565, 234435, 23987041);
              sendSet[6] = _mm_set_epi32(4253465, 3434565, 234435, 23987042);*/
              //std::cout << SetSize << std::endl;

              /*for (int i = 0; i < SetSize; i++)
              {
                  std::cout << "sendSet=" << i << "" << sendSet[i] << std::endl;
              }*/
            fillPartySet(sendSet, setSize, common, prng, "P1");
            // 产生 n-t 个伪元素
            PRNG prngSame(_mm_set_epi32(0000000, 0000000, 000000, 00000000));
            for (u64 i = setSize; i < SetSize; ++i)
            {
                sendSet[i] = prngSame.get<block>() + i;
            }

            IknpOtExtReceiver otRecv;

            IknpOtExtSender otSend;

            TPsiSender sendPSIs;

            gTimer.reset();



            sendPSIs.init(SetSize, thr, 40, otSend, prng.get<block>());


            sendPSIs.sendInput(sendSet, flag);
        }
    }
    else
    {
        PRNG prng(_mm_set_epi32(4253464, 3434564, 234434, 23987044));

        int setSize = mNumItems;
        int tt = mNumThreads;
        u64 thr = mThreshold;
        u64 common = commonIn;
        if (commonIn < 0) common = 0;
        if (common > (u64)setSize) common = setSize;

        for (u64 jj = 0; jj < 1; jj++)

        {
            int SetSize = setSize + setSize - thr;

            std::vector<block> sendSet(SetSize);

            fillPartySet(sendSet, setSize, common, prng, "P2");
            // 产生 n-t 个伪元素
            PRNG prngSame(_mm_set_epi32(0000000, 0000000, 000000, 00000000));
            for (u64 i = setSize; i < SetSize; ++i)
            {
                sendSet[i] = prngSame.get<block>() + i;
            }
            IknpOtExtReceiver otRecv;

            IknpOtExtSender otSend;

            TPsiSender sendPSIs;

            gTimer.reset();

            sendPSIs.init2(SetSize, thr, 40, otSend, prng.get<block>());

            sendPSIs.sendInput2(sendSet, flag);

        }
    }


}


void TPsiRecv(int setNum, int threshold, int commonIn)

{
    int mNumItems;
    std::cout << "setNum为:::" << setNum << std::endl;
    if (int(setNum) == 8)
    {
        mNumItems = 256;
    }
    else if (int(setNum) == 9)
    {
        mNumItems = 512;
    }
    else if (int(setNum) == 10)
    {
        mNumItems = 1024;
    }
    else if (int(setNum) == 11)
    {
        mNumItems = 2048;
    }
    else if (int(setNum) == 12)
    {
        mNumItems = 4096;
    }
    else if (int(setNum) == 13)
    {
        mNumItems = 8192;
    }
    else if (int(setNum) == 14)
    {
        mNumItems = 16384;
    }
    else if (int(setNum) == 15)
    {
        mNumItems = 32768;
    }
    else if (int(setNum) == 16)
    {
        mNumItems = 65536;
    }
    else if (int(setNum) == 17)
    {
        mNumItems = 131072;
    }
    else
    {
        mNumItems = 512;
    }
    std::cout << "集合元素的个数为:" << "" << mNumItems << "" << "个" << std::endl;
    int mNumThreads = 1;
    int mThreshold = threshold;
    std::cout << "门限值为:" << "" << mThreshold << std::endl;
    PRNG prng(_mm_set_epi32(4253465, 3434565, 234435, 23987045));
    //int setSize = mNumItems;
    int setSize = mNumItems;
    int numThreads = mNumThreads;
    //u64 thr = mThreshold;
    u64 thr = mThreshold;
    u64 common = commonIn;
    if (commonIn < 0) common = 0;
    if (common > (u64)setSize) common = setSize;


    for (u64 jj = 0; jj < 1; jj++)

    {
        int SetSize = setSize + setSize - thr;
        //std::cout << SetSize << std::endl;
        std::vector<block> set(SetSize);

        fillPartySet(set, setSize, common, prng, "P0");
        // 产生 n-t 个伪元素
        PRNG prngSame(_mm_set_epi32(0000000, 0000000, 000000, 00000000));
        for (u64 i = setSize; i < SetSize; ++i)
        {
            set[i] = prngSame.get<block>() + i;
        }
        IknpOtExtReceiver otRecv;

        IknpOtExtSender otSend;

        TPsiReceiver recvPSIs;

        gTimer.reset();

        Timer timer;

        recvPSIs.init(SetSize, thr, 40, otRecv, ZeroBlock);

        recvPSIs.sendInput(set);

        const char* outPath = std::getenv("MYPSI_OUT");
        if (outPath && *outPath)
        {
            std::ofstream out(outPath);
            if (!out)
            {
                std::cerr << "无法写入交集文件: " << outPath << std::endl;
            }
            else
            {
                u64 written = 0;
                for (size_t pos = 0; pos < recvPSIs.mIntersection.size(); ++pos)
                {
                    u64 idx = recvPSIs.mIntersection[pos];
                    if (idx >= (u64)setSize) continue; // 过滤 n-t 个伪元素
                    out << blockToHex(set[idx]) << "\n";
                    ++written;
                }
                std::cout << "Intersection output:" << written << " 条 -> " << outPath << std::endl;
            }
        }


    }

}






