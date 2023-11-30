/**
 * @file
 * Definitions a set-associative tagstore for Shepherd Cache.
 */

#include "mem/cache/tags/shepherd_tags.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"

#include <cassert>

namespace gem5
{
// ShepherdTags* ShepherdTagsParams::create() const {
//     return new ShepherdTags(*this);
// }

std::string
ShepherdBlk::print() const
{
    // TODO: print the counter values as well
    return csprintf("%s isSC (%i) counters size (%u)", CacheBlk::print(), isSC, counters.size());
}

ShepherdTags::ShepherdTags(const ShepherdTagsParams& p)
    : BaseTags(p), mc_assoc(p.assoc - p.sc_assoc), sc_assoc(p.sc_assoc), num_sets(p.size / (p.entry_size * p.assoc)),
    blks(p.size / p.block_size),
    _heads(num_sets), _nvc(num_sets, std::vector<unsigned>(p.sc_assoc, 1)), sequentialAccess(p.sequential_access),
    replacementPolicy(p.replacement_policy)
{
    fatal_if(sc_assoc + 1 > p.assoc, "Shepherd cache associativity too large, MC associativity must be at least one");
    // There must be a indexing policy
    fatal_if(!p.indexing_policy, "An indexing policy is required");
    fatal_if(dynamic_cast<SetAssociative* const>(p.indexing_policy) == nullptr, "Indexing policy must be set associative");

    // Check parameters
    if (blkSize < 4 || !isPowerOf2(blkSize)) {
        fatal("Block size must be at least 4 and a power of 2");
    }
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
        if (blk->getSet() < sc_assoc)
            blk->setSC(true);

        // Associate a data chunk to the block
        blk->data = &dataBlks[blkSize*blk_index];

        // Associate a replacement data entry to the block
        blk->replacementData = replacementPolicy->instantiateEntry();
    }
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
    assert(blk && !blk->isValid());
    ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(blk);
    DPRINTF(ShepherdTags, "%s at [%s] for %s\n", __func__, sblk->print(), pkt->print());

    unsigned set = blk->getSet();
    if (!sblk->isSC) {
        // victim block is in MC
        ShepherdBlk* mc_blk = sblk;

        // get head of SC
        int old_head = _heads[set];
        ShepherdBlk* sc_head = dynamic_cast<ShepherdBlk*>(indexingPolicy->getEntry(set, old_head));

        // if SC head is not valid (SC is empty), then MC is not full
        if (sc_head->isValid())
        {
            // Moving SC head to victim block in the MC
            moveBlockWithTag(sc_head, mc_blk);
            mc_blk->isSC  = false; // the old SC head has been moved to main cache (blk)
            sc_head->isSC = true;  // mark the block as being in SC

            // Reset the counters relative to old_head for all the blocks in the set
            for (unsigned way = 0; way < sc_assoc + mc_assoc; ++way)
            {
                ShepherdBlk* block = dynamic_cast<ShepherdBlk*>(indexingPolicy->getEntry(set, way));
                block->counters[old_head] = 0;
            }

            // Setting new head
            _heads[set] = (_heads[set] + 1) % sc_assoc;

            // insert new block at the old SC head
            sblk = sc_head;
            blk  = static_cast<CacheBlk*>(sc_head);
        }
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

    //unsigned set = dynamic_cast<SetAssociative*>(indexingPolicy)->extractSet(addr);
    unsigned set = entries[0]->getSet();

    if (!entries.size()) return nullptr;

    // check for empty MC blocks in the set
    for (unsigned i = 0; i < entries.size(); ++i)
    {
        ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(entries[i]);
        if (!sblk->isSC && !sblk->isValid())
            return sblk;
    }

    // check for empty SC blocks in the set
    for (unsigned i = 0; i < entries.size(); ++i)
    {
        ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(entries[i]);
        if (sblk->isSC && !sblk->isValid())
            return sblk;
    }

    // the set is full, need to evict MC and move SC head there to make room in SC
    // find MC block with either zero counter value
    unsigned head = _heads[set];
    assert(head < sc_assoc);

    ReplacementCandidates mc_victims;
    ShepherdBlk* max_mc_blk = nullptr;
    for (unsigned way = sc_assoc; way < sc_assoc+mc_assoc; ++way)
    {
        ShepherdBlk* sblk = dynamic_cast<ShepherdBlk*>(indexingPolicy->getEntry(set, way));
        assert(!sblk->isSC); // should not be in SC

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
    }

    evict_blks.push_back(victim);
    return victim;
};

} // namespace gem5
