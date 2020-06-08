#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>

template <typename T, class Hash = std::hash<T>>
class BloomCache {
public:
    explicit BloomCache(size_t bits=16) : cache_(1 << bits, false), mask_((1 << bits) - 1) {
    }

    bool Check(const T& value) const {
        size_t hash = Hash()(value);
        size_t h1 = (hash << 2) ^ (hash);
        size_t h2 = (hash >> 2) ^ (hash);
        // size_t h3 = (hash >> 2) ^ (hash << 2);

        return cache_[h1 & mask_] && cache_[h2 & mask_]; // && cache_[h3 & mask_];
    }


    void Add(const T& value) {
        size_t hash = Hash()(value);
        size_t h1 = (hash << 2) ^ (hash);
        size_t h2 = (hash >> 2) ^ (hash);
        // size_t h3 = (hash >> 2) ^ (hash << 2);
        cache_[h1 & mask_] = true;
        cache_[h2 & mask_] = true;
        // cache_[h3 & mask_] = true;
    }

private:
    std::vector<bool> cache_;
    size_t mask_;
};