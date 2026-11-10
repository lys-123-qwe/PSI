#pragma once
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Network/Channel.h"
//#include "cryptoTools/Crypto/sha1.h"
#include "cryptoTools/Crypto/RandomOracle.h"
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

    class TPsiSender
    {
    public:
        TPsiSender();
        ~TPsiSender();

        //void computeParameters(u64 n, u64 statSecParam, u64& totalOtCount, u64& cncOnesThreshold, double& cncProb, u64& numHashFunctions, u64& bfBitCount);

        u64 mN, mStatSecParam, mBfBitCount;
        //DcwOtSender mDcwOt;
        block mHashingSeed;

        ZZ p;
        //std::vector<SHA1> mHashs;
        std::vector<RandomOracle> mHashs;
        RandomOracle hash_secret;

        std::vector<std::array<block, 2>> mSendOtMessages;

        ZZ_p func(ZZ_pX& f, ZZ_p& x);
        block mEncSeed, mSeed, mshares;
        u64 Threshold;

        std::vector<block> mShares;
        std::vector<block> shares;
        std::vector<block> indexs;

        block computeSecureSharing(span<block> indexs, u64 mthreshold, span<block> shares, ZZ p);
        //void init(u64 n, u64 statSecParam, OtExtSender& otExt, block seed);
        void init(u64 n, u64 Thre, u64 statSecParam, OtExtSender& otExt, block seed);
        void init2(u64 n, u64 Thre, u64 statSecParam, OtExtSender& otExt, block seed);
        void sendInput(std::vector<block>& inputs,int flag);
        void sendInput2(std::vector<block>& inputs, int flag);
        //void sendInput(std::vector<block>& inputs, span<Channel> chl);
        void clientSend(unsigned char* data, int port);
        void serverRecv(int port);
        ZZ ex_gcd(ZZ a, ZZ b, ZZ& x, ZZ& y);//扩展欧几里得 
        ZZ mod_reverse(ZZ a, ZZ n);//ax=1(mod n) 求a的逆元x 
        ZZ Ch_remainder(std::vector<ZZ> M, std::vector<ZZ> P);

    };

} // namespace osuCrypto

//#endif
