#pragma once

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
        HashTableValue(ConcurrentHashTable<KeyType, ValType>* hash_table, const KeyType* key)
        {
            printf("HashTableValue\r\n");
            m_hash_table = hash_table; m_key = key;
        }
        ~HashTableValue()
        {
            printf("~HashTableValue\r\n");
        }

        operator ValType() const { ValueType& val = m_hash_table->at(*m_key); }
        HashTableValue& operator = (ValType const& val)
        {
            printf("HashTableValue& operator\r\n");
            m_hash_table->insert(*m_key, val); return *this;
        }
        // move constructor to avoid HashTableValue local object destruction in hashtable [] operator
        //HashTableValue(HashTableValue&& other)
        //{
        //    cout << "move constructor\r\n" << endl;
        //    m_hash_table = other.m_hash_table;  m_key = other.m_key;
        //    other.m_hash_table = nullptr;       other.m_key = nullptr;
        //}

    private:
        ConcurrentHashTable<KeyType, ValType>* m_hash_table;
        const KeyType* m_key;
    };

    // hash table item structure
    struct Item
    {
        KeyType key;
        ValType val;
        Item* next;
        Item(const KeyType& k, const ValType& v) { key = k; val = v; next = nullptr; }
    };

public:
    // constructor/destructor
    ConcurrentHashTable(size_t capacity = 31, float load_factor = 0.5);
    ~ConcurrentHashTable();

    // data access methods
    size_t size() const { return m_size; }
    ValType& at(const KeyType &key);
    void insert(const KeyType& key, const ValType& val);
    void erase(const KeyType& key);
    void clear();
    HashTableValue<KeyType, ValType>& operator [](const KeyType &key)
    {
        printf("HashTableValue::operator []\r\n");
        return HashTableValue<KeyType, ValType>(this, &key);
    }

    // data
    Item** m_items;
    size_t m_size;
    size_t m_capacity;
    float m_load_factor;

    // rehash
    void rehash();
};

// constructor
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::ConcurrentHashTable(size_t capacity, float load_factor)
{
    m_items = new Item*[capacity];
    for (size_t i = 0; i < capacity; ++i)
        m_items[i] = 0;

    m_size = 0;
    m_capacity = capacity;
    m_load_factor = load_factor;
}

// destructor
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::~ConcurrentHashTable()
{
    // free hash table items
    for (size_t i = 0; i < m_capacity; ++i)
    {
        if (m_items[i])
            delete m_items[i];
    }

    delete[] m_items;
}

// get item by key
template <class KeyType, class ValType>
ValType& ConcurrentHashTable<KeyType, ValType>::at(const KeyType &key)
{
    // get hash code
    std::hash<KeyType> hash_func;
    size_t hash_code = hash_func(key) % m_capacity;

    // get item
    Item* item = m_items[hash_code];
    if (!item)
        throw std::out_of_range();

    // find item with given key
    while (item)
    {
        if (item->key == key)
            return item->val;

        item = item->next;
    }

    return 0;
}

// insert item
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::insert(const KeyType& key, const ValType& val)
{
    // get hash code
    std::hash<KeyType> hash_func;
    size_t hash_code = hash_func(key) % m_capacity;

    // get item
    Item* item = m_items[hash_code];
    if (item)
    {
        // find item with given key, update value if found
        while (item)
        {
            if (item->key == key)
            {
                // found, update value
                item->val = val;
                return;
            }

            // end of chain reached and given key isn't found
            // put a new item in the tail of chain
            if (!item->next)
            {
                item->next = new Item(key, val);
                m_size++;
            }

            item = item->next;
        }
    }
    else
    {
        m_items[hash_code] = new Item(key, val);
        m_size++;
    }

    // rehash if load factor is exceeded
    if (m_size / m_capacity > m_load_factor)
        rehash();
}

// delete item
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::erase(const KeyType& key)
{
    // get hash code
    std::hash<KeyType> hash_func;
    size_t hash_code = hash_func(key) % m_capacity;

    // get item
    Item* item = m_items[hash_code];
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
                    m_items[hash_code] = 0;

                delete item;
                m_size--;
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
    for (size_t i = 0; i < m_size; ++i)
    {
        if (m_items[i])
        {
            delete m_items[i];
            m_items[i] = 0;
        }
    }

    m_size = 0;
}

// rehash
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::rehash()
{
}
