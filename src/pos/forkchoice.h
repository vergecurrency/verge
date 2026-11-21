// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_FORKCHOICE_H
#define VERGE_POS_FORKCHOICE_H

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <amount.h>
#include <chain.h>
#include <prevector.h>
#include <uint256.h>
#include <memusage.h>

namespace pos {

// Retain bond iteration order, including the original money-range skip rule.
// A height is sufficient because every recorded head is on the validated branch.
struct ForkChoiceSummary {
    std::vector<std::pair<int, CAmount>> heads;

    CAmount WeightAbove(int height) const
    {
        CAmount weight = 0;
        for (const auto& head : heads) {
            if (head.first >= height && head.second > 0 &&
                head.second <= MAX_MONEY - weight) weight += head.second;
        }
        return weight;
    }

    size_t DynamicMemoryUsage() const { return memusage::DynamicUsage(heads); }
};

// Project each authenticated branch view onto the tree. Competing histories
// use a maximum, never a sum that could count the same principal twice.
class ForkChoiceTree {
    struct Node {
        CAmount support{0};
        int maximum_height{-1};
        std::map<uint256, CBlockIndex*> children;
    };
    CBlockIndex* m_root;
    std::map<CBlockIndex*, Node> m_nodes;

public:
    explicit ForkChoiceTree(CBlockIndex* root) : m_root(root) {}

    bool Add(CBlockIndex* tip, const ForkChoiceSummary& summary)
    {
        if (m_root == nullptr || tip == nullptr || tip->nHeight < m_root->nHeight ||
            tip->GetAncestor(m_root->nHeight) != m_root) return false;
        std::map<int, CAmount> weights;
        CAmount total = 0;
        for (const auto& head : summary.heads) {
            if (head.second < 0 || head.second > MAX_MONEY - total) return false;
            total += head.second;
            weights[head.first] += head.second;
        }
        total = 0;
        for (auto it = weights.rbegin(); it != weights.rend(); ++it) {
            total += it->second;
            it->second = total;
        }
        m_nodes[m_root].maximum_height = std::max(
            m_nodes[m_root].maximum_height, tip->nHeight);
        for (CBlockIndex* index = tip; index != m_root; index = index->pprev) {
            Node& node = m_nodes[index];
            const auto weight = weights.lower_bound(index->nHeight);
            node.support = std::max(node.support,
                weight == weights.end() ? CAmount{0} : weight->second);
            node.maximum_height = std::max(node.maximum_height, tip->nHeight);
            m_nodes[index->pprev].children.emplace(index->GetBlockHash(), index);
        }
        return true;
    }

    CBlockIndex* Select() const
    {
        CBlockIndex* selected = m_root;
        if (m_nodes.count(selected) == 0) return nullptr;
        while (!m_nodes.at(selected).children.empty()) {
            CBlockIndex* best = nullptr;
            for (const auto& child : m_nodes.at(selected).children) {
                const Node& node = m_nodes.at(child.second);
                if (best == nullptr || node.support > m_nodes.at(best).support ||
                    (node.support == m_nodes.at(best).support &&
                     (node.maximum_height > m_nodes.at(best).maximum_height ||
                      (node.maximum_height == m_nodes.at(best).maximum_height &&
                       child.first < best->GetBlockHash())))) best = child.second;
            }
            selected = best;
        }
        return selected;
    }
};

// Derived, fully authenticated summaries only. Oversized entries are evaluated
// normally but not retained. Cache eviction must never evict chain candidates.
class ForkChoiceCache {
    struct Entry {
        std::shared_ptr<const ForkChoiceSummary> summary;
        uint64_t last_use;
        size_t bytes;
    };
    std::map<uint256, Entry> m_entries;
    size_t m_bytes{0};
    uint64_t m_clock{0};
    const size_t m_max_entries;
    const size_t m_max_bytes;

public:
    explicit ForkChoiceCache(size_t max_entries = 128,
                             size_t max_bytes = 16 * 1024 * 1024)
        : m_max_entries(max_entries), m_max_bytes(max_bytes) {}

    void Clear() { m_entries.clear(); m_bytes = 0; m_clock = 0; }
    size_t Size() const { return m_entries.size(); }
    size_t DynamicMemoryUsage() const { return m_bytes; }

    std::shared_ptr<const ForkChoiceSummary> Get(const uint256& hash)
    {
        const auto it = m_entries.find(hash);
        if (it == m_entries.end()) return {};
        it->second.last_use = ++m_clock;
        return it->second.summary;
    }

    void Put(const uint256& hash,
             const std::shared_ptr<const ForkChoiceSummary>& summary)
    {
        // Account for the tree node, shared allocation and vector capacity.
        const size_t bytes = summary->DynamicMemoryUsage() +
            memusage::MallocUsage(sizeof(memusage::stl_tree_node<
                std::pair<const uint256, Entry>>)) +
            memusage::MallocUsage(sizeof(ForkChoiceSummary) +
                                  sizeof(memusage::stl_shared_counter));
        if (m_max_entries == 0 || bytes > m_max_bytes) return;
        if (m_entries.count(hash) != 0) return;
        while (!m_entries.empty() &&
               (m_entries.size() >= m_max_entries ||
                m_bytes > m_max_bytes - bytes)) {
            auto oldest = std::min_element(m_entries.begin(), m_entries.end(),
                [](const auto& a, const auto& b) {
                    return a.second.last_use < b.second.last_use;
                });
            m_bytes -= oldest->second.bytes;
            m_entries.erase(oldest);
        }
        m_entries.emplace(hash, Entry{summary, ++m_clock, bytes});
        m_bytes += bytes;
    }
};

} // namespace pos

#endif // VERGE_POS_FORKCHOICE_H
