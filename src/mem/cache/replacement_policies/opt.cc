/**
 * Copyright (c) 2018-2020 Inria
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

#include "mem/cache/replacement_policies/opt.hh"

#include <cassert>
#include <memory>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include "params/OPT.hh"
#include "base/trace.hh"
#include "sim/cur_tick.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{

OPT::OPT(const Params &p)
  : Base(p), opt_stats(*this)
{
    DPRINTF(ReplacementOPT, "Cache using OPT replacement strategy\n");

    // Read what is the current benchmark
    std::ifstream benchmark_config_file ("current_benchmark.txt");
    std::string trace_loc;
    getline (benchmark_config_file, trace_loc);
    DPRINTF(ReplacementOPT, "%s\n", trace_loc);
    benchmark_config_file.close();

    // Read the actual benchmark trace
    std::ifstream trace_file (trace_loc);
    std::string line;
    for(int i = 0; getline (trace_file, line); i++ ){
        if(trace.find(line) != trace.end()) // Found
            trace[line].push_back(i);
        else{ //Not found
            std::vector<unsigned> temp;
            temp.push_back(i);
            trace.insert({line, temp});
        }
    }
    trace_file.close();
    
    // Verify
    if(trace.size() == 0)
        panic("Nothing was loaded. Please check if current_benchmark.txt has the right data.");
    /*
    for (const std::pair<const std::string, std::vector<int>>& n : trace){
        DPRINTF(ReplacementOPT, "%s \n",n.first); 
        for(auto & element : n.second)
            DPRINTF(ReplacementOPT, "%d", element); 
        DPRINTF(ReplacementOPT, "\n"); 
    }
    */
}

void
OPT::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
{
    DPRINTF(ReplacementOPT, "In invalidate\n");
    // Reset last touch timestamp
    std::static_pointer_cast<OPTReplData>(
        replacement_data)->lastTouchTick = Tick(0);
}

void
OPT::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    DPRINTF(ReplacementOPT, "In touch\n");
    access_counter++;
    DPRINTF(ReplacementOPT, "Access counter: %d\n",access_counter);

    // Update last touch timestamp
    std::static_pointer_cast<OPTReplData>(
        replacement_data)->lastTouchTick = curTick();
}

void
OPT::reset(const std::shared_ptr<ReplacementData>& replacement_data)
    const
{
    panic("Can't run OPT without access information.");
}

void
OPT::reset(const std::shared_ptr<ReplacementData>& replacement_data,
    const PacketPtr pkt)
{
    // Set last touch timestamp
    DPRINTF(ReplacementOPT, "In reset\n");
    access_counter++;
    DPRINTF(ReplacementOPT, "Access counter: %d\n",access_counter);
    std::static_pointer_cast<OPTReplData>(
        replacement_data)->lastTouchTick = curTick();

    std::static_pointer_cast<OPTReplData>(
        replacement_data)->addr = pkt->getAddr();

    DPRINTF(ReplacementOPT, "Adding addr %#llx to replacement_data\n", std::static_pointer_cast<OPTReplData>(replacement_data)->addr);
}

ReplaceableEntry*
OPT::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    DPRINTF(ReplacementOPT, "In getVictim\n");

    // Find empty space first
    ReplaceableEntry* victim = findEmptySpace(candidates);

    // Find victim if set is full
    if(victim == NULL){
        // Prioritize LRU 
        ReplaceableEntry* victim = findEarliestUsed(candidates); // LRU
        std::string victim_addr = int_to_hex_str(std::static_pointer_cast<OPTReplData>(victim->replacementData)->addr);

        if(auto search = trace.find(victim_addr); search != trace.end()){
            std::vector<unsigned> victim_mem_access = search->second;
            unsigned victim_last_access = victim_mem_access[victim_mem_access.size()-1]; // Last element will show furthest away access
            unsigned curr_counter = access_counter-20 > 0 ? access_counter-20 : 0;
            // If LRU victim access is in the last 20 access, go with FU
            if(victim_last_access > curr_counter)
                victim = NULL;
            else
                const_cast<OPT*>(this)->opt_stats.LRUVictims++;

        }

        if(victim == NULL){
            victim = findFurthestUse(candidates); // OPT
            const_cast<OPT*>(this)->opt_stats.OPTVictims++;
        }
    }
    else
        const_cast<OPT*>(this)->opt_stats.emptyVictims++;

    return victim;
}

