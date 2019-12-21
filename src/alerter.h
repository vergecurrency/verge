#ifndef VERGE_ALERTER_H
#define VERGE_ALERTER_H

#include <warnings.h>
#include <ui_interface.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>


void DoWarning(const std::string& strWarning);

void AlertNotify(const std::string& strMessage);

#endif // VERGE_ALERTER_H