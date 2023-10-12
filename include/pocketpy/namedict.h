#pragma once

#include "common.h"
#include "memory.h"
#include "str.h"

namespace pkpy{

template<typename T>
constexpr T default_invalid_value(){
    if constexpr(std::is_pointer_v<T>) return nullptr;
    else if constexpr(std::is_same_v<int, T>) return -1;
    else return Discarded();
}

template<typename V>
struct SmallNameDict{
    using K = StrName;
    static_assert(std::is_pod_v<V>);

    static const int kCapacity = 12;

    bool _is_small;
    uint16_t _size;
    K _keys[kCapacity];
    V _values[kCapacity];

    SmallNameDict(): _is_small(true), _size(0) {}

    bool try_set(K key, V val){
        for(int i=0; i<kCapacity; i++){
            if(_keys[i] == key){
                _values[i] = val;
                return true;
            }
        }
        if(_size == kCapacity) return false;
        _keys[_size] = key;
        _values[_size] = val;
        _size++;
        return true;
    }

    V operator[](K key) const {
        for(int i=0; i<kCapacity; i++){
            if(_keys[i] == key) return _values[i];
        }
        throw std::out_of_range(fmt("SmallDict key not found: ", key));
    }

    V try_get(K key) const {
        for(int i=0; i<kCapacity; i++){
            if(_keys[i] == key) return _values[i];
        }
        return default_invalid_value<V>();
    }

    V* try_get_2(K key) {
        for(int i=0; i<kCapacity; i++){
            if(_keys[i] == key) return &_values[i];
        }
        return nullptr;
    }

    bool contains(K key) const {
        for(int i=0; i<kCapacity; i++){
            if(_keys[i] == key) return true;
        }
        return false;
    }

    bool del(K key){
        for(int i=0; i<kCapacity; i++){
            if(_keys[i] == key){
                _keys[i] = StrName();
                _size--;
                return true;
            }
        }
        return false;
    }

    template<typename Func>
    void apply(Func func) const {
        for(int i=0; i<kCapacity; i++){
            if(!_keys[i].empty()) func(_keys[i], _values[i]);
        }
    }

    void clear(){
        for(int i=0; i<kCapacity; i++){
            _keys[i] = StrName();
        }
        _size = 0;
    }

    uint16_t size() const { return _size; }
    uint16_t capacity() const { return kCapacity; }
};

inline const uint16_t kHashSeeds[] = {9629, 43049, 13267, 59509, 39251, 1249, 27689, 9719, 19913};

inline uint16_t _hash(StrName key, uint16_t mask, uint16_t hash_seed){
    return ( (key).index * (hash_seed) >> 8 ) & (mask);
}

uint16_t _find_perfect_hash_seed(uint16_t capacity, const std::vector<StrName>& keys);

template<typename T>
struct LargeNameDict {
    using Item = std::pair<StrName, T>;
    static constexpr uint16_t __Capacity = 32;
    static_assert(is_pod<T>::value);

    bool _is_small;
    float _load_factor;
    uint16_t _capacity;
    uint16_t _size;
    uint16_t _hash_seed;
    uint16_t _mask;
    Item* _items;

#define HASH_PROBE_0(key, ok, i)            \
ok = false;                                 \
i = _hash(key, _mask, _hash_seed);          \
for(int _j=0; _j<_capacity; _j++) {         \
    if(!_items[i].first.empty()){           \
        if(_items[i].first == (key)) { ok = true; break; }  \
    }else{                                                  \
        if(_items[i].second == 0) break;                    \
    }                                                       \
    i = (i + 1) & _mask;                                    \
}

#define HASH_PROBE_1(key, ok, i)            \
ok = false;                                 \
i = _hash(key, _mask, _hash_seed);          \
while(!_items[i].first.empty()) {           \
    if(_items[i].first == (key)) { ok = true; break; }  \
    i = (i + 1) & _mask;                                \
}

#define NAMEDICT_ALLOC()                \
    _items = (Item*)malloc(_capacity * sizeof(Item));    \
    memset(_items, 0, _capacity * sizeof(Item));                \

    LargeNameDict(float load_factor=kInstAttrLoadFactor):
        _is_small(false),
        _load_factor(load_factor), _capacity(__Capacity), _size(0), 
        _hash_seed(kHashSeeds[0]), _mask(__Capacity-1) {
        NAMEDICT_ALLOC()
    }

    LargeNameDict(const LargeNameDict& other) {
        memcpy(this, &other, sizeof(LargeNameDict));
        NAMEDICT_ALLOC()
        for(int i=0; i<_capacity; i++) _items[i] = other._items[i];
    }

    LargeNameDict& operator=(const LargeNameDict& other) {
        free(_items);
        memcpy(this, &other, sizeof(LargeNameDict));
        NAMEDICT_ALLOC()
        for(int i=0; i<_capacity; i++) _items[i] = other._items[i];
        return *this;
    }
    
    ~LargeNameDict(){ free(_items); }

    LargeNameDict(LargeNameDict&&) = delete;
    LargeNameDict& operator=(LargeNameDict&&) = delete;
    uint16_t size() const { return _size; }
    uint16_t capacity() const { return _capacity; }

    T operator[](StrName key) const {
        bool ok; uint16_t i;
        HASH_PROBE_0(key, ok, i);
        if(!ok) throw std::out_of_range(fmt("NameDict key not found: ", key));
        return _items[i].second;
    }

