/**
 * @file
 * Definitions a set-associative tagstore for Shepherd Cache.
 */

#include "mem/cache/tags/shepherd_tags.hh"

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
    : BaseTags(p), mc_assoc(p.assoc), sc_assoc(p.sc_assoc), blks(p.size / p.block_size),
     sequentialAccess(p.sequential_access),
     replacementPolicy(p.replacement_policy)
{
    // There must be a indexing policy
    fatal_if(!p.indexing_policy, "An indexing policy is required");

    // Check parameters
    if (blkSize < 4 || !isPowerOf2(blkSize)) {
        fatal("Block size must be at least 4 and a power of 2");
    }
}
void ShepherdTags::tagsInit()
{
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
    ShepherdBlk *blk = dynamic_cast<ShepherdBlk*>(findBlock(pkt->getAddr(), pkt->isSecure()));

    // Update Stats
    stats.tagAccesses += sc_assoc + mc_assoc;
    if (blk != nullptr) {
        stats.dataAccesses += 1;
    }

    if (blk != nullptr) {
        // Update number of references to accessed block
        blk->increaseRefCount();
        // Update LRU data
        replacementPolicy->touch(blk->replacementData, pkt);
        //Increment Shepherd Counter for block hit
        blk->counters[blk->getSet()] += 1;
    }

    // Return Tag lookup latency
    lat = lookupLatency;

    return blk;
}

void ShepherdTags::insertBlock(const PacketPtr pkt, CacheBlk *blk)
{
        assert(!blk->isValid());

    if (!blk->isSc) {
        // Assert that SC head is valid
        int old_head = SheperdTag::_heads[blk->getSet()];
        Cacheblk *sc_head = indexingPolicy->getEntry(blk->getSet(), old_head);
        assert(sc_head->isValid());

        // Moving SC head to victim block in the MC
        BaseTags::moveBlock(sc_head, blk);
        sc_head->isSc = false;

        // Reset the counters for all the MC blocks in the set
        int way = SheperdTags::sc_size;
        while (SheperdTags::sc_size < (SheperdTags::sc_size + SheperdTags::mc_size - 1)) {
            CacheBlk *mc_block = indexingPolicy->getEntry(blk->getSet(), way);
            mc_block.counters[old_head] = 0;
            way += 1;
        }

        // Setting new head
        SheperdTag::_heads[blk->getSet()] = (SheperdTag::_heads[blk->getSet()] + 1) % SheperdTags::sc_size;

        // Get pointer to old SC head to replace it
        blk = indexingPolicy->getEntry(blk->getSet(), old_head);
    }
    // Block where data needs to be inserted is a SC block
    BaseTags::insertBlock(blk);

    stats.tagsInUse += 1;

    replacementPolicy->reset(blk->replacementData, pkt);
};


CacheBlk* ShepherdTags::findVictim(Addr addr, const bool is_secure,
                        const std::size_t size,
                        std::vector<CacheBlk*>& evict_blks)
{
    // Get a vecotr of possible victims
    const std::vector<ReplaceableEntry*> entries = BaseIndexingPolicy::getPossibleEntries(addr);

    int index = 0;
    while (index < entries.size) {
        SheperdBlk* blk = static_cast<SheperdBlk*>(indexingPolicy->getEntry());
    }

    return nullptr;
};

} // namespace gem5