ReplaceableEntry*
OPT::findEmptySpace(const ReplacementCandidates& candidates) const
{
    // Visit all candidates to find victim
    ReplaceableEntry* victim = NULL;

    for (const auto& candidate : candidates) {
        // Update victim entry if necessary
        std::string candidate_addr= int_to_hex_str(std::static_pointer_cast<OPTReplData>(candidate->replacementData)->addr);
        DPRINTF(ReplacementOPT, "Looking at candidate with address %s\n", candidate_addr);
        if (candidate_addr == "0x0"){
            victim = candidate;
            break;
        }
    }
    return victim;
}

ReplaceableEntry*
OPT::findEarliestUsed(const ReplacementCandidates& candidates) const
{
    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];;
    for (const auto& candidate : candidates) {
        // Update victim entry if necessary
        if (std::static_pointer_cast<OPTReplData>(
                    candidate->replacementData)->lastTouchTick <
                std::static_pointer_cast<OPTReplData>(
                    victim->replacementData)->lastTouchTick) {
            victim = candidate;
        }
    }
    return victim;
}

ReplaceableEntry*
OPT::findFurthestUse(const ReplacementCandidates& candidates) const
{
    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
    std::string victim_addr = int_to_hex_str(std::static_pointer_cast<OPTReplData>(victim->replacementData)->addr);
    unsigned int victim_last_access = 0;
    DPRINTF(ReplacementOPT, "Looking at victim with address %s\n", victim_addr);
    ReplaceableEntry* speculative_victim = NULL;
    
    if(auto search = trace.find(victim_addr); search != trace.end()){
        std::vector<unsigned> victim_mem_access = search->second;
        victim_last_access = victim_mem_access[victim_mem_access.size()-1]; // Last element will show furthest away access
    }

    for (const auto& candidate : candidates) {
        // Update victim entry if necessary
        std::string candidate_addr = int_to_hex_str(std::static_pointer_cast<OPTReplData>(candidate->replacementData)->addr);
        DPRINTF(ReplacementOPT, "Looking at candidate with address %s\n", candidate_addr);
        unsigned int candidate_last_access = 0;

        // Find trace data
        if(auto search = trace.find(candidate_addr); search != trace.end()){
            std::vector<unsigned> mem_access = search->second;
            candidate_last_access = mem_access[mem_access.size()-1];
        }
        else{
            DPRINTF(ReplacementOPT, "Could not find trace data with address %s\n", candidate_addr);
            speculative_victim = candidate;
            continue;
        }

        // Want max value of last_access
        if (victim_last_access < candidate_last_access) {
            victim = candidate;
            victim_last_access = candidate_last_access;
        }
    }

    if (speculative_victim){
        const_cast<OPT*>(this)->opt_stats.speculativeVictims++;
        DPRINTF(ReplacementOPT, "No better candidate found. Moving ahead to set 0x%llx as victim.\n", 
                std::static_pointer_cast<OPTReplData>(speculative_victim->replacementData)->addr);
        victim = speculative_victim;
    }
    DPRINTF(ReplacementOPT, "Evicting block with address 0x%llx\n",
                            std::static_pointer_cast<OPTReplData>(victim->replacementData)->addr);
    return victim;
}

std::shared_ptr<ReplacementData>
OPT::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new OPTReplData());
}

std::string OPT::int_to_hex_str(Addr addr) const
{
    std::stringstream stream;
    stream << "0x" << std::hex << addr;
    return stream.str();
}

OPT::OPTStats::OPTStats(OPT &_policy)
    : statistics::Group(&_policy),
    policy(_policy),
    ADD_STAT(speculativeVictims, statistics::units::Count::get(),
             "Speculatively evict block in cache."),
    ADD_STAT(emptyVictims, statistics::units::Count::get(),
             "Blocks that are evicted by cause its empty."),
    ADD_STAT(LRUVictims, statistics::units::Count::get(),
             "Blocks that are evicted by LRU."),
    ADD_STAT(OPTVictims, statistics::units::Count::get(),
             "Blocks that are evicted by OPT.")
{
}

void
OPT::OPTStats::regStats()
{
    using namespace statistics;
    statistics::Group::regStats();
}

void
OPT::OPTStats::preDumpStats()
{
    statistics::Group::preDumpStats();
}

} // namespace replacement_policy
} // namespace gem5

