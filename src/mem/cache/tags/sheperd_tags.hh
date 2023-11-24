/**
 * @file
 * Declaration of a associative tag store for Sheperd Cache.
 */

#ifndef __MEM_CACHE_TAGS_SHEPERD_HH__
#define __MEM_CACHE_TAGS_SHEPERD_HH__

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/bitfield.hh"
#include "base/intmath.hh"
#include "base/logging.hh"
#include "base/statistics.hh"
#include "base/types.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/tags/base.hh"
#include "mem/packet.hh"
#include "params/SheperdTags.hh"

namespace gem5
{

// Uncomment to enable sanity checks for the FALRU cache and the
// TrackedCaches class
//#define FALRU_DEBUG

class BaseCache;
class ReplaceableEntry;

// A bitmask of the caches we are keeping track of. Currently the
// lowest bit is the smallest cache we are tracking, as it is
// specified by the corresponding parameter. The rest of the bits are
// for exponentially growing cache sizes.
typedef uint32_t CachesMask;

/**
 * A fully associative cache block.
 */
class FALRUBlk : public CacheBlk
{
  public:
    FALRUBlk() : CacheBlk(), prev(nullptr), next(nullptr), inCachesMask(0) {}

    /** The previous block in LRU order. */
    FALRUBlk *prev;
    /** The next block in LRU order. */
    FALRUBlk *next;

    /** A bit mask of the caches that fit this block. */
    CachesMask inCachesMask;

    /**
     * Pretty-print inCachesMask and other CacheBlk information.
     *
     * @return string with basic state information
     */
    std::string print() const override;
};

/**
 * A fully associative LRU cache. Keeps statistics for accesses to a number of
 * cache sizes at once.
 */
class SheperdTags : public BaseTags
{
  public:
    /** Typedef the block type used in this class. */
    typedef FALRUBlk BlkType;

  protected:
    /** The cache blocks. */
    FALRUBlk *blks;

    /** The MRU block. */
    FALRUBlk *head;
    /** The LRU block. */
    FALRUBlk *tail;

    /** Hash table type mapping addresses to cache block pointers. */
    struct PairHash
    {
        template <class T1, class T2>
        std::size_t operator()(const std::pair<T1, T2> &p) const
        {
            return std::hash<T1>()(p.first) ^ std::hash<T2>()(p.second);
        }
    };
    typedef std::pair<Addr, bool> TagHashKey;
    typedef std::unordered_map<TagHashKey, FALRUBlk *, PairHash> TagHash;

    /** The address hash table. */
    TagHash tagHash;

    /**
     * Move a cache block to the MRU position.
     *
     * @param blk The block to promote.
     */
    void moveToHead(FALRUBlk *blk);

    /**
     * Move a cache block to the LRU position.
     *
     * @param blk The block to demote.
     */
    void moveToTail(FALRUBlk *blk);

  public:
    typedef FALRUParams Params;

    /**
     * Construct and initialize this cache tagstore.
     */
    SheperdTags(const Params &p);
    ~SheperdTags();

    /**
     * Initialize blocks as FALRUBlk instances.
     */
    void tagsInit() override;

    /**
     * Invalidate a cache block.
     * @param blk The block to invalidate.
     */
    void invalidate(CacheBlk *blk) override;

    /**
     * Access block and update replacement data.  May not succeed, in which
     * case nullptr pointer is returned.  This has all the implications of a
     * cache access and should only be used as such.
     * Returns tag lookup latency and the inCachesMask flags as a side effect.
     *
     * @param pkt The packet holding the address to find.
     * @param lat The latency of the tag lookup.
     * @param in_cache_mask Mask indicating the caches in which the blk fits.
     * @return Pointer to the cache block.
     */
    CacheBlk* accessBlock(const PacketPtr pkt, Cycles &lat,
                          CachesMask *in_cache_mask);

    /**
     * Just a wrapper of above function to conform with the base interface.
     */
    CacheBlk* accessBlock(const PacketPtr pkt, Cycles &lat) override;

    /**
     * Find the block in the cache, do not update the replacement data.
     * @param addr The address to look for.
     * @param is_secure True if the target memory space is secure.
     * @param asid The address space ID.
     * @return Pointer to the cache block.
     */
    CacheBlk* findBlock(Addr addr, bool is_secure) const override;

