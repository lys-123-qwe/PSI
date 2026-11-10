
#include "TPsiReceiver.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Crypto/Commit.h"
#include "libOTe/TwoChooseOne/IknpOtExtReceiver.h"
#include "cryptoTools/Common/Log.h"
#include <unordered_map>
#include "cryptoTools/Common/Timer.h"
//#include "ShamirSSScheme.h"
#include "libOTe/Base/BaseOT.h"
#include "cryptoTools/Network/IOService.h"
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
#include "rsdecode_impl.h"
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

#include <set>

namespace osuCrypto
{

    TPsiReceiver::TPsiReceiver()
    {
    }

    TPsiReceiver::~TPsiReceiver()
    {
    }

    ZZ_p TPsiReceiver::func1(ZZ_pX& f, ZZ_p& x)
    {
        ZZ_p a = ZZ_p(0);
        for (long i = 0; i < f.rep.length(); i++)
        {
            a += ZZ_p(coeff(f, i) * power(x, i));
        }
        return a;
    }


    void TPsiReceiver::clientSend(char* data, int port) {
        int sockClient = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addrSrv;
        addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
        addrSrv.sin_family = AF_INET;
        addrSrv.sin_port = htons(port);
        connect(sockClient, (const struct sockaddr*)&addrSrv, sizeof(struct sockaddr_in));
        //char szSendBuf[100] = "this is test";
        //send(sockClient, szSendBuf, strlen(szSendBuf) + 1, 0);
        send(sockClient, data, strlen(data) + 1, 0);
        close(sockClient);
    }