    void set(StrName key, T val){
        bool ok; uint16_t i;
        HASH_PROBE_1(key, ok, i);
        if(!ok) {
            _size++;
            if(_size > _capacity*_load_factor){
                _rehash(true);
                HASH_PROBE_1(key, ok, i);
            }
            _items[i].first = key;
        }
        _items[i].second = val;
    }

    void _rehash(bool resize){
        Item* old_items = _items;
        uint16_t old_capacity = _capacity;
        if(resize){
            _capacity *= 2;
            _mask = _capacity - 1;
        }
        NAMEDICT_ALLOC()
        for(uint16_t i=0; i<old_capacity; i++){
            if(old_items[i].first.empty()) continue;
            bool ok; uint16_t j;
            HASH_PROBE_1(old_items[i].first, ok, j);
            if(ok) FATAL_ERROR();
            _items[j] = old_items[i];
        }
        free(old_items);
    }

    void _try_perfect_rehash(){
        _hash_seed = _find_perfect_hash_seed(_capacity, keys());
        _rehash(false); // do not resize
    }

    T try_get(StrName key) const{
        bool ok; uint16_t i;
        HASH_PROBE_0(key, ok, i);
        if(!ok) return default_invalid_value<T>();
        return _items[i].second;
    }

    T* try_get_2(StrName key) {
        bool ok; uint16_t i;
        HASH_PROBE_0(key, ok, i);
        if(!ok) return nullptr;
        return &_items[i].second;
    }

    bool contains(StrName key) const {
        bool ok; uint16_t i;
        HASH_PROBE_0(key, ok, i);
        return ok;
    }

    bool del(StrName key){
        bool ok; uint16_t i;
        HASH_PROBE_0(key, ok, i);
        if(!ok) return false;
        _items[i].first = StrName();
        // _items[i].second = PY_DELETED_SLOT;      // do not change .second if it is not zero, it means the slot is occupied by a deleted item
        _size--;
        return true;
    }

    template<typename __Func>
    void apply(__Func func) const {
        for(uint16_t i=0; i<_capacity; i++){
            if(_items[i].first.empty()) continue;
            func(_items[i].first, _items[i].second);
        }
    }

    std::vector<StrName> keys() const {
        std::vector<StrName> v;
        for(uint16_t i=0; i<_capacity; i++){
            if(_items[i].first.empty()) continue;
            v.push_back(_items[i].first);
        }
        return v;
    }

    void clear(){
        for(uint16_t i=0; i<_capacity; i++){
            _items[i].first = StrName();
            _items[i].second = nullptr;
        }
        _size = 0;
    }
#undef HASH_PROBE
#undef NAMEDICT_ALLOC
#undef _hash
};

template<typename V>
struct NameDictImpl{
    PK_ALWAYS_PASS_BY_POINTER(NameDictImpl)

    union{
        SmallNameDict<V> _small;
        LargeNameDict<V> _large;
    };

    NameDictImpl(): _small() {}
    NameDictImpl(float load_factor): _large(load_factor) {}

    bool is_small() const{
        const bool* p = reinterpret_cast<const bool*>(this);
        return *p;
    }

    void set(StrName key, V val){
        if(is_small()){
            bool ok = _small.try_set(key, val);
            if(!ok){
                SmallNameDict<V> copied(_small);
                // move to large name dict
                new (&_large) LargeNameDict<V>();
                copied.apply([&](StrName key, V val){
                    _large.set(key, val);
                });
                _large.set(key, val);
            }
        }else{
            _large.set(key, val);
        }
    }

    uint16_t size() const{ return is_small() ?_small.size() : _large.size(); }
    uint16_t capacity() const{ return is_small() ?_small.capacity() : _large.capacity(); }
    V operator[](StrName key) const { return is_small() ?_small[key] : _large[key]; }
    V try_get(StrName key) const { return is_small() ?_small.try_get(key) : _large.try_get(key); }
    V* try_get_2(StrName key) { return is_small() ?_small.try_get_2(key) : _large.try_get_2(key); }
    bool contains(StrName key) const { return is_small() ?_small.contains(key) : _large.contains(key); }
    bool del(StrName key){ return is_small() ?_small.del(key) : _large.del(key); }

    void clear(){
        if(is_small()) _small.clear();
        else _large.clear();
    }

    template<typename Func>
    void apply(Func func) const {
        if(is_small()) _small.apply(func);
        else _large.apply(func);
    }

    void _try_perfect_rehash(){
        if(is_small()) return;
        _large._try_perfect_rehash();
    }

    std::vector<StrName> keys() const{
        std::vector<StrName> v;
        apply([&](StrName key, V val){
            v.push_back(key);
        });
        return v;
    }

    std::vector<std::pair<StrName, V>> items() const{
        std::vector<std::pair<StrName, V>> v;
        apply([&](StrName key, V val){
            v.push_back({key, val});
        });
        return v;
    }

    ~NameDictImpl(){
        if(!is_small()) _large.~LargeNameDict<V>();
    }
};

using NameDict = NameDictImpl<PyObject*>;
using NameDict_ = std::shared_ptr<NameDict>;
using NameDictInt = NameDictImpl<int>;

static_assert(sizeof(NameDict) <= 128);

} // namespace pkpy