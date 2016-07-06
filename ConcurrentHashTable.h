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
        operator ValType() const                        { return _hash_table.at(_key);                 }
        HashTableValue& operator = (const ValType& val) { _hash_table.insert(_key, val); return *this; }
        bool operator == (const ValType& val) { return _hash_table.at(_key) == val; }

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
    size_t size() const noexcept { return _size; }
    size_t capacity() const noexcept { return _capacity; }
    bool contains(const KeyType &key) const noexcept;
    const ValType& at(const KeyType &key);
    void insert(const KeyType& key, const ValType& val) noexcept;
    void erase(const KeyType& key) noexcept;
    void clear() noexcept;
    HashTableValue<KeyType, ValType> operator [](const KeyType& key) noexcept { return HashTableValue<KeyType, ValType>(*this, key); }

private:
    struct Item
    {
        KeyType _key;
        ValType _val;
        Item* _next = nullptr;
        Item(const KeyType& key, const ValType& val) noexcept : _key(key), _val(val) {}
    };

    Item** _items;                                          // hashtable items
    size_t _size = 0;                                       // hashtable items number
    size_t _capacity;                                       // hashtable capacity
    float _max_load_factor;                                 // hashtable maximal load factor
    float _capacity_step;                                   // capacity increase coefficient
    float _lock_factor;                                     // hashtable items number to item mutexes number ratio
    mutable std::shared_mutex _global_mutex;                // global entire hashtable level mutex
    mutable std::deque<std::shared_mutex> _items_mutexes;   // items mutexes collection to lock hashtable on particular item level
    bool _rehashing = false;                                // flag to indicate rehashing process

    // auxiliary methods
    bool get_item(const KeyType& key) const noexcept;
    bool get_item(const KeyType& key, Item**& item, std::shared_mutex*& item_mutex) const noexcept;
    bool get_item(const KeyType& key, Item**& item, std::shared_mutex*& item_mutex, Item*& prev_item) const noexcept;
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
    std::shared_lock<std::shared_mutex>(_global_mutex, std::defer_lock);
    return get_item(key);
}

// get item by key
// @key - value key
template <class KeyType, class ValType>
const ValType& ConcurrentHashTable<KeyType, ValType>::at(const KeyType &key)
{
    std::shared_lock<std::shared_mutex>(_global_mutex, std::defer_lock);

    // get item related data
    // return value if found or throw an exception otherwise
    Item** item;
    std::shared_mutex* item_mutex;
    if (get_item(key, item, item_mutex))
        return (*item)->_val;
    else
        throw std::out_of_range("Key not found");
}

// insert item
// this function might be called while rehashing (it might be detected with _rehashing flag)
// if this is the case, we shouldn't lock resources as we are already under global lock
// @key - key of item to be inserted
// @val - value of item to be inserted
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::insert(const KeyType& key, const ValType& val) noexcept
{
    try_rehash(); // try to rehash table

    if (!_rehashing) // lock entire hashtable if this function is not caused by rehashing process
        _global_mutex.lock();

    // get item related data
    Item** item;
    std::shared_mutex* item_mutex;
    bool item_found = get_item(key, item, item_mutex);

    if (!item_found)
    {
        _size++;
        try_add_mutex(); // items number increased, check whether we should add a new mutex
    }

    // since this moment we don't need the global lock anymore, so lock the particular item and release global lock
    std::unique_lock<std::shared_mutex> item_lock(*item_mutex, std::defer_lock);

    if (!_rehashing)
        _global_mutex.unlock();

    // update value if found or insert a new item if not found
    if (item_found)
        (*item)->_val = val;
    else
        *item = new Item(key, val);
}

// delete item
// @key - value key
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::erase(const KeyType& key) noexcept
{
    _global_mutex.lock();

    // get item related data
    Item** item;
    Item* prev_item;
    std::shared_mutex* item_mutex;
    if (!get_item(key, item, item_mutex, prev_item))
    {
        _global_mutex.unlock();
        return;
    }

    _size--;

    // delete item from chain
    if (prev_item)
        prev_item->_next = (*item)->_next;

    // since this moment we don't need the global lock anymore, so lock the particular item (if found) and release global lock
    std::unique_lock<std::shared_mutex> item_lock(*item_mutex, std::defer_lock);
    _global_mutex.unlock();

    delete *item;
    *item = nullptr;
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

// get item by key
// must be called under global lock
// @key         searchable item key
// @item_mutex  will contain item mutex
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::get_item(const KeyType& key) const noexcept
{
    Item** dummy1;
    std::shared_mutex* dummy2;
    Item* dummy3;
    return get_item(key, dummy1, dummy2, dummy3);
}

// get item by key
// must be called under global lock
// @key         searchable item key
// @item        will contain item pointer reference if item found and new item pointer reference otherwise
// @item_mutex  will contain item mutex
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::get_item(const KeyType& key, Item**& item, std::shared_mutex*& item_mutex) const noexcept
{
    Item* dummy;
    return get_item(key, item, item_mutex, dummy);
}

// get item by key
// must be called under global lock
// @key         searchable item key
// @item        will contain item pointer reference if item found and new item pointer reference otherwise
// @item_mutex  will contain item mutex
// @prev_item   will contain previous item reference
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::get_item(const KeyType &key,
                                                     Item**& item,
                                                     std::shared_mutex*& item_mutex,
                                                     Item*& prev_item) const noexcept
{
    bool res = false;
    prev_item = nullptr;

    std::hash<KeyType> hash_func;
    size_t item_idx = hash_func(key) % _capacity;

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

    // set item mutex regardless of whether we found an item or not
    size_t lock_idx = item_idx % _items_mutexes.size();
    item_mutex = &_items_mutexes[lock_idx];

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
            insert(old_item->_key, old_item->_val);
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
// must be called under global lock
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::try_add_mutex() noexcept
{
    if ((float)_size / _lock_factor >= _items_mutexes.size())
        _items_mutexes.emplace_back();
}
