
#include "TPsiSender.h"
#include "libOTe/TwoChooseOne/IknpOtExtSender.h"
#include "libOTe/TwoChooseOne/IknpOtExtReceiver.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Crypto/Commit.h"
#include "cryptoTools/Common/Log.h"
#include "libOTe/Base/BaseOT.h"
#include "cryptoTools/Network/IOService.h"
#include <NTL/ZZ_pX.h>
#include <NTL/ZZ_p.h>
#include <NTL/BasicThreadPool.h>
#include <NTL/ZZ.h>
#include <NTL/vec_ZZ_p.h>
#include <cstdlib>
#include <NTL/tools.h>
#include <NTL/vector.h>
#include <NTL/ZZ_p.h>
#include <NTL/vec_ZZ.h>
#include <NTL/vec_ZZ_p.h>
#include <NTL/FFT.h>
#include <NTL/Lazy.h>
#include <NTL/SmartPtr.h>
#include <sys/time.h>
#include <set>
#include <math.h>
// socket
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <fcntl.h>
#include <fcntl.h>
#include <sys/poll.h>
#include<time.h>
using namespace std;
using namespace NTL;
//#include "ShamirSSScheme.h"

namespace osuCrypto
{

    TPsiSender::TPsiSender()
    {
    }

    TPsiSender::~TPsiSender()
    {
    }

    /*block DcwBfPsiSender::computeSecureSharing(span<block> shares)
        {

                ShamirSSScheme ss;

                auto ret = ss.init(shares.size(), shares.size() / 2);
                ss.computeShares(shares);


                NTL::BytesFromGF2X((u8*)&mSharesPrime, ss.mPrime, sizeof(block));

                //std::cout << "send Prime  " << ss.mPrime << std::endl;

                return ret;
        }*/
        /*block DcwBfPsiSender::computeSecureSharing(span<block> indexs, span<block> shares,ZZ p)
        {
            //门限 k=shares.size()/2, 字份额个数 share.size()=inputs.size()
            long n = shares.size();
            long k = shares.size() / 2;

            ZZ_p::init(p);
            ZZ_pX h;
            //cout<<"p"<<p<<endl;
            h.SetLength(k);
            for (long i = 0; i < k; i++)
            {
                NTL::random(h[i]);
            }
            ZZ_p temp;
            temp = 0;
            ZZ_p secret= func(h, temp);
            //cout << "秘密：" << secret << endl;

            vec_ZZ_p a, b;
            a.SetLength(n);
            b.SetLength(n);

            //产生n个子份额
            for (long i = 0; i < n; i++)
            {

                a[i] = rand() % 100000;
                b[i] = func(h, a[i]);
                BytesFromZZ((u8*)&indexs[i], rep(a[i]), sizeof(block));
                BytesFromZZ((u8*)&shares[i], rep(b[i]), sizeof(block));
            }

            block ret;
            NTL::BytesFromZZ((u8 *)&ret, rep(secret), sizeof(block));
            //cout << "ret:" << ret << endl;
            return ret;
        }*/
    double TimeInterval(timeval start, timeval end)
    {
        return double(1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec) / 1000000;
    }
    ZZ_p TPsiSender::func(ZZ_pX& f, ZZ_p& x)
    {
        ZZ_p a = ZZ_p(0);
        for (long i = 0; i < f.rep.length(); i++)
        {
            a += ZZ_p(coeff(f, i) * power(x, i));
        }
        return a;
    }

    block TPsiSender::computeSecureSharing(span<block> indexs, u64 mthreshold, span<block> shares, ZZ p)
    {
        long n = shares.size();
        //long k = 50;
        //cout<<mthreshold<<endl;
        //cout << "n:" << n << ", k:" << k << endl;

        ZZ_p::init(p);

        ZZ_pX h;
        h.SetLength(mthreshold);
        for (long i = 0; i < mthreshold; i++)
        {
            NTL::random(h[i]);
        }
        ZZ_p temp;
        temp = 0;
        ZZ_p secret = func(h, temp);
        //cout << "secret：" << secret << endl;

        vec_ZZ_p x_vec, y_vec; //identifier
        x_vec.SetLength(n);
        y_vec.SetLength(n);
        for (int i = 0; i < n; i++)
            x_vec[i] = i;

        //n secret
        for (long i = 0; i < n; i++)
        {

            y_vec[i] = func(h, x_vec[i]);
            BytesFromZZ((u8*)&indexs[i], rep(x_vec[i]), sizeof(block));
            BytesFromZZ((u8*)&shares[i], rep(y_vec[i]), sizeof(block));
        }

        block ret;
        NTL::BytesFromZZ((u8*)&ret, rep(secret), sizeof(block));
        //cout << "ret:" << ret << endl;
        return ret;
    }

