// Copyright (c) 2019 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ELECTRUM_ELECTRUMSERVER_H
#define ELECTRUM_ELECTRUMSERVER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

class SubProcess;

namespace electrum {

struct Process;

class ElectrumServer {
public:
    static ElectrumServer& Instance();
    bool Start(int rpcport, const std::string& network);

    // for allow overriding path/args for unit testing
    bool Start(const std::string& path, const std::vector<std::string>& args);

    void Stop();
    bool IsRunning() const;
    ~ElectrumServer();

private:
    ElectrumServer();

    mutable std::mutex process_cs;
    std::unique_ptr<SubProcess> process;
    std::thread process_thread;

    std::atomic<bool> started;
};

} // ns electrum

#endif
