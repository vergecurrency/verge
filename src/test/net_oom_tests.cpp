// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <hash.h>
#include <net.h>
#include <netbase.h>
#include <test/setup_common.h>
#include <util/time.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstring>
#include <thread>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace {

class SocketEvents : public NetEventsInterface
{
public:
    bool ProcessMessages(CNode*, std::atomic<bool>&) override { return false; }
    bool SendMessages(CNode*, std::atomic<bool>&) override { return false; }
    void InitializeNode(CNode*) override {}
    void FinalizeNode(NodeId, bool&) override {}
};

struct SocketPair
{
    SOCKET listener{INVALID_SOCKET};
    SOCKET sender{INVALID_SOCKET};
    SOCKET receiver{INVALID_SOCKET};

    ~SocketPair()
    {
        if (listener != INVALID_SOCKET) CloseSocket(listener);
        if (sender != INVALID_SOCKET) CloseSocket(sender);
        if (receiver != INVALID_SOCKET) CloseSocket(receiver);
    }

    void Open()
    {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        BOOST_REQUIRE(listener != INVALID_SOCKET);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        BOOST_REQUIRE_EQUAL(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
        BOOST_REQUIRE_EQUAL(listen(listener, 1), 0);
        socklen_t size = sizeof(address);
        BOOST_REQUIRE_EQUAL(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size), 0);
        sender = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        BOOST_REQUIRE(sender != INVALID_SOCKET);
        BOOST_REQUIRE_EQUAL(connect(sender, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
        receiver = accept(listener, nullptr, nullptr);
        BOOST_REQUIRE(receiver != INVALID_SOCKET);
        BOOST_REQUIRE(SetSocketNonBlocking(receiver, true));
    }

    void Send(const std::vector<unsigned char>& bytes)
    {
        size_t sent = 0;
        while (sent < bytes.size()) {
            const int count = send(sender, reinterpret_cast<const char*>(bytes.data()) + sent, bytes.size() - sent, MSG_NOSIGNAL);
            BOOST_REQUIRE(count > 0);
            sent += count;
        }
    }
};

template <typename Predicate>
bool WaitFor(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

std::vector<unsigned char> MakeMessage()
{
    std::vector<unsigned char> payload(4096, 0);
    CMessageHeader header(Params().MessageStart(), "test", payload.size());
    const uint256 hash = Hash(payload.begin(), payload.end());
    std::memcpy(header.pchChecksum, hash.begin(), CMessageHeader::CHECKSUM_SIZE);
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << header;
    stream.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    return {stream.begin(), stream.end()};
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(net_oom_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(urgent_data_cannot_bypass_receive_pause)
{
    SocketPair sockets;
    sockets.Open();
    SocketEvents events;
    CConnman connman(0, 0);
    CConnman::Options options;
    options.m_msgproc = &events;
    options.nReceiveFloodSize = 1024;
    options.nSendBufferMaxSize = 1024;
    connman.Init(options);

    auto* node = new CNode(0, NODE_NETWORK, 0, sockets.receiver, CAddress(), 0, 0, CAddress());
    sockets.receiver = INVALID_SOCKET;
    CConnmanTest::StartSocketHandler(connman, *node);

    const auto message = MakeMessage();
    sockets.Send(message);
    BOOST_REQUIRE(WaitFor([&] { return node->fPauseRecv.load(); }));
    size_t paused_size;
    {
        LOCK(node->cs_vProcessMsg);
        paused_size = node->nProcessQueueSize;
    }
    BOOST_REQUIRE(paused_size > options.nReceiveFloodSize);

    for (int i = 0; i < 4; ++i) {
        sockets.Send(message);
    }
    BOOST_REQUIRE_EQUAL(send(sockets.sender, "!", 1, MSG_OOB | MSG_NOSIGNAL), 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    {
        LOCK(node->cs_vProcessMsg);
        BOOST_CHECK_EQUAL(node->nProcessQueueSize, paused_size);
        node->vProcessMsg.clear();
        node->nProcessQueueSize = 0;
        node->fPauseRecv = false;
    }
    BOOST_CHECK(WaitFor([&] { return node->fPauseRecv.load(); }));
}

BOOST_AUTO_TEST_SUITE_END()
