// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <flatfile.h>
#include <chain.h>
#include <streams.h>
#include <test/setup_common.h>
#include <util/system.h>

#include <limits>

#ifdef WIN32
#include <io.h>
#include <windows.h>
#include <winioctl.h>
#endif

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(flatfile_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(flatfile_filename)
{
    auto data_dir = SetDataDir("flatfile_test");

    FlatFilePos pos(456, 789);

    FlatFileSeq seq1(data_dir, "a", 16 * 1024);
    BOOST_CHECK_EQUAL(seq1.FileName(pos), data_dir / "a00456.dat");

    FlatFileSeq seq2(data_dir / "a", "b", 16 * 1024);
    BOOST_CHECK_EQUAL(seq2.FileName(pos), data_dir / "a" / "b00456.dat");
}

BOOST_AUTO_TEST_CASE(flatfile_open)
{
    auto data_dir = SetDataDir("flatfile_test");
    FlatFileSeq seq(data_dir, "a", 16 * 1024);

    std::string line1("A purely peer-to-peer version of electronic cash would allow online "
                      "payments to be sent directly from one party to another without going "
                      "through a financial institution.");
    std::string line2("Digital signatures provide part of the solution, but the main benefits are "
                      "lost if a trusted third party is still required to prevent double-spending.");

    size_t pos1 = 0;
    size_t pos2 = pos1 + GetSerializeSize(line1, CLIENT_VERSION);

    // Write first line to file.
    {
        CAutoFile file(seq.Open(FlatFilePos(0, pos1)), SER_DISK, CLIENT_VERSION);
        file << LIMITED_STRING(line1, 256);
    }

    // Attempt to append to file opened in read-only mode.
    {
        CAutoFile file(seq.Open(FlatFilePos(0, pos2), true), SER_DISK, CLIENT_VERSION);
        BOOST_CHECK_THROW(file << LIMITED_STRING(line2, 256), std::ios_base::failure);
    }

    // Append second line to file.
    {
        CAutoFile file(seq.Open(FlatFilePos(0, pos2)), SER_DISK, CLIENT_VERSION);
        file << LIMITED_STRING(line2, 256);
    }

    // Read text from file in read-only mode.
    {
        std::string text;
        CAutoFile file(seq.Open(FlatFilePos(0, pos1), true), SER_DISK, CLIENT_VERSION);

        file >> LIMITED_STRING(text, 256);
        BOOST_CHECK_EQUAL(text, line1);

        file >> LIMITED_STRING(text, 256);
        BOOST_CHECK_EQUAL(text, line2);
    }

    // Read text from file with position offset.
    {
        std::string text;
        CAutoFile file(seq.Open(FlatFilePos(0, pos2)), SER_DISK, CLIENT_VERSION);

        file >> LIMITED_STRING(text, 256);
        BOOST_CHECK_EQUAL(text, line2);
    }

    // Ensure another file in the sequence has no data.
    {
        std::string text;
        CAutoFile file(seq.Open(FlatFilePos(1, pos2)), SER_DISK, CLIENT_VERSION);
        BOOST_CHECK_THROW(file >> LIMITED_STRING(text, 256), std::ios_base::failure);
    }
}

BOOST_AUTO_TEST_CASE(flatfile_allocate)
{
    auto data_dir = SetDataDir("flatfile_test");
    FlatFileSeq seq(data_dir, "a", 100);

    bool out_of_space;

    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, 0), 1, out_of_space), 100);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 0))), 100);
    BOOST_CHECK(!out_of_space);

    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, 99), 1, out_of_space), 0);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 99))), 100);
    BOOST_CHECK(!out_of_space);

    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, 99), 2, out_of_space), 101);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 99))), 200);
    BOOST_CHECK(!out_of_space);
}

BOOST_AUTO_TEST_CASE(flatfile_flush)
{
    auto data_dir = SetDataDir("flatfile_test");
    FlatFileSeq seq(data_dir, "a", 100);

    bool out_of_space;
    seq.Allocate(FlatFilePos(0, 0), 1, out_of_space);

    // Flush without finalize should not truncate file.
    seq.Flush(FlatFilePos(0, 1));
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 1))), 100);

    // Flush with finalize should truncate file.
    seq.Flush(FlatFilePos(0, 1), true);
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 1))), 1);
}

