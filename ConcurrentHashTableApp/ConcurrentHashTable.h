#pragma once

template <class KeyType, class ValType>
class ConcurrentHashTable
{
public:
	// constructor/destructor
	ConcurrentHashTable(size_t capacity = 31, double load_factor = 0.5);
	~ConcurrentHashTable();

	// navigate methods
	const ValType& operator [](const KeyType &key) { return at(key); };
	ValType& operator [](const KeyType &key) const { return at(key); };

	const ValType& at(const KeyType &key) const;
	ValType& at(const KeyType &key);

	size_t size() const { return m_size; }

	// modify methods
	void insert(KeyType* key, ValType* val);
	void erase(const KeyType* key);
	void clear();

private:
	// item structure
	struct Item
	{
		KeyType* key;
		ValType* val;
		Item* next;
		Item(KeyType* k, ValType* v) { key = k; val = v; next = 0; }
	};

	// data
	Item** m_items;
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
	std::tr1::hash<KeyType> hash_func;
	size_t hash_code = hash_func(key) % m_capacity;

	// get item
	Item* item = m_items[hash_code];
	if (item)
	{
		// find item with given key
		while (item)
		{
			if (!item->key || !item->val)
				throw std::exception("data corrupted");

			if (*(item->key) == key)
				return *item->val;

			item = item->next;
		}
	}

	return 0;
}

// insert item
template <class KeyType, class ValType>
void ConcurrentHashTable<KeyType, ValType>::insert(KeyType* key, ValType* val)
{
	if (!key || !val)
		throw std::invalid_argument("invalid key or data provided");

	// get hash code
	std::tr1::hash<KeyType> hash_func;
	size_t hash_code = hash_func(*key) % m_capacity;

	// get item
	Item* item = m_items[hash_code];
	if (item)
	{
		// find item with given key, update value if found
		while (item)
		{
			if (!item->key)
				throw std::exception("data corrupted");

			if (*(item->key) == *key)
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
void ConcurrentHashTable<KeyType, ValType>::erase(const KeyType* key)
{
	if (!key)
		throw std::invalid_argument("invalid key provided");

	// get hash code
	std::tr1::hash<KeyType> hash_func;
	size_t hash_code = hash_func(*key) % m_capacity;

	// get item
	Item* item = m_items[hash_code];
	if (item)
	{
		// find item with given key, delete if found
		Item *prev = 0;
		while (item)
		{
			if (!item->key)
				throw std::exception("data corrupted");

			if (*(item->key) == *key)
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
