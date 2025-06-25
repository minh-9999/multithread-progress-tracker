
#include <immintrin.h> // AVX/SSE support
#include <span>

inline void process_data_simd(std::span<uint8_t> input)
{
    size_t size = input.size();
    size_t aligned_size = size - (size % 32); // Align for AVX (256-bit)

    for (size_t i = 0; i < aligned_size; i += 32)
    {
        __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(&input[i]));
        data = _mm256_add_epi8(data, _mm256_set1_epi8(1)); // Perform SIMD operations
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(&input[i]), data);
    }

    // Process the rest (if any)
    for (size_t i = aligned_size; i < size; ++i)
    {
        input[i] += 1; // Process each byte normally
    }
}
