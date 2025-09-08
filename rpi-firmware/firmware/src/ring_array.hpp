#pragma once
#include "common.hpp"

template<typename T, size_t N>
struct RingArray{
    array<T, N> ring;
    u16 write = 0;
    u16 read = 0;

    constexpr auto write_head(SelfMut){
        return self.ring.begin() + self.write;
    }
    constexpr auto read_head(SelfRef){
        return self.ring.begin() + self.read;
    }

    constexpr auto dist_till_writer_wrap(SelfRef){
        return self.ring.size() - self.write;
    }


    // The maximum number of elements the buffer can hold.
    constexpr u32 capacity(SelfRef){ return self.ring.size(); }
    // Does the ring buffer currently hold wrapped data?
    constexpr bool is_wrapping(SelfRef){ return self.write < self.read; }
    // Number of elements icurrently in the buffer
    constexpr u32 length(SelfRef){
        u32 base = self.is_wrapping() ? self.capacity() : 0;
        return base + self.write - self.read;
    }
    constexpr bool empty(SelfRef){
        return self.read == self.write;
    }

    constexpr T read_one(SelfMut){
        auto r = self.ring[self.read];
        self.read += 1;
        if(self.read >= self.capacity()){ self.read = 0; } // Wrap
        return r;
    }
    constexpr void write_reserve_n(SelfMut, size_t nelems){
        self.write += nelems;
        self.write %= self.capacity();
    }
};