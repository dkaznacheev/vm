#include <iostream>

long long measure(int array_size, int stride) {
    void** buffer = new void*[array_size];
    int loc = stride;
    for (; loc < array_size; loc += stride) {
        buffer[loc] = buffer + loc - stride;
    }
    buffer[0] = buffer + loc - stride;
    void* t = buffer;

    long long best_time = 1e12;

    for (int tries = 0; tries < 10; tries++) {
        for (int i = 0; i < 1000; i++) {
            t = *((void**)t);
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; i++) {
            t = *((void**)t);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto dr = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        if (dr < best_time)
            best_time = dr;
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
            std::cout << measure(size, stride) / 10 << '\t';
        }
        std::cout << std::endl;
    }
    return 0;
}
