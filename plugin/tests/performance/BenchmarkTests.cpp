#include <catch2/catch.hpp>
#include <catch2/benchmark/catch.hpp>
#include "../../src/ui/animations/Easing.h"
#include "../../src/ui/animations/TransitionAnimation.h"
#include "../../src/util/cache/CacheLayer.h"
#include "../../src/util/profiling/PerformanceMonitor.h"
#include "../../src/util/crdt/OperationalTransform.h"
#include <string>
#include <vector>
#include <memory>

using namespace Sidechain::UI::Animations;
using namespace Sidechain::Util::Cache;
using namespace Sidechain::Util::Profiling;
using namespace Sidechain::Util::CRDT;

// ========== Animation Benchmarks ==========

TEST_CASE("Easing Functions Performance", "[benchmarks][animations]")
{
    SECTION("Linear Easing")
    {
        BENCHMARK("easeLinear(0.5)")
        {
            return Easing::linear(0.5f);
        };
    }

    SECTION("Cubic Easing")
    {
        BENCHMARK("easeOutCubic(0.5)")
        {
            return Easing::easeOutCubic(0.5f);
        };
    }

    SECTION("Exponential Easing")
    {
        BENCHMARK("easeOutExpo(0.5)")
        {
            return Easing::easeOutExpo(0.5f);
        };
    }

    SECTION("Elastic Easing")
    {
        BENCHMARK("easeOutElastic(0.5)")
        {
            return Easing::easeOutElastic(0.5f);
        };
    }

    SECTION("Bounce Easing")
    {
        BENCHMARK("easeOutBounce(0.5)")
        {
            return Easing::easeOutBounce(0.5f);
        };
    }

    SECTION("Back Easing")
    {
        BENCHMARK("easeInBack(0.5)")
        {
            return Easing::easeInBack(0.5f);
        };
    }
}

TEST_CASE("Easing Function Lookup Performance", "[benchmarks][animations]")
{
    BENCHMARK("Easing::create('easeOutCubic')")
    {
        return Easing::create("easeOutCubic");
    };
}

TEST_CASE("TransitionAnimation Float Interpolation", "[benchmarks][animations]")
{
    auto anim = TransitionAnimation<float>::create(0.0f, 100.0f, 300);

    BENCHMARK("Get current value from animation")
    {
        return anim->getCurrentValue();
    };
}

// ========== Cache Benchmarks ==========

TEST_CASE("Memory Cache Operations", "[benchmarks][cache]")
{
    MemoryCache<std::string, std::string> cache(10 * 1024 * 1024, 10000);

    SECTION("Put operation - small value")
    {
        BENCHMARK("cache.put(key, value)")
        {
            cache.put("test_key", "test_value", 3600, 100);
        };
    }

    SECTION("Get hit - cached value")
    {
        cache.put("existing_key", "existing_value", 3600, 100);

        BENCHMARK("cache.get(existing_key)")
        {
            return cache.get("existing_key");
        };
    }

    SECTION("Get miss - not in cache")
    {
        BENCHMARK("cache.get(nonexistent_key)")
        {
            return cache.get("nonexistent_key");
        };
    }

    SECTION("Remove operation")
    {
        cache.put("to_remove", "value", 3600, 100);

        BENCHMARK("cache.remove(key)")
        {
            return cache.remove("to_remove");
        };
    }
}

TEST_CASE("Memory Cache Bulk Operations", "[benchmarks][cache]")
{
    MemoryCache<std::string, std::string> cache(10 * 1024 * 1024, 10000);

    SECTION("Insert 100 items")
    {
        BENCHMARK("Insert 100 items into cache")
        {
            for (int i = 0; i < 100; ++i)
            {
                cache.put("key_" + std::to_string(i), "value_" + std::to_string(i), 3600, 100);
            }
            return cache.getItemCount();
        };
    }

    SECTION("Sequential lookups - 100 gets")
    {
        for (int i = 0; i < 100; ++i)
        {
            cache.put("key_" + std::to_string(i), "value_" + std::to_string(i), 3600, 100);
        }

        BENCHMARK("100 sequential cache.get() calls")
        {
            int hits = 0;
            for (int i = 0; i < 100; ++i)
            {
                if (cache.get("key_" + std::to_string(i)))
                    hits++;
            }
            return hits;
        };
    }

    SECTION("LRU eviction under size pressure")
    {
        MemoryCache<std::string, std::string> smallCache(1024, 10);  // 1KB max, 10 items

        BENCHMARK("Fill cache with LRU eviction (10 items, 1KB limit)")
        {
            for (int i = 0; i < 20; ++i)
            {
                smallCache.put("key_" + std::to_string(i), std::string(100, 'x'), 3600, 100);
            }
            return smallCache.getItemCount();
        };
    }
}

