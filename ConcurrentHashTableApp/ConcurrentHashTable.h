#pragma once

template <class KeyType, class ValType>
class ConcurrentHashTable
{
public:
	// constructor/destructor
	ConcurrentHashTable(size_t capacity = 31, double load_factor = 0.5);
	~ConcurrentHashTable();

	// container methods
	void insert(KeyType *key, ValType *val);
	void erase(const KeyType *key);
	void clear();

	size_t size() const { return m_size; }

private:
	// item structure
	struct Item
	{
		KeyType *key;
		ValType *val;
		Item *next;
		Item(KeyType *k, ValType *v) { key = k; val = v; next = 0; }
	};

	// data
	Item **m_items;
	size_t m_size;
	size_t m_capacity;
	double m_load_factor;

	// rehash
	void rehash();
};

// constructor
template <class KeyType, class ValType>
ConcurrentHashTable<KeyType, ValType>::ConcurrentHashTable(size_t capacity, double load_factor)
{
	m_items = new Item*[capacity];
	for (size_t i = 0; i < capacity; ++i)
		m_items[i] = NULL;

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

// insert item
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::insert(KeyType *key, ValType *val)
{
	if (!key || !val)
		throw std::invalid_argument("invalid key or data provided");

	// get hash code
	std::tr1::hash<KeyType> hash_func;
	size_t hash_code = hash_func(*key) % m_capacity;

	// do insert/update
	Item *item = m_items[hash_code];
	if (item)
	{
		// search item with the same key, update data
		while (item)
		{
			if (*item->key == *key)
			{
				item->val = val;
				break;
			}

			// end of chain reached and given key isn't found
			// put a new item in the tail of chain
			if (!item->next)
			{
				item->next = new Item(key, val);
				m_size++;
			}
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

// erase item
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::erase(const KeyType *key)
{
	if (!key || !val)
		throw std::invalid_argument("invalid key or data provided");

	// get hash code
	std::tr1::hash<KeyType> hash_func;
	size_t hash_code = hash_func(*key) % m_capacity;
}

// rehash
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::rehash()
{
}
