/**
 * @file
 * Declaration of a set associative indexing * policy 
 * with no restrictions on set-size (non power of two).
 */

#ifndef __MEM_CACHE_INDEXING_POLICIES_SET_ASSOCIATIVE_GEN_HH__
#define __MEM_CACHE_INDEXING_POLICIES_SET_ASSOCIATIVE_GEN_HH__

#include <vector>

#include "mem/cache/tags/indexing_policies/base.hh"
#include "params/SetAssociativeGeneric.hh"

namespace gem5
{

class ReplaceableEntry;

class SetAssociativeGeneric : public BaseIndexingPolicy
{
  protected:
    /* cache block size in bytes */
    const unsigned entrySize;
    /* size of each way in bytes (entry size * num of sets)  */
    const uint32_t waySize;

    /**
     * Apply a hash function to calculate address set.
     *
     * @param addr The address to calculate the set for.
     * @return The set index for given combination of address and way.
     */
    virtual uint32_t extractSet(const Addr addr) const;

  public:
    /**
     * Convenience typedef.
     */
    typedef SetAssociativeGenericParams Params;

    /**
     * Construct and initialize this policy.
     */
    SetAssociativeGeneric(const Params &p);

    /**
     * Destructor.
     */
    ~SetAssociativeGeneric() {};

    /**
     * Find all possible entries for insertion and replacement of an address.
     * Should be called immediately before ReplacementPolicy's findVictim()
     * not to break cache resizing.
     * Returns entries in all ways belonging to the set of the address.
     *
     * @param addr The addr to a find possible entries for.
     * @return The possible entries.
     */
    std::vector<ReplaceableEntry*> getPossibleEntries(const Addr addr) const
                                                                     override;

    /**
     * Regenerate an entry's address from its tag and assigned set and way.
     *
     * @param tag The tag bits.
     * @param entry The entry.
     * @return the entry's original addr value.
     */
    Addr regenerateAddr(const Addr tag, const ReplaceableEntry* entry) const
                                                                   override;
                                                                       /**
     * Generate the tag from the given address.
     *
     * @param addr The address to get the tag from.
     * @return The tag of the address.
     */
    virtual Addr extractTag(const Addr addr) const;
};

} // namespace gem5

#endif //__MEM_CACHE_INDEXING_POLICIES_SET_ASSOCIATIVE_GEN_HH__
