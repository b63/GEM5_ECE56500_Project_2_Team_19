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
#include "base/trace.hh"
#include "mem/cache/base.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/packet.hh"
#include "base/statistics.hh"
#include "params/ShepherdTags.hh"
#include "debug/ShepherdTags.hh"

namespace gem5
{

/**
 * A cache block for Shepherd cache, augmented with counters.
 */
class ShepherdBlk : public CacheBlk
{

protected:
    /** flag to indicate if the cache block is in SC or in MC */
    bool _isSC; 

  public:
    /* imminence counters for each SC entry in a set. Should be equal to sc_size. */
    std::vector<unsigned> counters;

    ShepherdBlk() : CacheBlk(),  _isSC(false), counters()
        {}

    virtual ShepherdBlk&
    operator=(ShepherdBlk&& other)
    {

        this->_isSC = other._isSC;
        this->counters = other.counters;
        CacheBlk::operator=(std::move(other));
        return *this;
    }

    /**
     * Invalidate the block and clear all state.
     */
    virtual void invalidate() override
    {
        CacheBlk::invalidate();

        _isSC = false;
        for (int i = 0; i < counters.size(); ++i)
            counters[i] = 0;
    }

    void init_counters(unsigned sc_size)
    {
        counters.resize(sc_size);
        for (auto &x : counters) x = 0;
    }

    inline void setSC(bool is_sc)
    { _isSC = is_sc; }

    inline bool isSC() const
    { return _isSC; }

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

    struct ShepherdTagStats : public statistics::Group
    {
        ShepherdTagStats(ShepherdTags &tags);

        void regStats() override;
        void preDumpStats() override;

        ShepherdTags &tags;

        /** Number of times fallback replacement strategies was used to find victim. */
        statistics::Scalar fallbackReplRefs;
        /** Number of times there was enough imminence information when finding victim
         * for the chosen victim to be the same as OPT. */
        statistics::Scalar optReplRefs;
        /** Number of times victim was a invalid block (cache was not full, no conflict misses). */
        statistics::Scalar emptyReplRefs;
        /** Number of times victims were requested (misses). */
        statistics::Scalar victimReplRefs;

    } sc_stats;

    /* Move cache block data from src to dst. */
    void moveBlockData(ShepherdBlk* src, ShepherdBlk* dst);

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
     * Move a block's metadata and tag to another location decided by the replacement
     * policy. It behaves as a swap, however, since the destination block
     * should be invalid, the result is a move.
     *
     * @param src_blk The source block.
     * @param dest_blk The destination block. Must be invalid.
     */
    void moveBlockWithTag(CacheBlk *src_blk, CacheBlk *dest_blk)
    {
        assert(!dest_blk->isValid());
        assert(src_blk->isValid());

        // Move src's contents to dest's (along with tag)

        //*static_cast<TaggedEntry*>(dest_blk) = std::move(*static_cast<TaggedEntry*>(src_blk));
        DPRINTF(ShepherdTags, "%s moving src [%s] to [%s]\n", __func__, src_blk->print(), dest_blk->print());
        *dest_blk = std::move(*src_blk);
        DPRINTF(ShepherdTags, "%s moved src [%s] and [%s]\n", __func__, src_blk->print(), dest_blk->print());

        assert(dest_blk->isValid());
        assert(!src_blk->isValid());
    }

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
