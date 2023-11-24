/**
 * @file
 * Describes a Sheperd cache
 */

#ifndef __MEM_CACHE_SHEPERD_HH__
#define __MEM_CACHE_SHEPERD_HH__

#include <cstdint>
#include <numeric>
#include <unordered_set>

#include "base/compiler.hh"
#include "base/logging.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/packet.hh"
#include "params/SheperdCache.hh"

namespace gem5
{

/**
 * A non-coherent Sheperd cache
 */
class SheperdCache : public BaseCache
{
  protected:
    /***********************************************/
    /****** OMITTED FUNTIONALITY PANIC GUARDS ******/
    /***********************************************/
    void doWritebacksAtomic(PacketList& writebacks) override {
        panic("Unexpected %lu atomic writeback", writebacks.size());
    }

    void recvTimingSnoopReq(PacketPtr pkt) override {
        panic("Unexpected timing snoop request %s", pkt->print());
    }

    void recvTimingSnoopResp(PacketPtr pkt) override {
        panic("Unexpected timing snoop response %s", pkt->print());
    }

    Cycles handleAtomicReqMiss(PacketPtr pkt, CacheBlk *&blk,
                               PacketList &writebacks) override {
       panic("Unexpected atomic handle request miss %s", pkt->print());
       return Cycles(0);
    }

    Tick recvAtomic(PacketPtr pkt) override {
        panic("Unexpected atomic request request %s", pkt->print());
        return curTick();
    }

    Tick recvAtomicSnoop(PacketPtr pkt) override {
        panic("Unexpected atomic snoop request %s", pkt->print());
        return curTick();
    }


    /*****************************************/
    /****** SHEPERD CACHE FUNCTIONALITY *****/
    /*****************************************/

    bool access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                PacketList &writebacks) override;

    void handleTimingReqMiss(PacketPtr pkt, CacheBlk *blk,
                             Tick forward_time,
                             Tick request_time) override;

    void recvTimingReq(PacketPtr pkt) override;

    void doWritebacks(PacketList& writebacks,
                      Tick forward_time) override;

    void functionalAccess(PacketPtr pkt, bool from_cpu_side) override;

    void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                            CacheBlk *blk) override;

    void recvTimingResp(PacketPtr pkt) override;


    void satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                        bool deferred_response = false,
                        bool pending_downgrade = false) override;

    /*
     * Creates a new packet with the request to be send to the memory
     * below. The noncoherent cache is below the point of coherence
     * and therefore all fills bring in writable, therefore the
     * needs_writeble parameter is ignored.
     */
    PacketPtr createMissPacket(PacketPtr cpu_pkt, CacheBlk *blk,
                               bool needs_writable,
                               bool is_whole_line_write) const override;

    [[nodiscard]] PacketPtr evictBlock(CacheBlk *blk) override;

  public:
    SheperdCache(const SheperdCacheParams &p);
};

} // namespace gem5

#endif // __MEM_CACHE_SHEPERD_HH__
