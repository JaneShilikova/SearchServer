#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_set>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex m_;
        std::map<Key, Value> map_;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access(const Key& key, Bucket& buck) : guard_(buck.m_), ref_to_value(buck.map_[key]) { }

        std::lock_guard<std::mutex> guard_;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : all_maps_(bucket_count) { }

    Access operator[](const Key& key) {
        auto& buck = all_maps_[static_cast<uint64_t>(key) % all_maps_.size()];
        return { key, buck };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        for (auto& [mut, m] : all_maps_) {
            std::lock_guard g(mut);
            result.insert(m.begin(), m.end());
        }
        return result;
    }

    void erase(const Key& key) {
        Bucket& elem = all_maps_.at(static_cast<uint64_t>(key) % all_maps_.size());
        std::lock_guard<std::mutex> g(elem.m_);
        elem.map_.erase(key);
    }

private:
    std::vector<Bucket> all_maps_;
};

template <typename Value>
class ConcurrentSet {
private:
    struct Bucket {
        std::mutex m_;
        std::unordered_set<Value> set_;
    };

public:
    static_assert(std::is_integral_v<Value>, "ConcurrentSet supports only integers"s);

    explicit ConcurrentSet(size_t bucket_count) : all_sets_(bucket_count) { }

    void insert(const Value& value) {
        auto& buck = all_sets_[static_cast<uint64_t>(value) % all_sets_.size()];
        std::lock_guard<std::mutex> g(buck.m_);
        buck.set_.insert(value);
    }

    int count(const Value& value) {
        auto& buck = all_sets_[static_cast<uint64_t>(value) % all_sets_.size()];
        std::lock_guard<std::mutex> g(buck.m_);
        return buck.set_.count(value);
    }

    std::unordered_set<Value> BuildOrdinaryMap() {
        std::unordered_set<Value> result;

        for (auto& [mut, s] : all_sets_) {
            std::lock_guard g(mut);
            result.insert(s.begin(), s.end());
        }
        return result;
    }

private:
    std::vector<Bucket> all_sets_;
};