TEST_CASE("MultiTierCache Performance", "[benchmarks][cache]")
{
    MultiTierCache<std::string, std::string> cache(10 * 1024 * 1024,
                                                    juce::File::getSpecialLocation(
                                                        juce::File::tempDirectory),
                                                    100);

    SECTION("Put to multi-tier cache")
    {
        BENCHMARK("MultiTierCache::put(key, value)")
        {
            cache.put("test_key", "test_value", 3600);
        };
    }

    SECTION("Get from multi-tier cache")
    {
        cache.put("existing_key", "existing_value", 3600);

        BENCHMARK("MultiTierCache::get(key)")
        {
            return cache.get("existing_key");
        };
    }

    SECTION("Get cache statistics")
    {
        cache.put("key1", "value1", 3600);
        cache.put("key2", "value2", 3600);

        BENCHMARK("MultiTierCache::getStats()")
        {
            return cache.getStats();
        };
    }
}

// ========== Performance Monitor Benchmarks ==========

TEST_CASE("ScopedTimer Overhead", "[benchmarks][profiling]")
{
    BENCHMARK("Measure empty scope with ScopedTimer")
    {
        {
            ScopedTimer timer("benchmark_empty");
        }
        return 0;
    };
}

TEST_CASE("PerformanceMonitor Recording", "[benchmarks][profiling]")
{
    auto monitor = PerformanceMonitor::getInstance();

    SECTION("Record a single measurement")
    {
        BENCHMARK("monitor->record(name, duration, threshold)")
        {
            monitor->record("test_metric", 5.5, 10.0);
            return 0;
        };
    }

    SECTION("Get metrics calculation")
    {
        for (int i = 0; i < 100; ++i)
        {
            monitor->record("metric_stats", 1.0 + (i % 5), 10.0);
        }

        BENCHMARK("monitor->getMetrics(name) with 100 samples")
        {
            return monitor->getMetrics("metric_stats");
        };
    }

    SECTION("Dump all metrics")
    {
        for (int i = 0; i < 10; ++i)
        {
            monitor->record("perf_" + std::to_string(i), 2.5, 10.0);
        }

        BENCHMARK("monitor->dumpMetrics() with 10 metrics")
        {
            monitor->dumpMetrics();
            return 0;
        };
    }

    monitor->resetAll();
}

TEST_CASE("PerformanceMonitor Memory Overhead", "[benchmarks][profiling]")
{
    auto monitor = PerformanceMonitor::getInstance();

    SECTION("Track 1000 measurements")
    {
        BENCHMARK("Record 1000 measurements")
        {
            for (int i = 0; i < 1000; ++i)
            {
                monitor->record("many_samples", 1.5 + (i % 3), 10.0);
            }
            return monitor->getMetrics("many_samples").sampleCount;
        };
    }

    monitor->resetAll();
}

// ========== Operational Transform Benchmarks ==========

TEST_CASE("OT Operation Creation", "[benchmarks][crdt]")
{
    SECTION("Create Insert operation")
    {
        BENCHMARK("Make Insert(position, content)")
        {
            auto op = std::make_shared<OperationalTransform::Insert>();
            op->position = 5;
            op->content = "hello";
            op->clientId = 1;
            op->timestamp = 42;
            return op;
        };
    }

    SECTION("Create Delete operation")
    {
        BENCHMARK("Make Delete(position, length)")
        {
            auto op = std::make_shared<OperationalTransform::Delete>();
            op->position = 0;
            op->length = 5;
            op->clientId = 1;
            op->timestamp = 43;
            return op;
        };
    }

    SECTION("Create Modify operation")
    {
        BENCHMARK("Make Modify(position, oldContent, newContent)")
        {
            auto op = std::make_shared<OperationalTransform::Modify>();
            op->position = 0;
            op->oldContent = "old";
            op->newContent = "new";
            op->clientId = 1;
            op->timestamp = 44;
            return op;
        };
    }
}

