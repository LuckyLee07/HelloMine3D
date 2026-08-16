#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include <ctime>
#include <random>
#include <type_traits>

#include "Singleton.h"

/// @brief Singleton class that increases randomness (such as with seeds)
class RandomSingleton : public Singleton {
  public:
    static RandomSingleton &get();

    template <typename T> T intInRange(T low, T high)
    {
        static_assert(std::is_integral<T>::value, "Not integral type!");
        std::uniform_int_distribution<T> dist(low, high);
        return dist(m_randomEngine);
    }

  private:
    RandomSingleton();

    std::mt19937 m_randomEngine;
};

template <typename REngine = std::mt19937> class Random {
  public:
    using SeedType = typename REngine::result_type;

    Random()
        : Random(static_cast<SeedType>(std::time(nullptr)))
    {
    }

    explicit Random(SeedType n)
    {
        m_randomEngine.seed(n);
        for (int i = 0; i < 5; i++)
            intInRange(i, i * 5);
    }

    template <typename T> T intInRange(T low, T high)
    {
        static_assert(std::is_integral<T>::value, "Not integral type!");
        std::uniform_int_distribution<T> dist(low, high);
        return dist(m_randomEngine);
    }

    void setSeed(SeedType seed)
    {
        m_randomEngine.seed(seed);
    }

  private:
    REngine m_randomEngine;
};

#endif // RANDOM_H_INCLUDED
