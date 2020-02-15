// Copyright (c) 2018-2020 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <networking/torcontroller.h>
#include <logging.h>

extern "C" {
    int tor_main(int argc, char *argv[]);
}

static boost::thread torControllerThread;

char *convert_str(const std::string &s) {
    char *pc = new char[s.size()+1];
    std::strcpy(pc, s.c_str());
    return pc;
}

void run_tor() {
    fs::path tor_dir = GetDataDir() / "tor";
    fs::create_directory(tor_dir);
    fs::path log_file = tor_dir / "tor.log";

    LogPrintf("TOR thread started.\n");

    std::vector<std::string> argv;
    argv.emplace_back("tor");
    argv.push_back("--Log");
    argv.push_back("notice file " + log_file.string());
    argv.push_back("--SocksPort");

    std::string onion_port = std::to_string(DEFAULT_TOR_PORT);
    if (gArgs.IsArgSet("-torsocksport") && gArgs.GetArg("-torsocksport", "0") != "0")
    {
        onion_port = gArgs.GetArg("-torsocksport", "0");
    }

    argv.push_back(onion_port);
    argv.push_back("--ignore-missing-torrc");
    argv.push_back("-f");
    argv.push_back((tor_dir / "torrc").string());
    argv.push_back("--HiddenServiceDir");
    argv.push_back((tor_dir / "onion").string());
    argv.push_back("--ControlPort");
    argv.push_back(std::to_string(DEFAULT_TOR_CONTROL_PORT));
    argv.push_back("--HiddenServiceVersion");
    argv.push_back("3");
    argv.push_back("--HiddenServicePort");
    argv.push_back("21102");
    argv.push_back("--CookieAuthentication");
    argv.push_back("1");

    std::vector<char *> argv_c;
    std::transform(argv.begin(), argv.end(), std::back_inserter(argv_c), convert_str);

    tor_main(argv_c.size(), &argv_c[0]);
}

void InitalizeTorThread(){
    torControllerThread = boost::thread(boost::bind(&TraceThread<void (*)()>, "torcontroller", &StartTorController));
}

void StopTorController()
{
    torControllerThread.interrupt();
    LogPrintf("Tor Controller thread exited.\n");
}

void StartTorController()
{
    RenameThread("TorController");

    try
    {
        run_tor();
    }
    catch (std::exception& e) 
    {
        LogPrintf("%s\n", e.what());
    }    
}