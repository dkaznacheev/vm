#include <iostream>
#include <chrono>

#define ONE t = *((void**)t);
#define TEN ONE ONE ONE ONE ONE ONE ONE ONE ONE ONE
#define HUNDRED TEN TEN TEN TEN TEN TEN TEN TEN TEN TEN  
#define THOUSAND HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED

void** make_buffer(int array_size, int stride) {
    void** buffer = new void*[array_size];
    int loc = stride;
    for (; loc < array_size; loc += stride) {
        buffer[loc] = buffer + loc - stride;
    }
    buffer[0] = buffer + loc - stride;
    return buffer;
}

int bh = 0;

long long measure(int array_size, int stride) {
    void** buffer = make_buffer(array_size, stride);
    void* t = buffer;

    long long best_time = 1e12;

    for (int tries = 0; tries < 10; tries++) {
        for (int i = 0; i < 10; i++) {
            THOUSAND;
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; i++) {
            THOUSAND;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto dr = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        if (dr < best_time) {
            best_time = dr;
        }

        bh = (bh + (char*) t - (char*)buffer) % 1000;
    }

    delete[] buffer;

    return best_time;
}

int main() {
    std::cout << 's' << '\t';
    int max = 22;
    for (int stride = 1 << 4; stride <= 1 << max; stride *= 2)
        std::cout << stride * 8 << '\t';
    std::cout << std::endl;
    for (int spots = 2; spots <= 32; spots++) {
        std::cout << spots << '\t';
        for (int stride = 1 << 4; stride <= 1 << max; stride *= 2) {
            int size = spots * stride;
            std::cout << measure(size, stride) / 10000  << '\t';
        }
        std::cout << std::endl;
    }
    std::cerr << bh << std::endl; // so that it does not get discarded
    return 0;
}
