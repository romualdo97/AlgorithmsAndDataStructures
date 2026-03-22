
#include <list>
#include <unordered_map>

template <typename KeyType, typename ValueType>
class LruCache
{
public:
    LruCache(size_t InCapacity)
        : Capacity(InCapacity)
    {
        EntryHashmap.max_size(InCapacity);
        CacheEntries.set_capacity(InCapacity);
    }

    void Insert(KeyType key, ValueType value)
    {
        auto it = EntryHashmap.find(key);
        CacheEntries.push_back(std::make_pair(key, value));

        if (it != EntryHashmap.end())
        {
            CacheEntries.erase(it->second);
            EntryHashmap.erase(it);
        }

        EntryHashmap[key] = CacheEntries.begin();

        if (CacheEntries.size() > Capacity)
        {
            auto end = CacheEntries.end() - 1;
            EntryHashmap.erase(end->first);
            CacheEntries.pop_back();
        }
    }

    ValueType* Get(KeyType key)
    {
        auto it = EntryHashmap.find(key);
        if (it != EntryHashmap.end())
        {
            return &it->second->second;
        }

        return nullptr;
    }
    
private:
    using EntryPairType = std::pair<KeyType, ValueType>;
    using ListIteratorType = typename std::list<EntryPairType>::iterator;

    std::list<EntryPairType> CacheEntries;
    std::unordered_map<KeyType, ListIteratorType> EntryHashmap;
    size_t Capacity;
};

int main()
{
    // LruCache<int, int> LruCache;
    return 0;
}
