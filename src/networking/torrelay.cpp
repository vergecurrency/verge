// Copyright (c) 2018-2020 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <networking/torrelay.h>
#include <logging.h>

extern "C" {
    int tor_main(int argc, char *argv[]);
}

static boost::thread torRelayThread;

void run_tor_relay() {
    fs::path tor_dir = GetDataDir() / "torrelay";
    fs::create_directory(tor_dir);
    fs::path log_file = tor_dir / "notices.log";

    LogPrintf("TOR Relay thread started.\n");

    /*
     * Log notice file /var/log/tor/notices.log
     * ignore-missing-torrc
     * SocksPort 0
     * RunAsDaemon 1
     * ORPort 9001
     * Nickname xvg
     * ContactInfo contact(at)vergecurrency(at)com [tor-relay.co]
     * DirPort 9030
     * ExitPolicy reject6 *:*, reject *:*
     * RelayBandwidthRate 1 MBits
     * RelayBandwidthBurst 1 MBits
     * AccountingStart month 1 00:00
     * AccountingMax 100 GB
     */
    std::vector<std::string> argv;
    argv.emplace_back("tor");
    argv.push_back("--Log");
    argv.push_back("notice file " + log_file.string());
    argv.push_back("--ignore-missing-torrc");
    argv.push_back("--SocksPort");
    argv.push_back("0");
    argv.push_back("--ORPort");
    argv.push_back("9001");
    argv.push_back("--Nickname");
    argv.push_back("xvg");
    argv.push_back("--ContactInfo");
    argv.push_back("contact(at)vergecurrency(dot)com");
    argv.push_back("--DirPort");
    argv.push_back("9030");
    argv.push_back("--ExitPolicy");
    argv.push_back("reject6 *:*, reject *:*");
    argv.push_back("--RelayBandwidthRate");
    argv.push_back("1 MBits");
    argv.push_back("--RelayBandwidthBurst");
    argv.push_back("1 MBits");
    argv.push_back("--AccountingStart");
    argv.push_back("month 1 00:00");
    argv.push_back("--AccountingMax");
    argv.push_back("100 GB");


    std::vector<char *> argv_c;
    std::transform(argv.begin(), argv.end(), std::back_inserter(argv_c), convertStringToCharArray);

    tor_main(argv_c.size(), &argv_c[0]);
}

void InitalizeTorRelayThread(){
    torRelayThread = boost::thread(boost::bind(&TraceThread<void (*)()>, "torrelay", &StartTorRelay));
}

void StopTorRelay()
{
    torRelayThread.interrupt();
    LogPrintf("Tor Relay thread exited.\n");
}

void StartTorRelay()
{
    RenameThread("TorRelay");

    try
    {
        run_tor_relay();
    }
    catch (std::exception& e) 
    {
        LogPrintf("%s\n", e.what());
    }    
}