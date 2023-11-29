/*
 * Copyright (c) 2018 Inria
 * Copyright (c) 2013,2016-2018 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Definitions a fully associative LRU tagstore.
 */

#include "mem/cache/tags/fa_lru.hh"

#include <cassert>
#include <sstream>

#include "base/compiler.hh"
#include "base/intmath.hh"
#include "base/logging.hh"
#include "mem/cache/base_set_accoc.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"

namespace gem5
{

FALRU::accessBlock(const PacketPtr pkt, Cycles &lat)
{
    CacheBlk *blk = findBlock(pkt->getAddr(), pkt->isSecure());

    // Update Stats
    stats.tagAccesses += allocAssoc;
    if (blk != nullptr) {
        stats.dataAccesses += 1;
    }

    if (blk != nullptr) {
        // Update number of references to accessed block
        blk->increaseRefCount();
        // Update LRU data
        replacementPolicy->touch(blk->replacementData, pkt);
        //Increment Shepherd Counter for block hit
        blk.counter[blk->getSet()] += 1;
    }

    // Return Tag lookup latency
    lat = lookupLatency;

    return blk;
}

FALRU::insertBlock(const PacketPtr pkt, CacheBlk* blk)
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
}

FALRU::findVictim(Addr addr, const bool is_secure, const std::size_t size, std::vector<CacheBlk*>& evict_blks)
{
    // Get a vecotr of possible victims
    const std::vector<ReplaceableEntry*> entries = BaseIndexingPolicy::getPossibleEntries(addr);

    int index = 0;
    while (index < entries.size) {
        SheperdBlk* blk = static_cast<SheperdBlk*>(indexingPolicy->getEntry());
    }


}



} // namespace gem5
