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
    DPRINTF(ReplacementOPT, "Access counter: %d\n",access_counter);

    // Find empty space first
    ReplaceableEntry* victim = findEmptySpace(candidates);

    // Find victim if set is full
    if(victim == NULL){
        victim = findFurthestUse(candidates); // OPT
        const_cast<OPT*>(this)->opt_stats.OPTVictims++;
        DPRINTF(ReplacementOPT, "Using OPT victim\n");
    }
    else
        const_cast<OPT*>(this)->opt_stats.emptyVictims++;

    DPRINTF(ReplacementOPT, "Evicting block with address 0x%llx\n",
                            std::static_pointer_cast<OPTReplData>(victim->replacementData)->addr);

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
        //DPRINTF(ReplacementOPT, "Looking at candidate with address %s\n", candidate_addr);
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
    unsigned victim_next_access = std::numeric_limits<unsigned>::max();
    DPRINTF(ReplacementOPT, "Looking at victim with address %s\n", victim_addr);
    ReplaceableEntry* speculative_victim = NULL;

    std::vector<ReplacementCandidates> LRU_candidates;

    for (const auto& candidate : candidates) {
        // Update victim entry if necessary
        std::string candidate_addr = int_to_hex_str(std::static_pointer_cast<OPTReplData>(candidate->replacementData)->addr);
        DPRINTF(ReplacementOPT, "Looking at candidate with address %s\n", candidate_addr);

        // Find trace data
        if(auto search = trace.find(candidate_addr); search != trace.end()){
            unsigned candidate_next_access = findCandidateAddress(search->second);

            //Update LRU candidates
            if (candidate_next_access == std::numeric_limits<unsigned>::max())
                LRU_candidates.push_back(candidate);

            // Want max value of last_access
            if (victim_next_access < candidate_next_access) {
                DPRINTF(ReplacementOPT, "Update tracking victim; %d(victim) vs %d(candidate)\n", victim_next_access, candidate_next_access);
                victim = candidate;
                victim_next_access = candidate_next_access;
            }
        }
        else{
            DPRINTF(ReplacementOPT, "Could not find trace data with address %s\n", candidate_addr);
            speculative_victim = candidate;
            break;
        }
    }

    if (speculative_victim){
        const_cast<OPT*>(this)->opt_stats.speculativeVictims++;
        DPRINTF(ReplacementOPT, "No better candidate found. Moving ahead to set 0x%llx as victim.\n", 
                std::static_pointer_cast<OPTReplData>(speculative_victim->replacementData)->addr);
        victim = speculative_victim;
    }
    else if (LRU_candidates.size() != 0)
        victim = findEarliestUsed(LRU_candidates);

    if(victim_next_access == std::numeric_limits<unsigned>::max())
        const_cast<OPT*>(this)->opt_stats.notUsedAgainVictims++;
    return victim;
}

std::shared_ptr<ReplacementData>
OPT::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new OPTReplData());
}

std::string 
OPT::int_to_hex_str(Addr addr) const
{
    std::stringstream stream;
    stream << "0x" << std::hex << addr;
    return stream.str();
}

unsigned 
OPT::findCandidateAddress(std::vector<unsigned>& mem_access) const
{
   //unsigned curr_counter = access_counter-16 > 0 ? access_counter-16 : 0;
    unsigned curr_counter = access_counter;

    unsigned candidate_next_access = std::numeric_limits<unsigned>::max();
    for(int i=0; i < mem_access.size(); i++){
        if(mem_access[i]>curr_counter){
            candidate_next_access = mem_access[i];
            DPRINTF(ReplacementOPT, "mem_access[i]>curr_counter; %d(candidate) vs %d(curr_counter)\n", candidate_next_access, curr_counter);
            break;
        }
    }
    return candidate_next_access;
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
    ADD_STAT(notUsedAgainVictims, statistics::units::Count::get(),
             "Blocks was the evicted cause it was not used again.")
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

