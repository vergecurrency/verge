// Copyright (c) 2019 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_ELECTRUM_ELECTRUMRPCINFO_H
#define BITCOIN_ELECTRUM_ELECTRUMRPCINFO_H

#include <string>
#include <univalue.h>

namespace electrum
{
static constexpr const char *INDEX_HEIGHT_KEY = "electrs_index_height";
class ElectrumRPCInfo
{
public:
    ElectrumRPCInfo() = default;
    virtual ~ElectrumRPCInfo() = default;

    UniValue GetElectrumInfo() const;
    static void ThrowHelp();

protected:
    // Protected for overriding in unit testing
    virtual int ActiveTipHeight() const;
    virtual bool IsInitialBlockDownload() const;
    virtual bool IsRunning() const;
    virtual std::map<std::string, int64_t> FetchElectrsInfo() const;

private:
    std::string GetStatus(int64_t index_height) const;
    double GetIndexingProgress(int64_t index_height) const;
};

} // ns electrum

#endif
