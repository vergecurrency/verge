// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/handler.h>

#include <util/memory.h>

#include <boost/signals2/connection.hpp>
#include <utility>

namespace interfaces {
namespace {

class HandlerImpl : public Handler
{
public:
    HandlerImpl(boost::signals2::connection connection) : m_connection(std::move(connection)) {}

    void disconnect() override { m_connection.disconnect(); }

    boost::signals2::scoped_connection m_connection;
};

} // namespace

std::unique_ptr<Handler> MakeHandler(boost::signals2::connection connection)
{
    return MakeUnique<HandlerImpl>(std::move(connection));
}

} // namespace interfaces