    void TPsiSender::clientSend(unsigned char* data, int port) {
        int sockClient = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addrSrv;
        addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
        addrSrv.sin_family = AF_INET;
        addrSrv.sin_port = htons(port);
        connect(sockClient, (const struct sockaddr*)&addrSrv, sizeof(struct sockaddr_in));
        //char szSendBuf[100] = "this is test";
        //send(sockClient, szSendBuf, strlen(szSendBuf) + 1, 0);
        const char* data1 = reinterpret_cast<const char*>(data);
        send(sockClient, data1, strlen(data1) + 1, 0);
        close(sockClient);
    }

    void TPsiSender::serverRecv(int port) {
        int BACKLOG = 100;
        int index = 0; // 记录当前通信的数量
        int iListenSock = socket(AF_INET, SOCK_STREAM, 0);
        int arrayConnSock[BACKLOG] = { 0 }; // 这个初始化并不好，但其实无大碍
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        inet_aton("0.0.0.0", &addr.sin_addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        int iOpt = 1;
        setsockopt(iListenSock, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt));  // 标配
        bind(iListenSock, (sockaddr*)&addr, sizeof(addr));
        listen(iListenSock, 100);

        int flag = -1;
        pollfd fds[BACKLOG + 1];        // BACKLOG个通信socket和1个监听socket
        memset(fds, flag, sizeof(fds)); // 设置一个标志
        fds[0].fd = iListenSock;        // 监听socket
        fds[0].events = POLLIN;

        int ndfs = 1;
        int timeoutMS = -1; // 永不超时
        int i = 0;
        for (int i = 1; i <= BACKLOG; i++)
        {
            int iRet = poll(fds, ndfs, timeoutMS);
            if (iRet < 0)
            {
                printf("poll error, iRet %d\n", iRet);
                continue;
            }

            if (iRet < 0)
            {
                printf("poll error, iRet %d\n", iRet);
                continue;
            }

            if (fds[0].revents & POLLIN) // fds[0]对应iListenSock， 此处表明检测到有新客户端连接过来
            {
                arrayConnSock[i - 1] = accept(iListenSock, NULL, NULL); // arrayConnSock是通信socket
                fds[i].fd = arrayConnSock[i - 1];
                fds[i].events = POLLIN;
                ndfs++;
                printf("new client came, local fd is [%d]\n", arrayConnSock[i - 1]);
            }

            for (int j = 0; j < BACKLOG; j++)
            {
                char szBuf[1024] = { 0 };
                if (arrayConnSock[j] > 0 && (fds[j + 1].revents & POLLIN)) // 此处检测的是已经链接的socket的读事件
                {
                    int recvLen = recv(arrayConnSock[j], szBuf, sizeof(szBuf) - 1, 0);
                    if (recvLen > 0)
                    {

                        printf("recv data [%s] from local fd [%d] \n", szBuf, arrayConnSock[j]);
                        const unsigned char* data1 = reinterpret_cast<const unsigned char*>(szBuf);
                        long n = (long)recvLen;
                        NTL::ZZ testRcv = NTL::ZZFromBytes(data1, n);
                        std::cout << "this recv data" << testRcv << std::endl;
                        index += 1;
                        if (index == 1)
                        {
                            j = BACKLOG + 1;
                            i = BACKLOG + 1;
                            break;
                        }

                    }
                    else if (0 == recvLen)
                    {
                        close(arrayConnSock[j]);
                        memset(&fds[j + 1], flag, sizeof(pollfd));
                        printf("connection closed fd [%d]\n", arrayConnSock[j]);
                        arrayConnSock[j] = 0;
                    }
                    else
                    {
                        close(arrayConnSock[j]);
                        memset(&fds[j + 1], flag, sizeof(pollfd));
                        printf("recv error, recvLen is %d", recvLen);
                        arrayConnSock[j] = 0;
                    }
                }
            }

        }

        close(iListenSock);
    }
    ZZ TPsiSender::ex_gcd(ZZ a, ZZ b, ZZ& x, ZZ& y)       //扩展欧几里得 
    {
        if (b == ZZ(0))
        {
            x = 1;
            y = 0;
            return a;
        }
        ZZ r = ex_gcd(b, a % b, x, y);
        ZZ t = x;
        x = y;
        y = t - a / b * y;
        return r;
    }

