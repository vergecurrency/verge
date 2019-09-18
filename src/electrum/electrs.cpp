// Copyright (c) 2019 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "electrum/electrs.h"
#include "util.h"
#include "utilhttp.h"
#include "utilprocess.h"

#include <map>
#include <regex>
#include <sstream>

#include <boost/filesystem.hpp>

static std::string monitoring_port() { return GetArg("-electrum.monitoring.port", "4224"); }
static std::string monitoring_host() { return GetArg("-electrum.monitoring.host", "127.0.0.1"); }
static std::string rpc_host() { return GetArg("-electrum.host", "127.0.0.1"); }
static std::string rpc_port(const std::string &network)
{
    std::map<std::string, std::string> portmap = {{"main", "50001"}, {"test", "60001"}, {"regtest", "60401"}}; //50001 is the default electrum port 

    auto defaultPort = portmap.find(network);
    if (defaultPort == end(portmap))
    {
        std::stringstream ss;
        ss << "Electrum server does not support '" << network << "' network.";
        throw std::invalid_argument(ss.str());
    }

    return GetArg("-electrum.port", defaultPort->second);
}
namespace electrum
{
std::string electrs_path()
{
    // look for electrs in same path as verged
    boost::filesystem::path verged_dir(this_process_path());
    verged_dir = verged_dir.remove_filename();

    auto default_path = verged_dir / "electrs";
    const std::string path = GetArg("-electrum.exec", default_path.string());

    if (path.empty())
    {
        throw std::runtime_error("Path to electrum server executable not found. "
                                 "You can specify full path with -electrum.exec");
    }
    if (!boost::filesystem::exists(path))
    {
        std::stringstream ss;
        ss << "Cannot find electrum executable at " << path;
        throw std::runtime_error(ss.str());
    }
    return path;
}

//! Arguments to start electrs server with
std::vector<std::string> electrs_args(int rpcport, const std::string &network)
{
    std::vector<std::string> args;

    if (Logging::LogAcceptCategory(ELECTRUM))
    {
        // increase verboseness when electrum logging is enabled
        args.push_back("-vvvv");
    }

    // address to verged rpc interface
    {
        rpcport = GetArg("-rpcport", rpcport);
        std::stringstream ss;
        ss << "--daemon-rpc-addr=" << GetArg("-electrum.daemon.host", "127.0.0.1") << ":" << rpcport;
        args.push_back(ss.str());
    }

    args.push_back("--electrum-rpc-addr=" + rpc_host() + ":" + rpc_port(network));

    // verged data dir (for cookie file)
    args.push_back("--daemon-dir=" + GetDataDir(false).string());

    // Use rpc interface instead of attempting to parse *blk files
    args.push_back("--jsonrpc-import");

    // Where to store electrs database files.
    const std::string defaultDir = (GetDataDir() / "electrs").string();
    args.push_back("--db-dir=" + GetArg("-electrumdir", defaultDir));

    // Tell electrs what network we're on
    const std::map<std::string, std::string> netmapping = {
        {"main", "mainnet"}, {"test", "testnet"}, {"regtest", "regtest"}};
    if (!netmapping.count(network))
    {
        std::stringstream ss;
        ss << "Electrum server does not support '" << network << "' network.";
        throw std::invalid_argument(ss.str());
    }
    args.push_back("--network=" + netmapping.at(network));
    args.push_back("--monitoring-addr=" + monitoring_host() + ":" + monitoring_port());

    if (!GetArg("-rpcpassword", "").empty())
    {
        args.push_back("--cookie=" + GetArg("-rpcuser", "") + ":" + GetArg("-rpcpassword", ""));
    }

    // max txs to look up per address
    args.push_back("--txid-limit=" + GetArg("-electrum.addr.limit", "1000"));

    return args;
}

std::map<std::string, int64_t> fetch_electrs_info()
{
    if (!GetBoolArg("-electrum", false))
    {
        throw std::runtime_error("Electrum server is disabled");
    }

    std::stringstream infostream = http_get(monitoring_host(), std::stoi(monitoring_port()), "/");

    const std::regex keyval("^([a-z_]+)\\s(\\d+)\\s*$");
    std::map<std::string, int64_t> info;
    std::string line;
    std::smatch match;
    while (std::getline(infostream, line, '\n'))
    {
        if (!std::regex_match(line, match, keyval))
        {
            continue;
        }
        try
        {
            info[match[1].str()] = std::stol(match[2].str());
        }
        catch (const std::exception &e)
        {
            LOG(ELECTRUM, "%s error: %s", __func__, e.what());
        }
    }
    return info;
}

} // ns electrum
