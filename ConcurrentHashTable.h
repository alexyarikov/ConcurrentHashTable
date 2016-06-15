#pragma once

// Concurrent (thread safe) hash table class
// If item with specified key not found exception will be thrown.
template <class KeyType, class ValType>
class ConcurrentHashTable
{
private:
    // hash table value class, intended to implement hash table [] operator
    // (to distinguish which action, either read or write, is performed under hashtable value)
    template <class KeyType, class ValType>
    class HashTableValue
    {
    public:
        HashTableValue(ConcurrentHashTable<KeyType, ValType>& hash_table, const KeyType& key) : _hash_table(hash_table), _key(key) {}
        operator ValType() const                        { return _hash_table.at(_key);                   }
        HashTableValue& operator = (const ValType& val) { _hash_table.insert({_key, val}); return *this; }

    private:
        ConcurrentHashTable& _hash_table;
        const KeyType& _key;
    };

public:
    // constructor/destructor
    ConcurrentHashTable(const size_t capacity = 31,
                        const float max_load_factor = 0.5,
                        const float capacity_step = 2.0,
                        const float lock_factor = (float)std::thread::hardware_concurrency()) noexcept;
    ~ConcurrentHashTable() noexcept;

    // data access methods
    size_t size() const  noexcept { return _size; }
    bool contains(const KeyType &key) const noexcept;
    const ValType& at(const KeyType &key);
    void insert(const std::pair<KeyType, ValType>& val);
    void erase(const KeyType& key) noexcept;
    void clear() noexcept;
    HashTableValue<KeyType, ValType> operator [](const KeyType& key) noexcept { return HashTableValue<KeyType, ValType>(*this, key); }

private:
    struct Item
    {
        KeyType _key;
        ValType _val;
        Item* _next;
        Item(const KeyType& key, const ValType& val) noexcept : _key(key), _val(val), _next(nullptr) {}
    };

    Item** _items;
    size_t _size = 0;
    size_t _capacity;
    float _max_load_factor;
    float _capacity_step;
    float _lock_factor;
    mutable std::shared_mutex _global_mutex;
    mutable std::deque<std::shared_mutex> _items_mutexes;
    bool _rehashing = false;

    size_t get_item_index(const KeyType& key) const noexcept;
    std::shared_mutex& get_item_mutex(const size_t item_idx) const noexcept;
    bool get_item(const size_t data_idx, const KeyType& key, Item**& item, Item*& prev_item) const noexcept;
    void try_rehash() noexcept;
    void try_add_mutex() noexcept;
};

// constructor
// @capacity - initial hashtable capacity
// @max_load_factor - maximal hashtable load factor, used to determine that rehashing is needed
// @capacity_step - capacity step, used to increase capacity while rehashing
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::ConcurrentHashTable(const size_t capacity,
                                                           const float max_load_factor,
                                                           const float capacity_step,
                                                           const float lock_factor) noexcept :
    _capacity(capacity),
    _max_load_factor(max_load_factor),
    _capacity_step(capacity_step),
    _lock_factor(lock_factor)
{
    _items = new Item*[_capacity];
    for (size_t i = 0; i < _capacity; ++i)
        _items[i] = nullptr;

    _items_mutexes.emplace_back();
}

// destructor
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::~ConcurrentHashTable() noexcept
{
    // free hash table items
    for (size_t i = 0; i < _capacity; ++i)
    {
        if (_items[i])
            delete _items[i];
    }

    delete[] _items;
}

// checks whether item with specified key exists
// @key - value key
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::contains(const KeyType &key) const noexcept
{
    // lock entire hashtable on read
    _global_mutex.lock_shared();

    // get item index and item lock
    size_t item_idx = get_item_index(key);
    std::shared_mutex& item_mutex = get_item_mutex(item_idx);

    // lock item and unlock entire hashtable
    std::shared_lock<std::shared_mutex> item_lock(item_mutex, std::defer_lock);
    _global_mutex.unlock_shared();

    // return whether item exists or not
    Item** dummy1;
    Item* dummy2;
    return get_item(item_idx, key, dummy1, dummy2);
}

// get item by key
// @key - value key
template <class KeyType, class ValType>
const ValType& ConcurrentHashTable<KeyType, ValType>::at(const KeyType &key)
{
    // lock entire hashtable on read
    _global_mutex.lock_shared();

    // get item index and item lock
    size_t item_idx = get_item_index(key);
    std::shared_mutex& item_mutex = get_item_mutex(item_idx);

    // lock item and unlock entire hashtable
    std::shared_lock<std::shared_mutex> item_lock(item_mutex, std::defer_lock);
    _global_mutex.unlock_shared();

    // return item value of throw exception if not found
    Item** item;
    Item* dummy;
    if (get_item(item_idx, key, item, dummy))
        return (*item)->_val;
    else
        throw std::out_of_range("Key not found");
}