    ZZ TPsiSender::mod_reverse(ZZ a, ZZ n)//ax=1(mod n) 求a的逆元x 
    {
        ZZ d, x, y;
        d = ex_gcd(a, n, x, y);
        if (d == ZZ(1))
            return (x % n + n) % n;
        else
            return ZZ(-1);
    }

    ZZ TPsiSender::Ch_remainder(std::vector<ZZ> M, std::vector<ZZ> P)//主体部分
    {
        //int k = 1;
        ZZ k = conv<ZZ>(1);
        //int b = 0, z = 0;
        ZZ b = conv<ZZ>(0);
        ZZ z = conv<ZZ>(0);
        for (int i = 0; i < mBfBitCount; i++)
        {
            k *= P[i];
            //std::cout << "the pi is: " << P[i] << std::endl;
        }
        std::cout << "the k is: " << k << std::endl;
        for (int i = 0; i < mBfBitCount; i++)
        {
            z = mod_reverse(k / P[i], P[i]);//乘法逆元
            std::cout << "the z is: " << z << std::endl;
            if (z == ZZ(-1)) return ZZ(-1);
            b += (M[i] * (k / P[i]) * z);
        }
        return b % k;
    }

    void TPsiSender::init(u64 n, u64 Thre, u64 statSecParam, OtExtSender& otExt, block seed)
    {

        gTimer.setTimePoint("init.start");
        mN = n;
        Threshold = Thre;
        mStatSecParam = statSecParam;
        //PRNG prng(_mm_set_epi32(4253464, 3434564, 234434, 23987044));
        PRNG prng(seed);
        mSeed = prng.get<block>();
        auto myHashSeed = prng.get<block>();

        /// Commit comm(myHashSeed), theirComm;

        //auto& chl0 = chls[0];
        ///chl0.asyncSend(comm.data(), comm.size());
        ///auto theirCommFutre = chl0.asyncRecv(theirComm.data(), theirComm.size());

        //u64 statSecParam(40);
        u64 numHashFunctions = 128;

        mHashs.resize(numHashFunctions);
        mBfBitCount = n * numHashFunctions * 2;

        mSendOtMessages.resize(mBfBitCount);

        //mDcwOt.init(m, mBinSize * mHashs.size(), mNumBins, mHashs.size(), otExt, chl, prng);

       /// theirCommFutre.get();
       /// chl0.asyncSend(&myHashSeed, sizeof(block));
        ///block theirHashingSeed;
        ///chl0.recv(&theirHashingSeed, sizeof(block));
        gTimer.setTimePoint("init.commitDone");


        NTL::ZZ seed1 = conv<ZZ>("241218437866402733620616043852083267111");
        SetSeed(seed1);
        GenPrime(p, 128);
        ZZ_p::init(p);


        /// mHashingSeed = myHashSeed ^ theirHashingSeed;
        mHashingSeed = _mm_set_epi32(1234567, 8942022, 234435, 23987045);
        PRNG hashSeedGen(mHashingSeed);

        for (u64 i = 0; i < mHashs.size(); ++i)
        {
            mHashs[i].Update(hashSeedGen.get<block>());
        }
    }
    void TPsiSender::init2(u64 n, u64 Thre, u64 statSecParam, OtExtSender& otExt, block seed)
    {

        gTimer.setTimePoint("init.start");
        mN = n;
        Threshold = Thre;
        mStatSecParam = statSecParam;
        //PRNG prng(_mm_set_epi32(4253464, 3434564, 234434, 23987044));
        PRNG prng(seed);
        mSeed = prng.get<block>();
        auto myHashSeed = prng.get<block>();

        /// Commit comm(myHashSeed), theirComm;

        //auto& chl0 = chls[0];
        ///chl0.asyncSend(comm.data(), comm.size());
        ///auto theirCommFutre = chl0.asyncRecv(theirComm.data(), theirComm.size());

        //u64 statSecParam(40);
        u64 numHashFunctions = 128;

        mHashs.resize(numHashFunctions);
        mBfBitCount = n * numHashFunctions * 2;

        mSendOtMessages.resize(mBfBitCount);

        //mDcwOt.init(m, mBinSize * mHashs.size(), mNumBins, mHashs.size(), otExt, chl, prng);

       /// theirCommFutre.get();
       /// chl0.asyncSend(&myHashSeed, sizeof(block));
        ///block theirHashingSeed;
        ///chl0.recv(&theirHashingSeed, sizeof(block));
        gTimer.setTimePoint("init.commitDone");


        NTL::ZZ seed1 = conv<ZZ>("241218437866402733620616043852083267111");
        SetSeed(seed1);
        //GenPrime(p, 128);
        //ZZ_p::init(p);


        /// mHashingSeed = myHashSeed ^ theirHashingSeed;
        mHashingSeed = _mm_set_epi32(1234567, 8942022, 234435, 23987045);
        PRNG hashSeedGen(mHashingSeed);

        for (u64 i = 0; i < mHashs.size(); ++i)
        {
            mHashs[i].Update(hashSeedGen.get<block>());
        }
    }
    void TPsiSender::sendInput(std::vector<block>& inputs, int flag)
    {

        if (inputs.size() != mN)
            throw std::runtime_error(LOCATION);

        gTimer.setTimePoint("online.start");
        //TODO("real seed");

        shares.resize(mN);
        indexs.resize(mN);
        // 离线计算开始
        clock_t offlineStart = clock();
        mshares = computeSecureSharing(indexs, Threshold, shares, p);
        cout << "secret::" << mshares << endl;
        /*      for (int i = 0; i < mN; i++)
              {
                  std::cout << "这是 inputs::" << inputs[i] << std::endl;
                  std::cout << "这是 shares::" << shares[i] << std::endl;
                  std::cout << "这是 indexs::" << indexs[i] << std::endl;
                  std::cout << "******************" << std::endl;
              }*/
        gTimer.setTimePoint("init.ComputedShares");

        PRNG prng(mSeed);

        gTimer.setTimePoint("online.otCorrectionRecv");

        auto start = 0;           // chls.size();
        auto end = inputs.size(); // chls.size();

        std::vector<block> myMasksBuff(mBfBitCount, ZeroBlock);

        u8 hashOut[RandomOracle::HashSize];
        u8 hashOut1[RandomOracle::HashSize];

        std::vector<block> garbledBF(mBfBitCount, ZeroBlock);
        std::vector<block> garbledBF_index(mBfBitCount, ZeroBlock);

        std::set<u64> idxs;

        // 构造混淆布隆过滤器
        for (u64 i = start, k = 0; i < end; ++i, ++k)
        {
            u64 firstFreeIdx(-1);
            auto sum = inputs[i];
            auto sum1 = ZeroBlock;

            //std::cout << "input[" << i << "] " << inputs[i] << std::endl;

            idxs.clear();
            for (u64 j = 0; j < mHashs.size(); ++j)
            {
                // copy the hash since its stateful
                auto hash = mHashs[j];

                hash.Update(inputs[i]);
                hash.Final(hashOut);
                u64& idx = *(u64*)hashOut;

                idx %= mBfBitCount;

                idxs.emplace(idx);
            }

            for (auto idx : idxs)
            {
                if (eq(garbledBF[idx], ZeroBlock))
                {

                    if (firstFreeIdx == u64(-1))
                    {
                        firstFreeIdx = idx;
                    }
                    else
                    {
                        garbledBF[idx] = prng.get<block>();
                        garbledBF_index[idx] = prng.get<block>();
                    }
                }

                //auto mask = garbledBF[idx] ^ mSendOtMessages[idx][otCorrection[idx] ^ 1];
                auto mask = garbledBF[idx];
                sum = sum ^ mask;

                sum1 = sum1 ^ garbledBF_index[idx];

                //std::cout << "sender "<< i << " " << j << "   " << garbledBF[idx] << "  " << mask << "  " << idx << std::endl;
            }

            garbledBF[firstFreeIdx] = sum ^ shares[i];
            garbledBF_index[firstFreeIdx] = sum1 ^ indexs[i];
            //std::cout << "sender " << i << " *   " << garbledBF[firstFreeIdx] << "    " << firstFreeIdx << std::endl;
        }
        /*std::cout << "this is inputs[0]=" << inputs[0] << std::endl;
        std::cout << "this is indexs[0]=" << indexs[0] << std::endl;
        std::cout << "this is shares[0]=" << shares[0] << std::endl;*/
        for (u64 i = 0; i < garbledBF.size(); ++i)
        {
            if (eq(garbledBF[i], ZeroBlock))
                garbledBF[i] = prng.get<block>();

            if (eq(garbledBF_index[i], ZeroBlock))
                garbledBF_index[i] = prng.get<block>();

        }
        //// 测试GBF
        //block testData = _mm_set_epi32(4253465, 3434565, 234435, 23987046);
        //std::cout << "这是testData=" << testData << std::endl;

        auto testSum = ZeroBlock;
        auto testIndex = ZeroBlock;
        std::set<u64> indxs;

        //std::cout << "input[" << i << "] " << inputs[i] << std::endl;
        indxs.clear();
        for (u64 j = 0; j < mHashs.size(); ++j)
        {
            // copy the hash since its stateful
            auto hash = mHashs[j];
            hash.Update(inputs[1]);
            hash.Final(hashOut);
            u64& idx = *(u64*)hashOut;
            idx %= mBfBitCount;
            //std::cout << "测试bf和gbf是否对应::" << idx << std::endl;
            indxs.emplace(idx);
        }
        for (auto idx : indxs)
        {
            testSum = testSum ^ garbledBF[idx];
            testIndex = testIndex ^ garbledBF_index[idx];
        }
        //for (u64 j = 0; j < mHashs.size(); ++j)
        //{
        //    // copy the hash since its stateful and has the seed in it
        //    auto hash = mHashs[j];

        //    hash.Update((u8*)&testData, sizeof(block));
        //    hash.Final(hashOut);

        //    u64& idxIter = *(u64*)hashOut;
        //    idxIter %= mBfBitCount;
        //    testSum = testSum ^ garbledBF[idxIter];
        //    testIndex = testIndex ^ garbledBF_index[idxIter];
        //}


        //std::cout << "这是testData通过GBF异或得到的testSum=" << testSum << std::endl;
        //auto testSumXorShare = testSum ^ shares[1];
        //std::cout << "这是testSum异或上对应的份额shres1得到的=" << testSumXorShare << std::endl;
        //std::cout << "这是testData这个数据通过GBF_index得到的testIndex=" << testIndex << std::endl;
        // 拆分GBF,使它们异或后得到的值等于原来的GBF
        std::vector<block> garbledBF1(mBfBitCount, ZeroBlock);
        std::vector<block> garbledBF2(mBfBitCount, ZeroBlock);
        //std::vector<block> garbledBF3(mBfBitCount, ZeroBlock);
        for (u64 i = 0; i < garbledBF.size(); ++i)
        {
            garbledBF1[i] = prng.get<block>();
            garbledBF2[i] = garbledBF1[i] ^ garbledBF[i];
            //garbledBF3[i] = garbledBF1[i] ^ garbledBF2[i] ^ garbledBF[i];
        }
        //block ceshiRes = garbledBF1[0] ^ garbledBF2[0] ^ garbledBF3[0];
        //std::cout << "this is GBF[0]" << garbledBF[0] << std::endl;
        std::cout << "this is GBF1[0]" << garbledBF1[0] << std::endl;
        std::cout << "this is GBF2[0]" << garbledBF2[0] << std::endl;
        //std::cout << "this is GBF3[0]" << garbledBF3[0] << std::endl;
        //std::cout << "this is ceshi" << ceshiRes << std::endl;

        gTimer.setTimePoint("online.masksComputed");

        //chl.asyncSend(std::move(garbledBF));
        gTimer.setTimePoint("online.masksSent");

        //chl.asyncSend(std::move(garbledBF_index));
        gTimer.setTimePoint("online.indexsSent");

        hash_secret.Update(mshares);
        hash_secret.Final(hashOut1);
        u64& temp = *(u64*)hashOut1;
        // 离线计算阶段结束
        clock_t offlineEnd = clock();
        std::cout << "离线计算阶段时间 = " << double(offlineEnd - offlineStart) / CLOCKS_PER_SEC << "s" << std::endl;

        // 发送GBF份额给其他参与方/接收其他参与方发来的份额
        std::vector<block> recvGBF(mBfBitCount);
        if (flag == 1)
        {

            
            IOService ios1;

            
            // 接收
            Channel recverChl1 = Session(ios1, "localhost:1313", SessionMode::Client).addChannel();
            auto recvDone = recverChl1.asyncRecv((u8*)recvGBF.data(), recvGBF.size() * sizeof(block));
            recvDone.get();
            std::cout << "this is recvGBF" << recvGBF[0] << std::endl;
 
            // 发送
            Channel senderChl1 = Session(ios1, "localhost:1312", SessionMode::Server).addChannel();
            senderChl1.asyncSend(std::move(garbledBF2));
           
            u64 dataRecv1 = recverChl1.getTotalDataRecv();
            u64 dataSend1 = senderChl1.getTotalDataSent();
            std::cout << " Total Comm = " << (dataRecv1 + dataSend1) / std::pow(2.0, 20) << " MB\n" << endl;
   
        }
        else
        {
 
            IOService ios2;
            Channel senderChl2 = Session(ios2, "localhost:1313", SessionMode::Server).addChannel();
            std::vector<std::array<block, 2>> sendGBFP2(mBfBitCount);
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                sendGBFP2[i] = { prng.get<block>(),garbledBF1[i] };
            }
            PRNG prngP2(sysRandomSeed());
            IknpOtExtSender senderP2;
            if (senderP2.hasBaseOts() == false)
            {
                senderP2.genBaseOts(prngP2, senderChl2);
            }
            // Send the messages.
            senderP2.sendChosen(sendGBFP2, prngP2, senderChl2);

            // 接收
            BitVector newBF(mBfBitCount);
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                newBF[i] = 1;
            }
            Channel recverChl2 = Session(ios2, "localhost:1312", SessionMode::Client).addChannel();

