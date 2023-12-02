/**
 * @file
 * Definitions a set-associative tagstore for Shepherd Cache.
 */

#include "mem/cache/tags/shepherd_tags.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/cache/tags/indexing_policies/set_associative_generic.hh"

#include <cassert>

namespace gem5
{
// ShepherdTags* ShepherdTagsPaVerboserams::create() const {
//     return new ShepherdTags(*this);
// }

std::string
ShepherdBlk::print() const
{
    // TODO: print the counter values as well
    std::string str_counters;
    for (auto x : counters)
        str_counters += std::to_string(x) + " ";
    return csprintf("%s isSC (%i) counters ( %s)", CacheBlk::print(), _isSC, str_counters);
}

ShepherdTags::ShepherdTags(const ShepherdTagsParams& p)
    : BaseTags(p), mc_assoc(p.assoc - p.sc_assoc), sc_assoc(p.sc_assoc), 
    num_sets(p.size  / (p.entry_size * p.assoc)),
    blks(p.size / p.block_size),
    _heads(num_sets), _nvc(num_sets, std::vector<unsigned>(p.sc_assoc, 1)), sequentialAccess(p.sequential_access),
    replacementPolicy(p.replacement_policy), sc_stats(*this)
{
    fatal_if(sc_assoc + 1 > p.assoc, "Shepherd cache associativity too large, MC associativity must be at least one");
    // There must be a indexing policy
    fatal_if(!p.indexing_policy, "An indexing policy is required");
    fatal_if((dynamic_cast<SetAssociativeGeneric* const>(p.indexing_policy) == nullptr && dynamic_cast<SetAssociative* const>(p.indexing_policy) == nullptr), "Indexing policy must be set associative");

    // Check parameters
    if (blkSize < 4 || !isPowerOf2(blkSize)) {
        fatal("Block size must be at least 4 and a power of 2");
    }

    unsigned num_sets_req = (p.size + p.entry_size * p.assoc - 1) / (p.entry_size * p.assoc);
    fatal_if(num_sets_req != num_sets, "the total number of cache frames cannot be evenly divided into required ways, modify cache size");
}
void ShepherdTags::tagsInit()
{
    DPRINTF(ShepherdTags, "%s %u blocks, %u SC, %u MC\n", __func__, numBlocks, sc_assoc, mc_assoc);

    // Initialize all blocks
    for (unsigned blk_index = 0; blk_index < numBlocks; blk_index++) {
        // Locate next cache block
        ShepherdBlk* blk = &blks[blk_index];
        blk->init_counters(sc_assoc);

        // Link block to indexing policy
        indexingPolicy->setEntry(blk, blk_index);
        if (blk->getWay() < sc_assoc)
            blk->setSC(true);

        // Associate a data chunk to the block
        blk->data = &dataBlks[blkSize*blk_index];

        // Associate a replacement data entry to the block
        blk->replacementData = replacementPolicy->instantiateEntry();
    }
}

void ShepherdTags::moveBlockData(ShepherdBlk* src, ShepherdBlk* dst)
{
    assert(src && src->data);
    assert(dst && dst->data);
    std::memcpy(dst->data, (const uint8_t*)src->data, blkSize);
}

CacheBlk* ShepherdTags::accessBlock(const PacketPtr pkt, Cycles &lat)
{
    ShepherdBlk *blk = dynamic_cast<ShepherdBlk*>(BaseTags::findBlock(pkt->getAddr(), pkt->isSecure()));
    DPRINTF(ShepherdTags, "%s for %s\n", __func__, pkt->print());

    // Update Stats
    stats.tagAccesses += sc_assoc + mc_assoc;
    if (sequentialAccess) {
        if (blk != nullptr) {
            stats.dataAccesses += 1;
        }
    } else {
        stats.dataAccesses += sc_assoc+mc_assoc;
    }

    if (blk != nullptr) {
        // Update number of references to accessed block
        blk->increaseRefCount();
        // Update LRU data
        replacementPolicy->touch(blk->replacementData, pkt);
        //Increment all counters relative to all SC blocks in this set
        unsigned set = blk->getSet();
        for (unsigned way = 0; way < sc_assoc; ++way)
        {
            blk->counters[way] = _nvc[set][way];
            // update next value counter
            if (_nvc[set][way] < mc_assoc+sc_assoc)
                ++_nvc[set][way];
        }
    }

    // Return Tag lookup latency
    lat = lookupLatency;

    return blk;
}

void ShepherdTags::insertBlock(const PacketPtr pkt, CacheBlk *blk)
{
    assert(blk);
    ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(blk);
    DPRINTF(ShepherdTags, "%s at [%s] for %s\n", __func__, sblk->print(), pkt->print());

    unsigned set = blk->getSet();
    if (sblk->isSC() && sblk->isValid()) {
        // victim block is SC and is valid, should be the SC head
        // get head of SC
        int old_head = _heads[set];

        ShepherdBlk* sc_head = dynamic_cast<ShepherdBlk*>(indexingPolicy->getEntry(set, old_head));
        assert(sc_head == sblk);

        ShepherdBlk* mc_blk = nullptr;
        // there should be a block in MC that is invalid
        for (unsigned way = sc_assoc; way < sc_assoc + mc_assoc; ++way)
        {
            mc_blk = dynamic_cast<ShepherdBlk*>(indexingPolicy->getEntry(set, way));
            assert(!mc_blk->isSC()); // should be a MC block
            if (!mc_blk->isValid())
                break;
        }
        // victim block is in MC, should be invalid
        assert(mc_blk && !mc_blk->isValid());

        // Moving SC head to victim block in the MC
        BaseTags::moveBlock(sc_head, mc_blk); // only moves metadata
        moveBlockData(sc_head, mc_blk); // move (copy) the actual data

        mc_blk->setSC(false); // the old SC head has been moved to main cache (blk)
        sc_head->setSC(true);  // mark the block as being in SC

        // Reset the counters relative to old_head for all the blocks in the set
        for (unsigned way = 0; way < sc_assoc + mc_assoc; ++way)
        {
            ShepherdBlk* block = dynamic_cast<ShepherdBlk*>(indexingPolicy->getEntry(set, way));
            block->counters[old_head] = 0;
        }

        // Setting new head
        _heads[set] = (_heads[set] + 1) % sc_assoc;
    }
    // Block where data needs to be inserted is a SC block
    BaseTags::insertBlock(pkt, blk);

    stats.tagsInUse += 1;

    replacementPolicy->reset(blk->replacementData, pkt);
};

CacheBlk* ShepherdTags::findVictim(Addr addr, const bool is_secure,
                        const std::size_t size,
                        std::vector<CacheBlk*>& evict_blks)
{
    // Get a vecotr of possible victims
    const std::vector<ReplaceableEntry*> entries = indexingPolicy->getPossibleEntries(addr);

    DPRINTF(ShepherdTags, "%s for %#018x\n", __func__, addr);

    if (!entries.size()) return nullptr;
    sc_stats.victimReplRefs++;

    //unsigned set = dynamic_cast<SetAssociative*>(indexingPolicy)->extractSet(addr);
    unsigned set = entries[0]->getSet();

    // check for empty MC blocks in the set
    for (unsigned i = 0; i < entries.size(); ++i)
    {
        ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(entries[i]);
        if (!sblk->isSC() && !sblk->isValid())
        {
            DPRINTF(ShepherdTags, "%s victim is invalid MC block [%s]\n", __func__, sblk->print());
            sc_stats.emptyReplRefs++;
            return sblk;
        }
    }

    // check for empty SC blocks in the set
    for (unsigned i = 0; i < entries.size(); ++i)
    {
        ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(entries[i]);
        if (sblk->isSC() && !sblk->isValid())
        {
            DPRINTF(ShepherdTags, "%s victim is invalid SC block [%s]\n", __func__, sblk->print());
            sc_stats.emptyReplRefs++;
            return sblk;
        }
    }

    // the set is full, need to evict MC and move SC head there to make room in SC
    // so return SC head for now
    // find MC block with either zero counter value
    unsigned head = _heads[set];
    assert(head < sc_assoc);

    ReplacementCandidates mc_victims;
    ShepherdBlk* max_mc_blk = nullptr;
    for (unsigned way = sc_assoc; way < sc_assoc+mc_assoc; ++way)
    {
        ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(indexingPolicy->getEntry(set, way));
        assert(!sblk->isSC()); // should not be in SC

        if (!sblk->counters[head])
            mc_victims.push_back(sblk);

        // keep track of MC block with highest count
        if (!max_mc_blk || max_mc_blk->counters[head] > sblk->counters[head])
            max_mc_blk = sblk;
    }


    CacheBlk* victim = max_mc_blk; // default victim MC block with highest count
    if (mc_victims.size() > 0)
    {
        // choose MC blk based on fallback policy (LRU)
        victim = static_cast<CacheBlk*>(replacementPolicy->getVictim(mc_victims));
        sc_stats.fallbackReplRefs++;
    }
    else
    {
        sc_stats.optReplRefs++;
    }

    evict_blks.push_back(victim);

    CacheBlk* sc_head = dynamic_cast<CacheBlk*>(indexingPolicy->getEntry(set, head));
    DPRINTF(ShepherdTags, "%s victim is SC head [%s], evicting MC block [%s]\n", __func__, sc_head->print(), victim->print());

    // the victim block will be the SC head (it will be moved in insertBlock)
    return sc_head;
};


ShepherdTags::ShepherdTagStats::ShepherdTagStats(ShepherdTags &_tags)
    : statistics::Group(&_tags),
    tags(_tags),
    ADD_STAT(fallbackReplRefs, statistics::units::Count::get(),
             "Total number of times fallback replacement strategies was used to find victim."),
    ADD_STAT(optReplRefs, statistics::units::Count::get(),
             "Total number of times there was enough imminence information when finding victim."),
    ADD_STAT(emptyReplRefs, statistics::units::Count::get(),
             "Number of times victims was an empty/invalid block (not a conflict misses)."),
    ADD_STAT(victimReplRefs, statistics::units::Count::get(),
             "Number of times victims were requested (misses).")
{
}

void
ShepherdTags::ShepherdTagStats::regStats()
{
    using namespace statistics;

    statistics::Group::regStats();
}

void
ShepherdTags::ShepherdTagStats::preDumpStats()
{
    statistics::Group::preDumpStats();
}


} // namespace gem5
