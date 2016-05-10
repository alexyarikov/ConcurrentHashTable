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
    void insert(const std::pair<KeyType, ValType>& val) { operator[](val.first) = val.second; };
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
    };

    Item** _items;
    size_t _size;
    size_t _capacity;
    float _max_load_factor;
    float _capacity_step;
    bool get_item(const KeyType& key, Item**& item, Item*& prev_item) const;
    void rehash();
};

// constructor
// @capacity - initial hashtable capacity
// @max_load_factor - maximal hashtable load factor, used to determine that rehashing is needed
// @capacity_step - capacity step, used to increase capacity while rehashing
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::ConcurrentHashTable(const size_t capacity, const float max_load_factor, const float capacity_step) :
    _size(0),
    _capacity(capacity),
    _max_load_factor(max_load_factor),
    _capacity_step(capacity_step)
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
// @key - value key
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::contains(const KeyType &key) const
{
    Item** dummy1;
    Item* dummy2;
    return get_item(key, dummy1, dummy2);
}

// get item by key
// @key - value key
template <class KeyType, class ValType>
ValType& ConcurrentHashTable<KeyType, ValType>::at(const KeyType &key)
{
    Item** item;
    Item* dummy;
    if (get_item(key, item, dummy))
        return (*item)->_val;
    else
        throw std::out_of_range("Key not found");
}

// get item value by key, insert a new default constructed item if key not found
// @key - value key
template <class KeyType, class ValType>
ValType& ConcurrentHashTable<KeyType, ValType>::operator [](const KeyType& key)
{
    // rehash if load factor is exceeded
    if ((double)_size / (double)_capacity > _max_load_factor)
        rehash();

    // get item by key
    Item** item;
    Item* dummy;
    if (!get_item(key, item, dummy))
    {
        *item = new Item(key);
        _size++;
    }

    return (*item)->_val;
}

// delete item
// @key - value key
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::erase(const KeyType& key)
{
    // get item by key
    Item** item;
    Item* prev_item;
    if (get_item(key, item, prev_item))
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
void ConcurrentHashTable<KeyType, ValType>::clear()
{
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
// @key         searchable item key
// @item        will contain item pointer reference if item found and new item pointer reference otherwise
// @prev_item   will contain previous item reference
template <class KeyType, class ValType>
bool ConcurrentHashTable<KeyType, ValType>::get_item(const KeyType &key, Item**& item, Item*& prev_item) const
{
    bool res = false;
    prev_item = nullptr;

    // get hash code
    std::hash<KeyType> hash_func;
    size_t hash_code = hash_func(key) % _capacity;

    item = &_items[hash_code];

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

// rehash
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::rehash()
{
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
}