    void TPsiReceiver::serverRecv(int port) {
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
                        index += 1;
                        if (index == 2)
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
    void TPsiReceiver::init(u64 n, u64 thre, u64 statSecParam, OtExtReceiver& otExt, block seed)
    {

        //Timer timer;
        gTimer.setTimePoint("Init.start");
        mMyInputSize = n;
        mTheirInputSize = n;
        mStatSecParam = statSecParam;

        //auto logn = std::log2(n);
        //mNumBins = (n + logn - 1) / logn;
        //mBinSize = logn * std::log2(logn);
        PRNG prng(seed);
        mSeed = prng.get<block>();
        auto myHashSeed = prng.get<block>();


        u64 numHashFunctions;
        gTimer.setTimePoint("Init.params");

        numHashFunctions = 128;
        mBfBitCount = numHashFunctions * 2 * n;

        mHashs.resize(numHashFunctions);

        mRandChoices.resize(mBfBitCount);
        mRandChoices.randomize(prng);
        mMessages.resize(mBfBitCount);
        input_size = n;
        //threshold = 50;
        threshold = thre;
        //cout<<"thre:"<<thre<<endl;

        NTL::ZZ seed1 = conv<ZZ>("241218437866402733620616043852083267111");
        SetSeed(seed1);
        GenPrime(p, 128);
        ZZ_p::init(p);

        ///mHashingSeed = myHashSeed ^ theirHashingSeed;
        mHashingSeed = _mm_set_epi32(1234567, 8942022, 234435, 23987045);
        PRNG hashSeedGen(mHashingSeed);

        for (u64 i = 0; i < mHashs.size(); ++i)
        {
            mHashs[i].Update(hashSeedGen.get<block>());
        }

        gTimer.setTimePoint("Init.done");
        //std::cout << timer;
    }


    void TPsiReceiver::sendInput(std::vector<block>& inputs)
    {
        if (inputs.size() != mMyInputSize)
            throw std::runtime_error(LOCATION);
        u8 hashOut[RandomOracle::HashSize];
        gTimer.setTimePoint("online.start");
        std::vector<u8> bf(mBfBitCount, 0);
        auto start = 0;
        auto end = inputs.size();
        // 离线计算
        clock_t offlineStart = clock();
        std::vector<u64> idxs((end - start) * mHashs.size());
        auto idxIter = idxs.begin();
        for (u64 i = 0; i < inputs.size(); ++i)
        {
            auto& item = inputs[i];
            //std::cout << "input[" << i << "] " << inputs[i] << std::endl;

            for (u64 j = 0; j < mHashs.size(); ++j)
            {
                // copy the hash since its stateful and has the seed in it
                auto hash = mHashs[j];

                hash.Update((u8*)&item, sizeof(block));
                hash.Final(hashOut);

                *idxIter = *(u64*)hashOut % mBfBitCount;
                //std::cout << "recver " << i << " " << j << "   " /*<< garbledBF[idx] << "  " << mask */ << "  " << *idxIter << std::endl;
                //std::cout << "测试bf和gbf是否对应::" << *idxIter << std::endl;
                bf[*idxIter++] = 1;
            }
        }
        //TODO("need real seed");
        // 测试布隆过滤器代码
        block testData = _mm_set_epi32(4253465, 3434565, 234435, 23987046);
     
        u8 hashOut1[RandomOracle::HashSize];
        //for (auto b : bf) {
        //    std::cout << "this is bf: " << b << std::endl;
        //}

        BitVector newBF(mBfBitCount);
        std::mutex finalMtx;
        std::vector<block> garbledBF(mBfBitCount, ZeroBlock);
        std::vector<block> garbledBF_indexs(mBfBitCount);
        std::vector<block> sshares(inputs.size(), ZeroBlock);
        std::vector<block> indexs(inputs.size(), ZeroBlock);
        vec_ZZ_p vec_indexs; //identifier
        vec_indexs.SetLength(inputs.size());
        vec_ZZ_p vec_sshares;
        vec_sshares.SetLength(inputs.size());
        ZZ_pX f;
        f.SetLength(threshold);
        for (u64 i = 0; i < mBfBitCount; i++)
        {
            if (bf[i] == 1)
            {
                newBF[i] = 1;
            }
            else {
                newBF[i] = 0;
            }
        }
        /*for (u64 i = 0; i < 10; i++)
        {
            std::cout << "this is newBF:" << newBF[i] << std::endl;
        }*/
        // 离线计算阶段结束
        clock_t offlineEnd = clock();
        std::cout << "离线计算阶段时间 = " << double(offlineEnd - offlineStart) / CLOCKS_PER_SEC << "s" << std::endl;
        // 测试OT
        IOService ios;

        Channel recverChl1 = Session(ios, "localhost:1212", SessionMode::Client).addChannel();
        //Channel recverChl1_index = Session(ios, "localhost:1211", SessionMode::Client).addChannel();
        Channel recverChl2 = Session(ios, "localhost:1213", SessionMode::Client).addChannel();

        // The code to be run by the OT receiver.
        auto recverThread = std::thread([&]() {
            PRNG prng(sysRandomSeed());

            IknpOtExtReceiver recver1;
            //IknpOtExtReceiver recver1_index;
            IknpOtExtReceiver recver2;

            if (recver1.hasBaseOts() == false)
            {
                recver1.genBaseOts(prng, recverChl1);
            }
            /*if (recver1_index.hasBaseOts() == false)
            {
                recver1_index.genBaseOts(prng, recverChl1_index);
            }*/
            if (recver2.hasBaseOts() == false)
            {
                recver2.genBaseOts(prng, recverChl2);
            }
            // Choose which messages should be received.
            //BitVector choices(otN);
            //choices[0] = 1;
            //choices[1] = 0;
            //choices[2] = 1;
            //choices[3] = 0;
            //choices[4] = 1;
            // Receive the messages
            std::vector<block> messages1(mBfBitCount);
            std::vector<block> messages2(mBfBitCount);
            recver1.receiveChosen(newBF, messages1, prng, recverChl1);
            recver1.receiveChosen(newBF, garbledBF_indexs, prng, recverChl1);

            //std::cout << "this is message1_0" << messages1[0] << std::endl;
            //std::cout << "this is message1_1" << messages1[1] << std::endl;
            //std::cout << "this is message1_2" << messages1[2] << std::endl;
            //std::cout << "this is message1_3" << messages1[3] << std::endl;
            //recver1_index.receiveChosen(newBF,garbledBF_indexs , prng, recverChl1_index);
            recver2.receiveChosen(newBF, messages2, prng, recverChl2);
            //std::cout << "this is message2" << messages2[1] << std::endl;
            for (u64 i = 0; i < garbledBF.size(); ++i)
            {
                garbledBF[i] = messages1[i] ^ messages2[i];
            }
            // messages[i] = sendMessages[i][choices[i]];
            });
        recverThread.join();
        // 份额收发阶段结束
        clock_t shareEnd = clock();
        std::cout << "份额收发阶段时间 = " << double(shareEnd - offlineEnd) / CLOCKS_PER_SEC << "s" << std::endl;
        // 测试接收到的GBF
        //block testData = _mm_set_epi32(1234567, 8942022, 234435, 23987046);
        std::cout << "testData=" << testData << std::endl;

        // 所罗门码重构
        idxIter = idxs.begin();

        std::vector<u64> localIntersection;

        std::set<u64> ss;
        //std::cout << IoStream::lock;
        for (u64 i = start; i < end; ++i)
        {
            ss.clear();
            block mask(ZeroBlock);
            block mask1(ZeroBlock);
            
            std::set<u64> indxs;
            //std::cout << "input[" << i << "] " << inputs[i] << std::endl;
            indxs.clear();
            for (u64 j = 0; j < mHashs.size(); ++j)
            {
                // copy the hash since its stateful
                auto hash = mHashs[j];
                hash.Update(inputs[i]);
                hash.Final(hashOut);
                u64& idx = *(u64*)hashOut;
                idx %= mBfBitCount;
                //std::cout << "测试bf和gbf是否对应::" << idx << std::endl;
                indxs.emplace(idx);
            }
            for (auto idx : indxs)
            {
                mask = mask ^ garbledBF[idx];
                mask1 = mask1 ^ garbledBF_indexs[idx];
            }
            sshares[i] = mask;
            indexs[i] = mask1;
        }


        bool result;
        ZZ x_temp;
        vec_ZZ_p res, err, xx;
        for (int i = 0; i < input_size; i++)
        {
            ZZFromBytes(x_temp, (u8*)&indexs[i], sizeof(block));
            vec_indexs[i] = to_ZZ_p(x_temp);
            ZZFromBytes(x_temp, (u8*)&sshares[i], sizeof(block));
            vec_sshares[i] = to_ZZ_p(x_temp);
        }

        result = gao_interpolate(res, err, vec_indexs, vec_sshares, threshold, input_size);
        std::cout << "this is result::" << result << std::endl;
        gTimer.setTimePoint("online.sharesInterpolated");
        if (result)
        {
            for (int i = 0; i < threshold; i++)
            {
                f[i] = res[i];
            }
            ZZ_p x_temp;
            x_temp = 0;
            ZZ x;
            block yy;
            ZZ_p y_temp = func1(f, x_temp);

            NTL::BytesFromZZ((u8*)&yy, rep(y_temp), sizeof(block));
            cout << "recover:" << yy << endl;

            //------
            hashmy_secret.Update(yy);
            hashmy_secret.Final(hashOut1);
            u64& my_temp = *(u64*)hashOut1;
            // cout << my_temp << endl;
             //cout<<temp<<endl;
            /*if (my_temp != temp)
            {
                throw std::runtime_error(LOCATION);
            }*/

            //------
            ZZ_p y, xx;
            //for (u64 i = start; i < end; ++i)
            for (u64 i = start; i < end - (input_size - threshold) / 2; ++i)
            {
                ZZFromBytes(x, (u8*)&indexs[i], sizeof(block));
                xx = to_ZZ_p(x);
                y = func1(f, xx);
                NTL::BytesFromZZ((u8*)&yy, rep(y), sizeof(block));
                if (eq(sshares[i], yy))
                    localIntersection.push_back(i);
            }

            {
                std::lock_guard<std::mutex> lock(finalMtx);
                if (mIntersection.capacity() == 0)
                {
                    // if it appears that we are first here, lets estimate the size of the intersection and reserve the memory
                    mIntersection.reserve((u64)(localIntersection.size() * 1.2));
                }

                mIntersection.insert(mIntersection.end(), localIntersection.begin(), localIntersection.end());

                std::cout << "mIntersection's size:" << mIntersection.size() << std::endl;
            }
        }
        else
        {
            std::cout << "Not reach threshold value" << std::endl;
        }
        // 交集计算阶段
        clock_t comIsEnd = clock();
        std::cout << "交集计算阶段时间 = " << double(comIsEnd - shareEnd) / CLOCKS_PER_SEC << "s" << std::endl;
        // 总时间
        std::cout << "总时间 = " << double(comIsEnd - offlineStart) / CLOCKS_PER_SEC << "s" << std::endl;


        u64 dataRecv1 = recverChl1.getTotalDataRecv();
        u64 dataRecv2 = recverChl2.getTotalDataRecv();
        std::cout << " Total Comm = " << (dataRecv1 + dataRecv2) / std::pow(2.0, 20) << " MB\n" << endl;
    }
} // namespace osuCrypto
  //#endif

