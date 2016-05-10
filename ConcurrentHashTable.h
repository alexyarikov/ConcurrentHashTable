#pragma once

// Concurrent (thread safe) hash table class
// If item with specified key not found exception will be thrown.
template <class KeyType, class ValType>
class ConcurrentHashTable
{
public:
    // constructor/destructor
    ConcurrentHashTable(const size_t capacity = 31, const float max_load_factor = 0.5, const float capacity_step = 2.0);
    ~ConcurrentHashTable();

    // data access methods
    size_t size() const { return _size; }
    bool contains(const KeyType &key) const;
    ValType& at(const KeyType &key);
    void insert(const KeyType& key, const ValType& val) { operator[](key) = val; };
    void erase(const KeyType& key);
    void clear();
    ValType& operator [](const KeyType& key);

private:
    struct Item
    {
        KeyType _key;
        ValType _val;
        Item* _next;
        Item(const KeyType& key) { _key = key; _next = nullptr; }
    }

    Item** _items;
    size_t _size;
    size_t _capacity;
    float _max_load_factor;
    float _capacity_step;
    mutable std::shared_mutex _rehash_lock;         // rehashing mutex is used to lock the whole hashtable while rehashing
    mutable std::shared_mutex _hashtable_lock;      // hashtable mutex is used to lock all items while insert/delete operations
    std::vector<std::shared_mutex> _items_locks;    // items locks are used to lock particular item

    bool at(const KeyType& key, Item** item_pptr) const;
    void rehash();
};

// constructor
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::ConcurrentHashTable(const size_t capacity, const float max_load_factor, const float capacity_step) :
    _size(0),
    _capacity(capacity),
    _max_load_factor(max_load_factor),
    _capacity_step(capacity_step),
    _items_locks(nullptr),
    _items_locks_size(0)
{
    _items = new Item*[_capacity];
    for (size_t i = 0; i < _capacity; ++i)
        _items[i] = nullptr;
}

// destructor
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::~ConcurrentHashTable()
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
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::contains(const KeyType &key) const
{
    Item* dummy;
    return at(key, &dummy);
}

// get item by key
template <class KeyType, class ValType>
ValType& ConcurrentHashTable<KeyType, ValType>::at(const KeyType &key)
{
    Item* item;
    if (at(key, &item))
        return item->val;
    else
        throw std::out_of_range("Key not found");
}

// get item value by key, insert a new default constructed item if key not found
template <class KeyType, class ValType>
ValType& ConcurrentHashTable<KeyType, ValType>::operator [](const KeyType& key)
{
    // rehash if load factor is exceeded
    if (_size / _capacity > _max_load_factor)
        rehash();

    // get hash code
    std::hash<KeyType> hash_func;
    size_t hash_code = hash_func(key) % _capacity;

    // get item
    Item* item = _items[hash_code];
    if (item)
    {
        // find item with given key, update value if found
        while (item)
        {
            if (item->key == key)
                break;

            if (!item->next)
            {
                // end of chain reached and given key isn't found, put a new item in the tail of chain
                item->next = new Item(key);
                item = item->next;
                _size++;

                _items_locks[_items_locks++] = new std::shared_mutex();

                break;
            }

            item = item->next;
        }
    }
    else
    {
        // there are no items with given key, insert a new one
        item = new Item(key);
        _items[hash_code] = item;
        _items_locks[_items_locks++] = new std::shared_mutex();
        _size++;
    }

    return item->val;
}

// delete item
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::erase(const KeyType& key)
{
    // get hash code
    std::hash<KeyType> hash_func;
    size_t hash_code = hash_func(key) % _capacity;

    // get item
    Item* item = _items[hash_code];
    if (item)
    {
        // find item with given key, delete if found
        Item *prev = 0;
        while (item)
        {
            if (item->key == key)
            {
                // found, delete item
                if (prev)
                    prev->next = item->next;
                else
                    _items[hash_code] = 0;

                delete item;
                _size--;
                return;
            }

            prev = item;
            item = item->next;
        }
    }
}

// delete all items
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::clear()
{
    for (size_t i = 0; i < _size; ++i)
    {
        if (_items[i])
        {
            delete _items[i];
            _items[i] = 0;
        }
    }

    _size = 0;
}

// get item by key
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::at(const KeyType &key, Item** item_pptr) const
{
    if (!item_pptr)
        throw std::invalid_argument("ConcurrentHashTable<KeyType, ValType>::at: invalid item provided");

    // lock the whole hashtable while rehashing
    std::shared_lock<std::shared_mutex> lock(_lock);

    // get hash code
    std::hash<KeyType> hash_func;
    size_t hash_code = hash_func(key) % _capacity;

    // get item
    Item* item = _items[hash_code];

    // find item with given key
    while (item)
    {
        if (item->key == key)
        {
            *item_pptr = item;
            return true;
        }

        item = item->next;
    }

    *item_pptr = nullptr;
    return false;
}

// rehash
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::rehash()
{
    // lock the whole hashtable while rehashing
    std::unique_lock<std::shared_mutex> lock(_lock);

    // save old capacity and data
    size_t old_capacity = _capacity;
    Item** old_data = _items;

    // clear size, increase capacity and allocate a new hash table
    _size = 0;
    _capacity *= _capacity_step;
    _items = new Item*[_capacity];

    // copy elements with freeing old items
    for (size_t i = 0; i < old_capacity; ++i)
    {
        Item* old_item = old_items[i];
        while (old_item)
        {
            insert(old_item->key, old_item->val);
            delete old_item;
            old_item = old_item->next;
        }
    }

    // free old items
    delete[] items;
}
