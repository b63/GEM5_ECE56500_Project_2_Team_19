/**
 * @file
 * Declaration of a set associative tag store implementing Shepherd cache.
 */

#ifndef __MEM_CACHE_TAGS_Shepherd_HH__
#define __MEM_CACHE_TAGS_Shepherd_HH__

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "base/logging.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/packet.hh"
#include "params/ShepherdTags.hh"
#include "debug/ShepherdTags.hh"

namespace gem5
{

/**
 * A cache block for Shepherd cache, augmented with counters.
 */
class ShepherdBlk : public CacheBlk
{
  public:
    ShepherdBlk() : CacheBlk(),  isSC(false), counters()
        {}

    /** flag to indicate if the cache block is in SC or in MC */
    bool isSC; 
    /* imminence counters for each SC entry in a set. Should be equal to sc_size. */
    std::vector<unsigned> counters;

    void init_counters(unsigned sc_size)
    {
        counters.resize(sc_size);
        for (auto &x : counters) x = 0;
    }

    inline void setSC(bool is_sc)
    { isSC = is_sc; }

    /**
     * Pretty-print information.
     *
     * @return string with basic state information
     */
    std::string print() const override;
};

/**
 * A tag store implementing Shepherd Cache.
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 */
class ShepherdTags : public BaseTags
{
  protected:
    // unsigned allocAssoc; this field in parent class is total assoc (sc_assoc + mc_assoc)

    /* Associativity associated with the Main Cache. */
    unsigned mc_assoc;
    /* Associativity associated with the Shepherd Cache. */
    unsigned sc_assoc;
    /* number of sets */
    size_t const num_sets;

    /** The cache blocks. */
    std::vector<ShepherdBlk> blks;
    /** list of head pointers for each set */
    std::vector<unsigned> _heads;
    /** list of next value counters for each SC block for each set*/
    std::vector<std::vector<unsigned> > _nvc;

    /** Whether tags and data are accessed sequentially. */
    const bool sequentialAccess;

    /** Replacement policy */
    replacement_policy::Base *replacementPolicy;


  public:
    /** Convenience typedef. */
     typedef ShepherdTagsParams Params;

    /**
     * Construct and initialize this tag store.
     */
    ShepherdTags(const ShepherdTagsParams &p);

    /**
     * Destructor
     */
    ~ShepherdTags() {};

    /**
     * Initialize blocks as CacheBlk instances.
     */
    void tagsInit() override;

    /**
     * Access block and update replacement data. May not succeed, in which case
     * nullptr is returned. This has all the implications of a cache access and
     * should only be used as such. Returns the tag lookup latency as a side
     * effect.
     *
     * @param pkt The packet holding the address to find.
     * @param lat The latency of the tag lookup.
     * @return Pointer to the cache block if found.
     */
    CacheBlk* accessBlock(const PacketPtr pkt, Cycles &lat) override;

    /**
     * Find replacement victim based on address. The list of evicted blocks
     * only contains the victim.
     *
     * @param addr Address to find a victim for.
     * @param is_secure True if the target memory space is secure.
     * @param size Size, in bits, of new block to allocate.
     * @param evict_blks Cache blocks to be evicted.
     * @return Cache block to be replaced.
     */
    CacheBlk* findVictim(Addr addr, const bool is_secure,
                         const std::size_t size,
                         std::vector<CacheBlk*>& evict_blks) override;

    /**
     * Insert the new block into the cache and update replacement data.
     *
     * @param pkt Packet holding the address to update
     * @param blk The block to update.
     */
    void insertBlock(const PacketPtr pkt, CacheBlk *blk) override;

    /**
     * Regenerate the block address from the tag and indexing location.
     *
     * @param block The block.
     * @return the block address.
     */
    Addr regenerateBlkAddr(const CacheBlk* blk) const override
    {
        return indexingPolicy->regenerateAddr(blk->getTag(), blk);
    }

    void forEachBlk(std::function<void(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            visitor(blk);
        }
    }

    bool anyBlk(std::function<bool(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            if (visitor(blk)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace gem5

#endif //__MEM_CACHE_TAGS_Shepherd_HH__
