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

#include "params/OPT.hh"
#include "base/trace.hh"
#include "sim/cur_tick.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{

OPT::OPT(const Params &p)
  : Base(p)
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
            std::vector<int> temp;
            temp.push_back(i);
            trace.insert({line, temp});
        }
    }
    trace_file.close();

    // Verify
    for (const std::pair<const std::string, std::vector<int>>& n : trace){
        DPRINTF(ReplacementOPT, "%s \n",n.first); 
        for(auto & element : n.second)
            DPRINTF(ReplacementOPT, "%d", element); 
        DPRINTF(ReplacementOPT, "\n"); 
    }
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
OPT::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Set last touch timestamp
    DPRINTF(ReplacementOPT, "In reset\n");
    access_counter++;
    DPRINTF(ReplacementOPT, "Access counter: %d\n",access_counter);
    std::static_pointer_cast<OPTReplData>(
        replacement_data)->lastTouchTick = curTick();
}

ReplaceableEntry*
OPT::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    DPRINTF(ReplacementOPT, "In getVictim\n");

    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
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

std::shared_ptr<ReplacementData>
OPT::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new OPTReplData());
}

} // namespace replacement_policy
} // namespace gem5