    /**
     * Find a block given set and way.
     *
     * @param set The set of the block.
     * @param way The way of the block.
     * @return The block.
     */
    ReplaceableEntry* findBlockBySetAndWay(int set, int way) const override;

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

    void moveBlock(CacheBlk *src_blk, CacheBlk *dest_blk) override;

    /**
     * Generate the tag from the addres. For fully associative this is just the
     * block address.
     * @param addr The address to get the tag from.
     * @return The tag.
     */
    Addr extractTag(Addr addr) const override
    {
        return blkAlign(addr);
    }

    /**
     * Regenerate the block address from the tag.
     *
     * @param block The block.
     * @return the block address.
     */
    Addr regenerateBlkAddr(const CacheBlk* blk) const override
    {
        return blk->getTag();
    }

    void forEachBlk(std::function<void(CacheBlk &)> visitor) override {
        for (int i = 0; i < numBlocks; i++) {
            visitor(blks[i]);
        }
    }

    bool anyBlk(std::function<bool(CacheBlk &)> visitor) override {
        for (int i = 0; i < numBlocks; i++) {
            if (visitor(blks[i])) {
                return true;
            }
        }
        return false;
    }

  private:
    /**
     * Mechanism that allows us to simultaneously collect miss
     * statistics for multiple caches. Currently, we keep track of
     * caches from a set minimum size of interest up to the actual
     * cache size.
     */
    class CacheTracking : public statistics::Group
    {
      public:
        CacheTracking(unsigned min_size, unsigned max_size,
                      unsigned block_size, statistics::Group *parent);

        /**
         * Initialiaze cache blocks and the tracking mechanism
         *
         * All blocks in the cache need to be initialized once.
         *
         * @param blk the MRU block
         * @param blk the LRU block
         */
        void init(FALRUBlk *head, FALRUBlk *tail);

        /**
         * Update boundaries as a block will be moved to the MRU.
         *
         * For all caches that didn't fit the block before moving it,
         * we move their boundaries one block closer to the MRU. We
         * also update InCacheMasks as neccessary.
         *
         * @param blk the block that will be moved to the head
         */
        void moveBlockToHead(FALRUBlk *blk);

        /**
         * Update boundaries as a block will be moved to the LRU.
         *
         * For all caches that fitted the block before moving it, we
         * move their boundaries one block closer to the LRU. We
         * also update InCacheMasks as neccessary.
         *
         * @param blk the block that will be moved to the head
         */
        void moveBlockToTail(FALRUBlk *blk);

        /**
         * Notify of a block access.
         *
         * This should be called every time a block is accessed and it
         * updates statistics. If the input block is nullptr then we
         * treat the access as a miss. The block's InCacheMask
         * determines the caches in which the block fits.
         *
         * @param blk the block to record the access for
         */
        void recordAccess(FALRUBlk *blk);

        /**
         * Check that the tracking mechanism is in consistent state.
         *
         * Iterate from the head (MRU) to the tail (LRU) of the list
         * of blocks and assert the inCachesMask and the boundaries
         * are in consistent state.
         *
         * @param head the MRU block of the actual cache
         * @param head the LRU block of the actual cache
         */
        void check(const FALRUBlk *head, const FALRUBlk *tail) const;

      private:
        /** The size of the cache block */
        const unsigned blkSize;
        /** The smallest cache we are tracking */
        const unsigned minTrackedSize;
        /** The number of different size caches being tracked. */
        const int numTrackedCaches;
        /** A mask for all cache being tracked. */
        const CachesMask inAllCachesMask;
        /** Array of pointers to blocks at the cache boundaries. */
        std::vector<FALRUBlk*> boundaries;

      protected:
        /**
         * @defgroup FALRUStats Fully Associative LRU specific statistics
         * The FA lru stack lets us track multiple cache sizes at once. These
         * statistics track the hits and misses for different cache sizes.
         * @{
         */

        /** Hits in each cache */
        statistics::Vector hits;
        /** Misses in each cache */
        statistics::Vector misses;
        /** Total number of accesses */
        statistics::Scalar accesses;

        /**
         * @}
         */
    };
    CacheTracking cacheTracking;
};

} // namespace gem5

#endif // __MEM_CACHE_TAGS_SHEPERD_HH__
