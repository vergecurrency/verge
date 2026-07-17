// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/votepool.h>

#include <boost/test/unit_test.hpp>

namespace {

uint256 Number(uint32_t value)
{
    uint256 result;
    result.begin()[0] = static_cast<unsigned char>(value);
    result.begin()[1] = static_cast<unsigned char>(value >> 8);
    result.begin()[2] = static_cast<unsigned char>(value >> 16);
    result.begin()[3] = static_cast<unsigned char>(value >> 24);
    return result;
}

pos::CheckpointVote MakeVote(uint32_t bond, uint64_t target, uint64_t slot)
{
    pos::CheckpointVote vote;
    vote.bond_outpoint = COutPoint(Number(bond), 0);
    vote.snapshot_epoch = 0;
    vote.source_epoch = target == 0 ? 0 : target - 1;
    vote.source_checkpoint_root = Number(static_cast<uint32_t>(target + 1));
    vote.target_epoch = target;
    vote.target_checkpoint_root = Number(static_cast<uint32_t>(target + 2));
    vote.head_slot = slot;
    vote.head_block_root = Number(static_cast<uint32_t>(slot + 3));
    vote.signature[0] = 1;
    return vote;
}

} // namespace

BOOST_AUTO_TEST_SUITE(pos_votepool_tests)

BOOST_AUTO_TEST_CASE(admission_and_replacement)
{
    pos::VotePool pool;
    const pos::CheckpointVote first = MakeVote(1, 4, 100);
    BOOST_CHECK_EQUAL(GetSerializeSize(first, SER_NETWORK, 0),
                      pos::SERIALIZED_CHECKPOINT_VOTE_SIZE);
    BOOST_CHECK(pool.Add(first) == pos::VotePoolResult::ADDED);
    BOOST_CHECK(pool.Add(first) == pos::VotePoolResult::DUPLICATE);

    pos::CheckpointVote stale = MakeVote(1, 3, 110);
    BOOST_CHECK(pool.Add(stale) == pos::VotePoolResult::STALE);

    pos::CheckpointVote conflict = MakeVote(1, 4, 101);
    conflict.target_checkpoint_root = Number(255);
    pos::VoteEquivocationEvidence detected;
    BOOST_CHECK(pool.Add(conflict, &detected) ==
                pos::VotePoolResult::CONFLICT);
    BOOST_CHECK(pos::CheckStructure(detected) == pos::StructureError::NONE);

    const pos::CheckpointVote replacement = MakeVote(1, 5, 120);
    BOOST_CHECK(pool.Add(replacement) == pos::VotePoolResult::REPLACED);
    BOOST_CHECK_EQUAL(pool.Size(), 1U);
    BOOST_CHECK(!pool.Exists(pos::GetVoteSigningHash(first)));
    BOOST_CHECK(pool.Exists(pos::GetVoteSigningHash(replacement)));
}

BOOST_AUTO_TEST_CASE(ordering_removal_and_pruning)
{
    pos::VotePool pool;
    const pos::CheckpointVote second = MakeVote(2, 7, 200);
    const pos::CheckpointVote first = MakeVote(1, 2, 100);
    BOOST_CHECK(pool.Add(second) == pos::VotePoolResult::ADDED);
    BOOST_CHECK(pool.Add(first) == pos::VotePoolResult::ADDED);

    const std::vector<pos::CheckpointVote> votes = pool.GetVotes(10);
    BOOST_REQUIRE_EQUAL(votes.size(), 2U);
    BOOST_CHECK(votes[0].bond_outpoint < votes[1].bond_outpoint);
    BOOST_CHECK(pool.DynamicMemoryUsage() > 0U);

    pool.Prune(6);
    BOOST_CHECK_EQUAL(pool.Size(), 1U);
    BOOST_CHECK(pool.Exists(pos::GetVoteSigningHash(second)));
    pool.Remove(pos::GetVoteSigningHash(second));
    BOOST_CHECK_EQUAL(pool.Size(), 0U);
    BOOST_CHECK_EQUAL(pool.DynamicMemoryUsage(), 0U);
}

BOOST_AUTO_TEST_CASE(evidence_pool_bounds_and_identity)
{
    pos::VoteEvidencePool pool;
    pos::VoteEquivocationEvidence evidence;
    evidence.first = MakeVote(7, 4, 100);
    evidence.second = MakeVote(7, 4, 101);
    evidence.second.target_checkpoint_root = Number(99);
    const uint256 first_hash = pos::GetTaggedHash(
        pos::HashDomain::VOTE, evidence.first);
    const uint256 second_hash = pos::GetTaggedHash(
        pos::HashDomain::VOTE, evidence.second);
    if (second_hash < first_hash) std::swap(evidence.first, evidence.second);

    BOOST_CHECK_EQUAL(GetSerializeSize(evidence, SER_NETWORK, 0),
                      pos::SERIALIZED_VOTE_EVIDENCE_SIZE);
    BOOST_CHECK(pos::CheckStructure(evidence) == pos::StructureError::NONE);
    BOOST_CHECK(pool.Add(evidence) == pos::VotePoolResult::ADDED);
    BOOST_CHECK(pool.Add(evidence) == pos::VotePoolResult::DUPLICATE);
    const uint256 evidence_hash = pos::GetTaggedHash(
        pos::HashDomain::EQUIVOCATION, evidence);
    pos::VoteEquivocationEvidence loaded;
    BOOST_CHECK(pool.Get(evidence_hash, loaded));
    BOOST_CHECK(pool.Exists(evidence_hash));
    BOOST_CHECK_EQUAL(pool.GetEvidence(1).size(), 1U);
    pool.Remove(evidence_hash);
    BOOST_CHECK_EQUAL(pool.Size(), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