            // The code to be run by the OT receiver.
            auto recverThread = std::thread([&]() {
                PRNG prng22(sysRandomSeed());

                IknpOtExtReceiver recver2;
                if (recver2.hasBaseOts() == false)
                {
                    recver2.genBaseOts(prng22, recverChl2);
                }
                std::vector<block> messages1(mBfBitCount);

                recver2.receiveChosen(newBF, messages1, prng22, recverChl2);
                std::cout << "this is message1" << messages1[0] << std::endl;


                });
            recverThread.join();
        }



        // 和P0进行OT扩展的阶段
        IOService ios;
        Channel senderChl;

        std::vector<std::array<block, 2>> sendMessages(mBfBitCount);
        std::vector<std::array<block, 2>> sendMessages2(mBfBitCount);
        std::vector<std::array<u64, 2>> hashS(1);
        if (flag == 1)
        {

            senderChl = Session(ios, "localhost:1212", SessionMode::Server).addChannel();
            // Choose which messages should be sent.
            //sendMessages[0] = { toBlock(21), toBlock(19) };
            //sendMessages[1] = { toBlock(54), toBlock(33) };
            //sendMessages[2] = { toBlock(54), toBlock(33) };
            //sendMessages[3] = { toBlock(54), toBlock(33) };
            //sendMessages[4] = { toBlock(54), toBlock(33) };
            std::vector<block> ceshiGBF(mBfBitCount, ZeroBlock);
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                sendMessages[i] = { prng.get<block>(),garbledBF1[i] ^ recvGBF[i] };
                ceshiGBF[i] = garbledBF1[i] ^ recvGBF[i];
            }

            /*std::cout << "0:" << ceshiGBF[0] << std::endl;
            std::cout << "1:" << ceshiGBF[1] << std::endl;
            std::cout << "2:" << ceshiGBF[2] << std::endl;
            std::cout << "3:" << ceshiGBF[3] << std::endl;*/
        }
        else
        {
            senderChl = Session(ios, "localhost:1213", SessionMode::Server).addChannel();
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                sendMessages[i] = { prng.get<block>(),garbledBF1[i] ^ recvGBF[i] };
            }
        }
        PRNG prng1(sysRandomSeed());
        IknpOtExtSender sender;
        if (sender.hasBaseOts() == false)
        {
            sender.genBaseOts(prng1, senderChl);
        }
        for (u64 i = 0; i < mBfBitCount; i++)
        {
            sendMessages2[i] = { prng.get<block>(),garbledBF_index[i] };
        }
        // 和P0进行OT
        // Send the messages.
        sender.sendChosen(sendMessages, prng1, senderChl);
        sender.sendChosen(sendMessages2, prng1, senderChl);
        // 份额收发阶段结束
        clock_t shareEnd = clock();
        std::cout << "份额收发阶段时间 = " << double(shareEnd - offlineEnd) / CLOCKS_PER_SEC << "s" << std::endl;
        // 向P0发送GBF_index
        //std::vector<std::array<block, 2>> sendMessages2(mBfBitCount);
        //for (u64 i = 0; i < mBfBitCount; i++)
        //{
        //    sendMessages2[i] = { prng.get<block>(),garbledBF_index[i] };      
        //}
        //Channel senderChl2 = Session(ios, "localhost:1211", SessionMode::Server).addChannel();
        //IknpOtExtSender sender2;
        //if (sender2.hasBaseOts() == false)
        //{
        //    sender2.genBaseOts(prng1, senderChl2);
        //}
        //
        //// Send the messages.
        //sender2.sendChosen(sendMessages2, prng1, senderChl2);
    }
    void TPsiSender::sendInput2(std::vector<block>& inputs, int flag)
    {

        if (inputs.size() != mN)
            throw std::runtime_error(LOCATION);

        gTimer.setTimePoint("online.start");
        //TODO("real seed");
        PRNG prng(mSeed);
        //auto t = 0;
        //auto& chl = chls[t];

        //BitVector otCorrection(mBfBitCount);
        //chl.recv(otCorrection);

        gTimer.setTimePoint("online.otCorrectionRecv");

        //auto routine = [&](u64 t)
        //{
        auto start = 0;           // chls.size();
        auto end = inputs.size(); // chls.size();

        std::vector<block> myMasksBuff(mBfBitCount, ZeroBlock);

        u8 hashOut[RandomOracle::HashSize];
        u8 hashOut1[RandomOracle::HashSize];

        std::vector<block> garbledBF(mBfBitCount, ZeroBlock);


        std::set<u64> idxs;


        for (u64 i = start, k = 0; i < end; ++i, ++k)
        {
            u64 firstFreeIdx(-1);
            auto sum = inputs[i];
            auto sum1 = ZeroBlock;

            //std::cout << "input[" << i << "] " << inputs[i] << std::endl;

            idxs.clear();
            for (u64 j = 0; j < mHashs.size(); ++j)
            {
                // copy the hash since its stateful
                auto hash = mHashs[j];

                hash.Update(inputs[i]);
                hash.Final(hashOut);
                u64& idx = *(u64*)hashOut;

                idx %= mBfBitCount;

                idxs.emplace(idx);
            }

            for (auto idx : idxs)
            {
                if (eq(garbledBF[idx], ZeroBlock))
                {

                    if (firstFreeIdx == u64(-1))
                    {
                        firstFreeIdx = idx;
                    }
                    else
                    {
                        garbledBF[idx] = prng.get<block>();

                    }
                }

                //auto mask = garbledBF[idx] ^ mSendOtMessages[idx][otCorrection[idx] ^ 1];
                auto mask = garbledBF[idx];
                sum = sum ^ mask;

                //std::cout << "sender "<< i << " " << j << "   " << garbledBF[idx] << "  " << mask << "  " << idx << std::endl;
            }

            garbledBF[firstFreeIdx] = sum;

            //std::cout << "sender " << i << " *   " << garbledBF[firstFreeIdx] << "    " << firstFreeIdx << std::endl;
        }


        for (u64 i = 0; i < garbledBF.size(); ++i)
        {
            if (eq(garbledBF[i], ZeroBlock))
                garbledBF[i] = prng.get<block>();

        }
        // 测试GBF
        block testData = _mm_set_epi32(4253465, 3434565, 234435, 23987046);
        std::cout << "这是testData=" << testData << std::endl;

        //u64& idxIter = *(u64*) 0;
        auto testSum = ZeroBlock;
        for (u64 j = 0; j < mHashs.size(); ++j)
        {
            // copy the hash since its stateful and has the seed in it
            auto hash = mHashs[j];

            hash.Update((u8*)&testData, sizeof(block));
            hash.Final(hashOut);

            u64& idxIter = *(u64*)hashOut;
            idxIter %= mBfBitCount;

            testSum = testSum ^ garbledBF[idxIter];
        }
        std::cout << "测试testData通过GBF异或得到的值:" << testSum << std::endl;
        // 拆分GBF,使它们异或后得到的值等于原来的GBF
        std::vector<block> garbledBF1(mBfBitCount, ZeroBlock);
        std::vector<block> garbledBF2(mBfBitCount, ZeroBlock);
        //std::vector<block> garbledBF3(mBfBitCount, ZeroBlock);
        for (u64 i = 0; i < garbledBF.size(); ++i)
        {
            garbledBF1[i] = prng.get<block>();
            garbledBF2[i] = garbledBF1[i] ^ garbledBF[i];
            //garbledBF3[i] = garbledBF1[i] ^ garbledBF2[i] ^ garbledBF[i];
        }

        gTimer.setTimePoint("online.masksComputed");

        //chl.asyncSend(std::move(garbledBF));
        gTimer.setTimePoint("online.masksSent");

        //chl.asyncSend(std::move(garbledBF_index));
        gTimer.setTimePoint("online.indexsSent");

        // socket发份额阶段
        std::vector<block> recvGBF(mBfBitCount);
        if (flag == 1)
        {

           
            IOService ios1;
            BitVector newBF(mBfBitCount);
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                newBF[i] = 1;
            }

            // 接收
            Channel recverChl1 = Session(ios1, "localhost:1313", SessionMode::Client).addChannel();

            // The code to be run by the OT receiver.
            auto recverThread = std::thread([&]() {
                PRNG prng11(sysRandomSeed());
                IknpOtExtReceiver recver1;

                if (recver1.hasBaseOts() == false)
                {
                    recver1.genBaseOts(prng11, recverChl1);
                }
                // Receive the messages

                recver1.receiveChosen(newBF, recvGBF, prng11, recverChl1);
                std::cout << "this is recvGBF" << recvGBF[0] << std::endl;
                });
            recverThread.join();
            // 发送
            Channel senderChl1 = Session(ios1, "localhost:1312", SessionMode::Server).addChannel();
            std::vector<std::array<block, 2>> sendGBFP1(mBfBitCount);
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                sendGBFP1[i] = { prng.get<block>(),garbledBF2[i] };
            }
            PRNG prngP1(sysRandomSeed());
            IknpOtExtSender senderP1;
            if (senderP1.hasBaseOts() == false)
            {
                senderP1.genBaseOts(prngP1, senderChl1);
            }
            // Send the messages.
            senderP1.sendChosen(sendGBFP1, prngP1, senderChl1);
        }
        else
        {
            //unsigned char szSendBuf[128];
            ////char szSendBuf[100] = "this is P2";
            //NTL::ZZ test = conv<ZZ>("241218437866402733620616043852083267111");
            //std::cout << "this is ZZ test:" << test << endl;
            //long n = NTL::NumBits(test);
            //std::cout << "this is ZZ test long:" << n << endl;
            //NTL::BytesFromZZ(szSendBuf, test, n);
            //std::cout << "this is szSendBuf:" << szSendBuf << endl;
            //clientSend(szSendBuf, 8888);
            //serverRecv(8889);
            IOService ios2;
            Channel senderChl2 = Session(ios2, "localhost:1313", SessionMode::Server).addChannel();
            senderChl2.asyncSend(std::move(garbledBF1));
            //std::vector<std::array<block, 2>> sendGBFP2(mBfBitCount);
            //for (u64 i = 0; i < mBfBitCount; i++)
            //{
            //    sendGBFP2[i] = { prng.get<block>(),garbledBF1[i] };
            //}
            //PRNG prngP2(sysRandomSeed());
            //IknpOtExtSender senderP2;
            //if (senderP2.hasBaseOts() == false)
            //{
            //    senderP2.genBaseOts(prngP2, senderChl2);
            //}
            //// Send the messages.
            //senderP2.sendChosen(sendGBFP2, prngP2, senderChl2);

            // 接收
            /*BitVector newBF(mBfBitCount);
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                newBF[i] = 1;
            }*/
            Channel recverChl2 = Session(ios2, "localhost:1312", SessionMode::Client).addChannel();
            auto recvDone = recverChl2.asyncRecv((u8*)recvGBF.data(), recvGBF.size() * sizeof(block));
            recvDone.get();
            std::cout << "this is recvGBF" << recvGBF[0] << std::endl;
            // The code to be run by the OT receiver.
            //auto recverThread = std::thread([&]() {
            //    PRNG prng22(sysRandomSeed());

            //    IknpOtExtReceiver recver2;
            //    if (recver2.hasBaseOts() == false)
            //    {
            //        recver2.genBaseOts(prng22, recverChl2);
            //    }
            //    //std::vector<block> messages1(mBfBitCount);

            //    recver2.receiveChosen(newBF, recvGBF, prng22, recverChl2);
            //    std::cout << "this is recvGBF" << recvGBF[0] << std::endl;


            //    });
            //recverThread.join();
        }



        IOService ios;
        Channel senderChl;

        std::vector<std::array<block, 2>> sendMessages(mBfBitCount);
        if (flag == 1)
        {

            senderChl = Session(ios, "localhost:1212", SessionMode::Server).addChannel();
            // Choose which messages should be sent.

            for (u64 i = 0; i < mBfBitCount; i++)
            {
                sendMessages[i] = { prng.get<block>(),garbledBF[i] };
            }
        }
        else
        {
            senderChl = Session(ios, "localhost:1213", SessionMode::Server).addChannel();
            for (u64 i = 0; i < mBfBitCount; i++)
            {
                sendMessages[i] = { prng.get<block>(),garbledBF2[i] ^ recvGBF[i] };
            }
        }
        PRNG prng1(sysRandomSeed());
        IknpOtExtSender sender;
        if (sender.hasBaseOts() == false)
        {
            sender.genBaseOts(prng1, senderChl);
        }

        // Send the messages.
        sender.sendChosen(sendMessages, prng1, senderChl);
    }
} // namespace osuCrypto

