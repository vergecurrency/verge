// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2020 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <alerter.h>

void DoWarning(const std::string& strWarning)
{
    static bool fWarned = false;
    SetMiscWarning(strWarning);
    if (!fWarned) {
        AlertNotify(strWarning);
        fWarned = true;
    }
}

void AlertNotify(const std::string& strMessage)
{
  uiInterface.NotifyAlertChanged();
  std::string strCmd = gArgs.GetArg("-alertnotify", "");
  if (strCmd.empty()) return;

  // Alert text should be plain ascii coming from a trusted source, but to
  // be safe we first strip anything not in safeChars, then add single quotes around
  // the whole string before passing it to the shell:
  std::string singleQuote("'");
  std::string safeStatus = SanitizeString(strMessage);
  safeStatus = singleQuote+safeStatus+singleQuote;
  boost::replace_all(strCmd, "%s", safeStatus);

  std::thread t(runCommand, strCmd);
  t.detach(); // thread runs free
}