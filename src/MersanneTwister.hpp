//
//  rtmp_relay
//

#pragma once

class MersanneTwister
{
public:
    // Re-init with a given seed
    MersanneTwister(const uint32_t seed)
    {
        mt[0] = seed;

        for (uint32_t i = 1; i < N; i++)
        {
            mt[i] = (F * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i);
        }

        index = N;
    }

    uint32_t extractU32()
    {
        uint16_t i = index;

        if (index >= N)
        {
            twist();
            i = index;
        }

        uint32_t y = mt[i];
        index = i + 1;

        y ^= (mt[i] >> U);
        y ^= (y << S) & B;
        y ^= (y << T) & C;
        y ^= (y >> L);

        return y;
    }

private:
    void twist()
    {
        for (uint32_t i = 0; i < N; i++)
        {
            uint32_t x = (mt[i] & MASK_UPPER) + (mt[(i + 1) % N] & MASK_LOWER);

            uint32_t xA = x >> 1;

            if (x & 0x1)
                xA ^= A;

            mt[i] = mt[(i + M) % N] ^ xA;
        }

        index = 0;
    }

    // Define MT19937 constants (32-bit RNG)

    // Assumes W = 32 (omitting this)
    static const uint32_t N = 624;
    static const uint32_t M = 397;
    static const uint32_t R = 31;
    static const uint32_t A = 0x9908B0DF;

    static const uint32_t F = 1812433253;

    static const uint32_t U = 11;
    // Assumes D = 0xFFFFFFFF (omitting this)

    static const uint32_t S = 7;
    static const uint32_t B = 0x9D2C5680;

    static const uint32_t T = 15;
    static const uint32_t C = 0xEFC60000;

    static const uint32_t L = 18;

    static const uint32_t MASK_LOWER = (1ull << R) - 1;
    static const uint32_t MASK_UPPER = (1ull << R);
    
    uint32_t  mt[N];
    uint16_t  index;
};
