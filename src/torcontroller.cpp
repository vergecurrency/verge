#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <torcontroller.h>

#include <boost/filesystem.hpp>
#include <util/system.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <addrdb.h>
#include <addrman.h>
#include <amount.h>
#include <bloom.h>
#include <compat.h>
#include <crypto/siphash.h>
#include <hash.h>
#include <limitedmap.h>
#include <netaddress.h>
#include <policy/feerate.h>
#include <protocol.h>
#include <random.h>
#include <streams.h>
#include <sync.h>
#include <uint256.h>
#include <threadinterrupt.h>
#include <crypto/common.h>
#include <crypto/sha256.h>

#include <atomic>
#include <deque>
#include <stdint.h>
#include <thread>
#include <memory>
#include <condition_variable>
#include <chainparams.h>
#include <clientversion.h>
#include <consensus/consensus.h>
#include <crypto/common.h>
#include <crypto/sha256.h>
#include <primitives/transaction.h>
#include <netbase.h>
#include <scheduler.h>
#include <ui_interface.h>
#include <util/strencodings.h>

extern "C" {
    int tor_main(int argc, char *argv[]);
}

char *convert_str(const std::string &s) {
    char *pc = new char[s.size()+1];
    std::strcpy(pc, s.c_str());
    return pc;
}

void run_tor() {
    std::string clientTransportPlugin;
    // struct stat sb;
    fs::path tor_dir = GetDataDir() / "tor";
    fs::create_directory(tor_dir);
    fs::path log_file = tor_dir / "tor.log";
    fs::path torrc_file = tor_dir / "torrc";
    // std::regex bridge_regex("^Bridge obfs4");
    // int bridgeNum = 0;
    std::ifstream torrc_stream;
    std::string line;
    std::string obfs4proxy_path;

    printf("TOR thread started.\n");
    
    /*torrc_stream.open(torrc_file.string().c_str());
    while (getline(torrc_stream, line)) {
        if (regex_search (line, bridge_regex)) {
            ++bridgeNum;
        }
    }
    torrc_stream.close();

    if (stat("/usr/bin/obfs4proxy", &sb) == 0 && sb.st_mode & S_IXUSR) {
        obfs4proxy_path = "/usr/bin/obfs4proxy";
    }
    if (stat("/usr/local/bin/obfs4proxy", &sb) == 0 && sb.st_mode & S_IXUSR) {
        obfs4proxy_path = "/usr/local/bin/obfs4proxy";
    }
    if (stat("c:\\bin\\obfs4proxy.exe", &sb) == 0 && sb.st_mode & S_IXUSR) {
        obfs4proxy_path = "c:\\bin\\obfs4proxy.exe";
    }

    if ((bridgeNum > 0) && (!obfs4proxy_path.empty())) {
            clientTransportPlugin = "obfs4 exec " + obfs4proxy_path;
    }*/

    std::vector<std::string> argv;
    argv.push_back("tor");
    argv.push_back("--Log");
    argv.push_back("notice file " + log_file.string());
    argv.push_back("--SocksPort");

    std::string onion_port = std::to_string(DEFAULT_TOR_PORT);
    if (gArgs.IsArgSet("-torsocksport") && gArgs.GetArg("-torsocksport", "0") != "0")
    {
        onion_port = gArgs.GetArg("-torsocksport", "0");
    }

    argv.push_back(onion_port.c_str());
    argv.push_back("--ignore-missing-torrc");
    argv.push_back("-f");
    argv.push_back((tor_dir / "torrc").string());
    argv.push_back("--HiddenServiceDir");
    argv.push_back((tor_dir / "onion").string());
    argv.push_back("--HiddenServicePort");
    argv.push_back("21102");
    argv.push_back("--CookieAuthentication");
    argv.push_back("1");
    argv.push_back("--ControlPort");
    argv.push_back(std::to_string(DEFAULT_TOR_CONTROL_PORT));

    if(!clientTransportPlugin.empty()){
      printf("Using OBFS4.\n");
      argv.push_back("--ClientTransportPlugin");
      argv.push_back(clientTransportPlugin);
      argv.push_back("--UseBridges");
      argv.push_back("1");
    }
    else {
      printf("No OBFS4 found, not using it.\n");
    }

    std::vector<char *> argv_c;
    std::transform(argv.begin(), argv.end(), std::back_inserter(argv_c), convert_str);

    tor_main(argv_c.size(), &argv_c[0]);
}

void StartTor(void* parg)
{
    // Make this thread recognisable as the tor thread
    RenameThread("onion");

    try
    {
        run_tor();
    }
    catch (std::exception& e) {
        printf("%s\n", e.what());
    }

    printf("Onion thread exited.\n");
}