BOOST_AUTO_TEST_CASE(flatfile_large_position_serialization)
{
    for (const uint64_t offset : {uint64_t{0}, uint64_t{123}, (uint64_t{1} << 31), (uint64_t{1} << 32) - 1, (uint64_t{1} << 32) + 123}) {
        FlatFilePos position(1, offset);
        CDataStream stream(SER_DISK, CLIENT_VERSION);
        stream << position;
        if (offset <= std::numeric_limits<uint32_t>::max()) {
            CDataStream legacy(SER_DISK, CLIENT_VERSION);
            int file = 1;
            uint32_t old_offset = offset;
            legacy << VARINT(file, VarIntMode::NONNEGATIVE_SIGNED) << VARINT(old_offset);
            BOOST_CHECK_EQUAL(stream.str(), legacy.str());
        }
        FlatFilePos restored;
        stream >> restored;
        BOOST_CHECK(position == restored);
    }

    CBlockFileInfo info;
    info.nUndoSize = (uint64_t{1} << 32) - 1024;
    info.nUndoSize += 2048;
    BOOST_CHECK_EQUAL(info.nUndoSize, (uint64_t{1} << 32) + 1024);
    CDataStream stream(SER_DISK, CLIENT_VERSION);
    stream << info;
    CBlockFileInfo restored_info;
    stream >> restored_info;
    BOOST_CHECK_EQUAL(restored_info.nUndoSize, info.nUndoSize);

    CDiskBlockIndex index;
    index.nStatus = BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO;
    index.nFile = 1;
    index.nDataPos = 100;
    index.nUndoPos = (uint64_t{1} << 32) + 100;
    index.hashBlockShared = uint256S("01");
    index.hashPOWShared = uint256S("02");
    stream << index;
    CDiskBlockIndex restored_index;
    stream >> restored_index;
    BOOST_CHECK_EQUAL(restored_index.GetUndoPos().nPos, index.nUndoPos);
    BOOST_CHECK_EQUAL(restored_index.GetBlockPos().nPos, index.nDataPos);
}

BOOST_AUTO_TEST_CASE(flatfile_large_sparse_file)
{
    const auto data_dir = SetDataDir("flatfile_large");
    FlatFileSeq seq(data_dir, "rev", 4096);
    {
        CAutoFile file(seq.Open(FlatFilePos(0, 0)), SER_DISK, CLIENT_VERSION);
        BOOST_REQUIRE(!file.IsNull());
#ifdef WIN32
        DWORD returned = 0;
        BOOST_REQUIRE(DeviceIoControl(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file.Get()))), FSCTL_SET_SPARSE,
                                      nullptr, 0, nullptr, 0, &returned, nullptr));
#endif
        file << uint64_t{11};
    }

    const uint64_t offsets[] = {(uint64_t{1} << 31) + 100, (uint64_t{1} << 32) + 100};
    for (const uint64_t offset : offsets) {
        CAutoFile file(seq.Open(FlatFilePos(0, offset)), SER_DISK, CLIENT_VERSION);
        BOOST_REQUIRE(!file.IsNull());
        file << offset;
        BOOST_CHECK_EQUAL(FileTell(file.Get()), offset + sizeof(offset));
    }
    for (const uint64_t offset : offsets) {
        CAutoFile file(seq.Open(FlatFilePos(0, offset), true), SER_DISK, CLIENT_VERSION);
        BOOST_REQUIRE(!file.IsNull());
        uint64_t value = 0;
        file >> value;
        BOOST_CHECK_EQUAL(value, offset);
    }
    {
        CAutoFile file(seq.Open(FlatFilePos(0, 0), true), SER_DISK, CLIENT_VERSION);
        uint64_t value = 0;
        file >> value;
        BOOST_CHECK_EQUAL(value, 11);
    }

    bool out_of_space = false;
    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, (uint64_t{1} << 32) + 4096), 1, out_of_space), 4096);
    BOOST_CHECK(!out_of_space);
    const uint64_t end = offsets[1] + sizeof(uint64_t);
    BOOST_REQUIRE(seq.Flush(FlatFilePos(0, end), true));
    BOOST_CHECK_EQUAL(fs::file_size(seq.FileName(FlatFilePos(0, 0))), end);
    CAutoFile file(seq.Open(FlatFilePos(0, offsets[1]), true), SER_DISK, CLIENT_VERSION);
    uint64_t value = 0;
    file >> value;
    BOOST_CHECK_EQUAL(value, offsets[1]);
}

BOOST_AUTO_TEST_CASE(flatfile_allocation_overflow)
{
    const auto data_dir = SetDataDir("flatfile_overflow");
    FlatFileSeq seq(data_dir, "rev", 4096);
    bool out_of_space = false;
    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, std::numeric_limits<uint64_t>::max()), 1, out_of_space), 0);
    BOOST_CHECK(out_of_space);
    out_of_space = false;
    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, std::numeric_limits<int64_t>::max()), 1, out_of_space), 0);
    BOOST_CHECK(out_of_space);
    out_of_space = false;
    BOOST_CHECK_EQUAL(seq.Allocate(FlatFilePos(0, std::numeric_limits<int64_t>::max()), 0, out_of_space), 0);
    BOOST_CHECK(out_of_space);
    BOOST_CHECK(!fs::exists(seq.FileName(FlatFilePos(0, 0))));
}

BOOST_AUTO_TEST_SUITE_END()