TEST_CASE("OT Transform Operations", "[benchmarks][crdt]")
{
    SECTION("Transform Insert vs Insert")
    {
        auto ins1 = std::make_shared<OperationalTransform::Insert>();
        ins1->position = 0;
        ins1->content = "hello";
        ins1->clientId = 1;

        auto ins2 = std::make_shared<OperationalTransform::Insert>();
        ins2->position = 0;
        ins2->content = "world";
        ins2->clientId = 2;

        BENCHMARK("Transform(Insert, Insert)")
        {
            return OperationalTransform::transform(ins1, ins2);
        };
    }

    SECTION("Transform Insert vs Delete")
    {
        auto ins = std::make_shared<OperationalTransform::Insert>();
        ins->position = 5;
        ins->content = "hello";
        ins->clientId = 1;

        auto del = std::make_shared<OperationalTransform::Delete>();
        del->position = 0;
        del->length = 3;
        del->clientId = 2;

        BENCHMARK("Transform(Insert, Delete)")
        {
            return OperationalTransform::transform(ins, del);
        };
    }

    SECTION("Transform Delete vs Delete")
    {
        auto del1 = std::make_shared<OperationalTransform::Delete>();
        del1->position = 0;
        del1->length = 5;
        del1->clientId = 1;

        auto del2 = std::make_shared<OperationalTransform::Delete>();
        del2->position = 10;
        del2->length = 3;
        del2->clientId = 2;

        BENCHMARK("Transform(Delete, Delete)")
        {
            return OperationalTransform::transform(del1, del2);
        };
    }
}

TEST_CASE("OT Apply Operation to Text", "[benchmarks][crdt]")
{
    std::string text = "The quick brown fox jumps over the lazy dog";

    SECTION("Apply Insert to text")
    {
        auto op = std::make_shared<OperationalTransform::Insert>();
        op->position = 4;
        op->content = "SLOW ";

        BENCHMARK("Apply Insert operation to text")
        {
            return OperationalTransform::apply(text, op);
        };
    }

    SECTION("Apply Delete to text")
    {
        auto op = std::make_shared<OperationalTransform::Delete>();
        op->position = 0;
        op->length = 4;

        BENCHMARK("Apply Delete operation to text")
        {
            return OperationalTransform::apply(text, op);
        };
    }

    SECTION("Apply Modify to text")
    {
        auto op = std::make_shared<OperationalTransform::Modify>();
        op->position = 0;
        op->oldContent = "The";
        op->newContent = "A";

        BENCHMARK("Apply Modify operation to text")
        {
            return OperationalTransform::apply(text, op);
        };
    }
}

// ========== Integration Benchmarks ==========

TEST_CASE("Animation Performance Under Load", "[benchmarks][integration]")
{
    SECTION("Update 100 animations per frame")
    {
        std::vector<std::shared_ptr<TransitionAnimation<float>>> animations;
        for (int i = 0; i < 100; ++i)
        {
            animations.push_back(
                TransitionAnimation<float>::create(0.0f, 100.0f, 300)
                    ->withEasing(Easing::easeOutCubic));
        }

        BENCHMARK("Get current value from 100 animations")
        {
            float total = 0.0f;
            for (auto& anim : animations)
            {
                total += anim->getCurrentValue();
            }
            return total;
        };
    }
}

TEST_CASE("Cache Performance Regression", "[benchmarks][regression]")
{
    MemoryCache<std::string, std::string> cache(10 * 1024 * 1024, 10000);

    // Pre-populate cache
    for (int i = 0; i < 1000; ++i)
    {
        cache.put("key_" + std::to_string(i), "value_" + std::to_string(i), 3600, 100);
    }

    SECTION("Cache hit rate under repeated access")
    {
        BENCHMARK("1000 lookups in 1000-item cache")
        {
            int hits = 0;
            for (int i = 0; i < 1000; ++i)
            {
                if (cache.get("key_" + std::to_string(i % 1000)))
                    hits++;
            }
            return hits;
        };
    }

    SECTION("Cache insertion performance (should stay constant)")
    {
        BENCHMARK("Insert 100 new items into populated cache")
        {
            for (int i = 1000; i < 1100; ++i)
            {
                cache.put("key_" + std::to_string(i), "value_" + std::to_string(i), 3600, 100);
            }
            return cache.getItemCount();
        };
    }
}

TEST_CASE("OT Transform Chain Performance", "[benchmarks][crdt]")
{
    SECTION("Chain 5 transforms (simulating concurrent edits)")
    {
        auto op1 = std::make_shared<OperationalTransform::Insert>();
        op1->position = 0;
        op1->content = "a";
        op1->clientId = 1;

        auto op2 = std::make_shared<OperationalTransform::Insert>();
        op2->position = 0;
        op2->content = "b";
        op2->clientId = 2;

        auto op3 = std::make_shared<OperationalTransform::Insert>();
        op3->position = 1;
        op3->content = "c";
        op3->clientId = 3;

        BENCHMARK("Transform 3 concurrent insert operations")
        {
            auto [r1, r2] = OperationalTransform::transform(op1, op2);
            auto [r3, r4] = OperationalTransform::transform(r1, op3);
            return r3;
        };
    }
}
