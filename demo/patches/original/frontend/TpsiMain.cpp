
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
#include<time.h>
using namespace osuCrypto;

using namespace std;



void TPsiSend(int flag, int setNum, int threshold)

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
              // 为了测试,现需要产生达到门限值的共同的集合元素
            PRNG prngTest(_mm_set_epi32(8888888, 8888888, 888888, 88888888));
            for (u64 i = 0; i < thr; ++i)
            {
                sendSet[i] = prngTest.get<block>() + i;
            }
            for (u64 i = thr; i < setSize; ++i)
            {
                sendSet[i] = prng.get<block>();
            }
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

        for (u64 jj = 0; jj < 1; jj++)

        {
            int SetSize = setSize + setSize - thr;

            std::vector<block> sendSet(SetSize);

            // 为了测试,现需要产生达到门限值的共同的集合元素
            PRNG prngTest(_mm_set_epi32(8888888, 8888888, 888888, 88888888));
            for (u64 i = 0; i < thr; ++i)
            {
                sendSet[i] = prngTest.get<block>() + i;
            }
            for (u64 i = thr; i < setSize; ++i)
            {
                sendSet[i] = prng.get<block>();
            }
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


void TPsiRecv(int setNum, int threshold)

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


    for (u64 jj = 0; jj < 1; jj++)

    {
        int SetSize = setSize + setSize - thr;
        //std::cout << SetSize << std::endl;
        std::vector<block> set(SetSize);

        // 为了测试,现需要产生达到门限值的共同的集合元素
        PRNG prngTest(_mm_set_epi32(8888888, 8888888, 888888, 88888888));
        for (u64 i = 0; i < thr; ++i)
        {
            set[i] = prngTest.get<block>() + i;
        }
        for (u64 i = thr; i < setSize; ++i)
        {
            set[i] = prng.get<block>();
        }
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


    }

}