// insert item
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::insert(const std::pair<KeyType, ValType>& val)
{
    // this function might be called during rehashing (it might be detected with _rehashing flag)
    // if this is the case, we shouldn't lock resources as there we are already under global lock

    // rehash if load factor is exceeded
    try_rehash();

    // add a new mutex if there are not enough mutexes
    try_add_mutex();

    if (!_rehashing)
        _global_mutex.lock();

    size_t item_idx = get_item_index(val.first);
    std::shared_mutex& item_mutex = get_item_mutex(item_idx);

    if (!_rehashing)
    {
        item_mutex.lock();
        _global_mutex.unlock();
    }

    // get item by key, update value if found, insert a new item if not found
    Item** item;
    Item* dummy;
    if (get_item(item_idx, val.first, item, dummy))
        (*item)->_val = val.second;
    else
    {
        *item = new Item(val.first, val.second);
        _size++;
    }

    if (!_rehashing)
        item_mutex.unlock();
}

// delete item
// @key - value key
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::erase(const KeyType& key) noexcept
{
    _global_mutex.lock();

    size_t item_idx = get_item_index(key);

    std::shared_mutex& item_mutex = get_item_mutex(item_idx);
    std::unique_lock<std::shared_mutex> item_lock(item_mutex, std::defer_lock);
    _global_mutex.unlock();

    // get item by key
    Item** item;
    Item* prev_item;
    if (get_item(item_idx, key, item, prev_item))
    {
        // delete from chain and free found item
        if (prev_item)
            prev_item->_next = (*item)->_next;
        delete *item;
        *item = nullptr;
        _size--;
    }
}

// delete all items
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::clear() noexcept
{
    std::unique_lock<std::shared_mutex> item_lock(_global_mutex, std::defer_lock);

    for (size_t i = 0; i < _size; ++i)
    {
        if (_items[i])
        {
            delete _items[i];
            _items[i] = nullptr;
        }
    }

    _size = 0;
}

// get item data index
template <class KeyType, class ValType>
size_t ConcurrentHashTable<KeyType, ValType>::get_item_index(const KeyType& key) const noexcept
{
    std::hash<KeyType> hash_func;
    return hash_func(key) % _capacity;
}

// get item data lock
// @item_idx    item index
template <class KeyType, class ValType>
std::shared_mutex& ConcurrentHashTable<KeyType, ValType>::get_item_mutex(const size_t item_idx) const noexcept
{
    size_t lock_idx = item_idx % _items_mutexes.size();
    return _items_mutexes[lock_idx];
}

// get item by key
// @data_idx    searchable item index
// @key         searchable item key
// @item        will contain item pointer reference if item found and new item pointer reference otherwise
// @prev_item   will contain previous item reference
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::get_item(const size_t item_idx,
                                                     const KeyType &key,
                                                     Item**& item,
                                                     Item*& prev_item) const noexcept
{
    bool res = false;
    prev_item = nullptr;

    item = &_items[item_idx];

    // find item with given key
    for (Item* i = *item; i; i = i->_next)
    {
        if (i->_key == key)
        {
            res = true;
            break;
        }

        item = &i->_next;
        prev_item = i;
    }

    return res;
}

// rehash if load factor is exceeded
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::try_rehash() noexcept
{
    if (_rehashing)
        return;

    std::unique_lock<std::shared_mutex> global_lock(_global_mutex, std::defer_lock);

    // check load factor
    if ((float)_size / (float)_capacity <= _max_load_factor)
        return;

    _rehashing = true;

    // save old capacity and data
    size_t old_capacity = _capacity;
    Item** old_items = _items;

    // clear size, increase capacity and allocate a new hash table
    _size = 0;
    _capacity = std::lroundf(_capacity * _capacity_step);
    _items = new Item*[_capacity];
    for (size_t i = 0; i < _capacity; ++i)
        _items[i] = nullptr;

    // copy elements with freeing old items
    for (size_t i = 0; i < old_capacity; ++i)
    {
        Item* old_item = old_items[i];
        while (old_item)
        {
            insert(std::make_pair(old_item->_key, old_item->_val));
            Item* next_item = old_item->_next;
            delete old_item;
            old_item = next_item;
        }
    }

    // free old items
    delete[] old_items;

    _rehashing = false;
}

// add mutex if there are not enough mutexes
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::try_add_mutex() noexcept
{
    std::unique_lock<std::shared_mutex> global_lock(_global_mutex, std::defer_lock);

    if ((float)_size / _lock_factor >= _items_mutexes.size())
        _items_mutexes.emplace_back();
}
