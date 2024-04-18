#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "tbmalloc.hpp"

// using namespace std;

void bench_malloc(size_t ntimes, size_t rounds, size_t thread_num) {
	std::vector<std::thread> th;
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t i = 0; i < thread_num; i++) {
		th.emplace_back(std::thread([&]() {
			std::vector<void*> ptr(ntimes);
			for (size_t j = 0; j < rounds; j++) {
				auto start_time_1 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					size_t kk = (16 + k) % 8192 + 1;
					// size_t kk = 1024;
					ptr[k] = malloc(kk);
					
					// char* cptr = (char*)ptr[k];
					// cptr[0] = '$';
					// cptr[kk - 1] = '$';
				}
				auto end_time_1 = std::chrono::high_resolution_clock::now();

				auto start_time_2 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					free(ptr[k]);
				}
				auto end_time_2 = std::chrono::high_resolution_clock::now();

				malloc_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end_time_1 - start_time_1).count();
				free_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end_time_2 - start_time_2).count();
			}
		}));
	}
	for (auto& t : th) {
		t.join();
	}

    std::cout << thread_num << " threads, " << rounds << " rounds, " << ntimes << " malloc/per round: " << malloc_costtime.load() << " ms" << std::endl;
    std::cout << thread_num << " threads, " << rounds << " rounds, " << ntimes << " free/per round: " << free_costtime.load() << " ms" << std::endl;
    std::cout << thread_num << " threads, " << rounds << " rounds, " << ntimes << " malloc&free/per round: " << malloc_costtime.load() + free_costtime.load() << " ms" << std::endl;
}

void bench_tbmalloc(size_t ntimes, size_t rounds, size_t thread_num) {
	std::vector<std::thread> th;
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t i = 0; i < thread_num; i++) {
		th.emplace_back(std::thread([&]() {
			std::vector<void*> ptr(ntimes);
			for (size_t j = 0; j < rounds; j++) {
				auto start_time_1 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					size_t kk = (16 + k) % 8192 + 1;
					// size_t kk = 1024;
					ptr[k] = tballoc(kk);
					
					// char* cptr = (char*)ptr[k];
					// cptr[0] = '$';
					// cptr[kk - 1] = '$';
				}
				auto end_time_1 = std::chrono::high_resolution_clock::now();

				auto start_time_2 = std::chrono::high_resolution_clock::now();
				for (size_t k = 0; k < ntimes; k++) {
					tbfree(ptr[k]);
				}
				auto end_time_2 = std::chrono::high_resolution_clock::now();

				malloc_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end_time_1 - start_time_1).count();
				free_costtime += std::chrono::duration_cast<std::chrono::milliseconds>(end_time_2 - start_time_2).count();
			}
		}));
	}
	for (auto& t : th) {
		t.join();
	}

    std::cout << thread_num << " threads, " << rounds << " rounds, " << ntimes << " tbmalloc/per round: " << malloc_costtime.load() << " ms" << std::endl;
    std::cout << thread_num << " threads, " << rounds << " rounds, " << ntimes << " tbfree/per round: " << free_costtime.load() << " ms" << std::endl;
    std::cout << thread_num << " threads, " << rounds << " rounds, " << ntimes << " tbmalloc&tbfree/per round: " << malloc_costtime.load() + free_costtime.load() << " ms" << std::endl;
}

int main()
{
	size_t n = 10000;
	bench_tbmalloc(n, 10, 8);
	std::cout << std::endl;
	bench_malloc(n, 10, 8);

	return 0;
}
