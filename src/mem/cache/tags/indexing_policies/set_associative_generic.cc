
/**
 * @file
 * Declaration of a set associative indexing * policy 
 * with no restrictions on set-size (non power of two).
 */

#include "mem/cache/tags/indexing_policies/set_associative_generic.hh"

#include "mem/cache/replacement_policies/replaceable_entry.hh"

namespace gem5
{

SetAssociativeGeneric::SetAssociativeGeneric(const Params &p)
    : BaseIndexingPolicy(p), entrySize(64), waySize(entrySize * numSets)
{
}

uint32_t
SetAssociativeGeneric::extractSet(const Addr addr) const
{
    // find which block addr falls in
    const long long index = addr / entrySize;

    const uint32_t set = index % numSets;
    return set;
}

Addr SetAssociativeGeneric::extractTag(const Addr addr) const
{
    return addr / waySize;
}

Addr
SetAssociativeGeneric::regenerateAddr(const Addr tag, const ReplaceableEntry* entry)
                                                                        const
{
    return (tag * numSets  + entry->getSet()) * entrySize;
}

std::vector<ReplaceableEntry*>
SetAssociativeGeneric::getPossibleEntries(const Addr addr) const
{
    return sets[extractSet(addr)];
}

} // namespace gem5
