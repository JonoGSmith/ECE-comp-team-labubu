#pragma once
#include "common.hpp"

template<typename T, size_t N>
struct RingArray{
    array<T, N> ring;
    u16 write = 0;
    u16 read = 0;

    // // Inserts the given data into the ring array, wrapping on the edge. Hopefully you don't crush the buffer (you can).
    // static constexpr auto insert(SelfMut, span<T> data){
    //     auto remaining = self.dist_till_wrap();
    //     auto phase1copy = std::min(remaining, data.size())
    //     auto phase2copy = data.size() - phase1copy;
    //     std::copy_n(data.begin(), phase1copy, self.ring.begin() + self.write);
    //     if(phase2copy > 0){ // Wrapping copy
    //         std::copy_n(data.begin() + phase1copy, phase2copy, self.ring.begin());
    //         self.write = phase2copy;
    //     }
    // }

    constexpr auto write_head(SelfMut){
        return self.ring.begin() + self.write;
    }

    constexpr auto dist_till_wrap(SelfRef){
        return self.ring.size() - self.write;
    }
};