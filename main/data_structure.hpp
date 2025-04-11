#pragma once

#include <vector>

struct BITree {
    int n;          // 配列の要素数(数列の要素数+1)
    std::vector<int> bit;  // データの格納先
    BITree(int n_) : n(n_ + 1), bit(n, 0) {}
    void add(int i, int x) {
        for (int idx = i; idx < n; idx += (idx & -idx)) {
            bit[idx] += x;
        }
    }
    int sum(int i) {
        int s(0);
        for (int idx = i; idx > 0; idx -= (idx & -idx)) {
            s += bit[idx];
        }
        return s;
    }
    int lower_bound(int rank) {
        if(rank < 0) return 0;
        int x = 0, r = 1;
        while((r << 1) <= n) r = r << 1; 
        for(int k = r; k > 0; k >>= 1) {
            if((x + k <= n) && bit[x + k] < rank) {
                rank -= bit[x + k];
                x += k;
            }
        }
        return x + 1;
    }
};




template<typename T>
class ringBuffer {
    //static_assert((size & (size-1)) == 0, "size must be a power of two");
    public:
        //ringBuffer(std::vector<T> val) : buffer_(val) {}
        ringBuffer(int32_t size) : buffer_(size) {}

        T& operator [](int64_t i) & { return buffer_[i & (buffer_.size() - 1)]; }
        const T& operator[] (int64_t i) const& {return buffer_[i & (buffer_.size() - 1)]; }
    
        //size_t size() const { return buffer_.size();};

        void fill(const T val){
            std::fill_n(buffer_, buffer_.size(), val);
        }

        std::vector<T> buffer_;
};

