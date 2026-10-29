// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <limits>
#include <stdexcept>

#include <flatfile.h>
#include <logging.h>
#include <tinyformat.h>
#include <util/system.h>

FlatFileSeq::FlatFileSeq(fs::path dir, const char* prefix, uint64_t chunk_size) :
    m_dir(std::move(dir)),
    m_prefix(prefix),
    m_chunk_size(chunk_size)
{
    if (chunk_size == 0 || chunk_size > uint64_t{std::numeric_limits<int64_t>::max()}) {
        throw std::invalid_argument("chunk_size is outside the supported file range");
    }
}

std::string FlatFilePos::ToString() const
{
    return strprintf("FlatFilePos(nFile=%i, nPos=%i)", nFile, nPos);
}

fs::path FlatFileSeq::FileName(const FlatFilePos& pos) const
{
    return m_dir / strprintf("%s%05u.dat", m_prefix, pos.nFile);
}

FILE* FlatFileSeq::Open(const FlatFilePos& pos, bool read_only)
{
    if (pos.IsNull()) {
        return nullptr;
    }
    fs::path path = FileName(pos);
    fs::create_directories(path.parent_path());
    FILE* file = fsbridge::fopen(path, read_only ? "rb": "rb+");
    if (!file && !read_only)
        file = fsbridge::fopen(path, "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return nullptr;
    }
    if (pos.nPos && !FileSeek(file, pos.nPos)) {
        LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
        fclose(file);
        return nullptr;
    }
    return file;
}

uint64_t FlatFileSeq::Allocate(const FlatFilePos& pos, uint64_t add_size, bool& out_of_space)
{
    out_of_space = false;

    const uint64_t max_position = std::numeric_limits<int64_t>::max();
    if (pos.nPos > max_position || add_size > max_position - pos.nPos) {
        out_of_space = true;
        return 0;
    }
    const uint64_t end = pos.nPos + add_size;
    const uint64_t n_old_chunks = pos.nPos / m_chunk_size + (pos.nPos % m_chunk_size != 0);
    const uint64_t n_new_chunks = end / m_chunk_size + (end % m_chunk_size != 0);
    if (n_new_chunks > max_position / m_chunk_size) {
        out_of_space = true;
        return 0;
    }
    if (n_new_chunks > n_old_chunks) {
        uint64_t old_size = pos.nPos;
        uint64_t new_size = n_new_chunks * m_chunk_size;
        uint64_t inc_size = new_size - old_size;

        if (CheckDiskSpace(m_dir, inc_size)) {
            FILE *file = Open(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in %s%05u.dat\n", new_size, m_prefix, pos.nFile);
                AllocateFileRange(file, pos.nPos, inc_size);
                fclose(file);
                return inc_size;
            }
        } else {
            out_of_space = true;
        }
    }
    return 0;
}

bool FlatFileSeq::Flush(const FlatFilePos& pos, bool finalize)
{
    FILE* file = Open(FlatFilePos(pos.nFile, 0)); // Avoid fseek to nPos
    if (!file) {
        return error("%s: failed to open file %d", __func__, pos.nFile);
    }
    if (finalize && !TruncateFile(file, pos.nPos)) {
        fclose(file);
        return error("%s: failed to truncate file %d", __func__, pos.nFile);
    }
    if (!FileCommit(file)) {
        fclose(file);
        return error("%s: failed to commit file %d", __func__, pos.nFile);
    }

    fclose(file);
    return true;
}
