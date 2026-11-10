
#pragma once
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Network/Channel.h"
//#include "cryptoTools/Crypto/sha1.h"
#include "cryptoTools/Crypto/RandomOracle.h"
#include "cryptoTools/Common/BitVector.h"
#include <vector>
#include "libOTe/TwoChooseOne/OTExtInterface.h"

#include <NTL/ZZ_pX.h>
#include <NTL/ZZ_p.h>
#include <NTL/BasicThreadPool.h>
#include <NTL/ZZ.h>
#include <NTL/vec_ZZ_p.h>
#include <cstdlib>

#include <NTL/vector.h>
#include <NTL/ZZ_p.h>
#include <NTL/vec_ZZ.h>
#include <NTL/vec_ZZ_p.h>
#include <NTL/FFT.h>
#include <NTL/Lazy.h>
#include <NTL/SmartPtr.h>
#include <sys/time.h>
#include <set>

using namespace std;
using namespace NTL;
namespace osuCrypto
{



    class TPsiReceiver
    {
    public:


        TPsiReceiver();
        ~TPsiReceiver();

        //DcwOtReceiver mDcwOt;
        //std::vector<SHA1> mHashs;
        std::vector<RandomOracle> mHashs;
        u64 mMyInputSize, mTheirInputSize, mBfBitCount, mStatSecParam;
        block mHashingSeed;
        RandomOracle hashmy_secret;
        std::vector<u64> mIntersection;
        std::vector<block> mMessages;
        BitVector mRandChoices;
        double interpolateTime;
        block mEncSeed, mSeed, share;
        u64 threshold, input_size;
        u64 temp;
        ZZ p;

        //ZZ x;
        //block yy;
        ZZ_p func1(ZZ_pX& f, ZZ_p& x);
        bool zuhe(vector<int> a, vector<block> indexs, vector<block> share, int n, int m, ZZ_pX& f);
        block interpolate1(std::vector<block>& index, std::vector<block>& msgs, ZZ_pX& f);
        //block interpolate(block prime, std::vector<block>& msgs);

        void init(u64 n, u64 thre, u64 statSecParam, OtExtReceiver& otExt, block seed);
        //void init(u64 n, u64 thre, u64 statSecParam, OtExtReceiver& otExt, span<Channel> chl0, block seed);
        void sendInput(std::vector<block>& inputs);
        //void sendInput(std::vector<block>& inputs, span<Channel> chl0);
        void clientSend(char* data, int port);
        void serverRecv(int port);
    };

}
//#endif

