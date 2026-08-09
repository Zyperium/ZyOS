#pragma once
#include <lib/hash.hpp>

inline void* operator new(size_t, void* ptr) noexcept { return ptr; }
inline void* operator new[](size_t, void* ptr) noexcept { return ptr; }
inline void operator delete(void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}

namespace lib {
    template <typename K, typename V, typename HashFunc = Hash<K>>
    class umap {
    public:
        struct Entry {
            K key;
            V value;

            template <typename... Args>
            Entry(const K &k, Args&&... args)
                : key(k), value(static_cast<Args&&>(args)...) {}

            template <typename... Args>
            Entry(K &&k, Args&&... args)
                : key(static_cast<K&&>(k)), value(static_cast<Args&&>(args)...) {}
        };

    private:
        struct Node {
            Entry data;
            Node *next = nullptr;

            template <typename... Args>
            Node(Args&&... args) : data(static_cast<Args&&>(args)...) {}
        };

        Node **m_buckets      = nullptr;
        size_t m_bucket_count = 0;
        size_t m_size         = 0;
        HashFunc m_hasher;

        static Node **allocate_buckets(size_t count) {
            if (count == 0) return nullptr;
            Node **ptr = static_cast<Node**>(::operator new[](count * sizeof(Node*)));
            if (!ptr) return nullptr;
            for (size_t i = 0; i < count; ++i) {
                ptr[i] = nullptr;
            }
            return ptr;
        }

        static void free_buckets(Node **buckets) {
            if (buckets) {
                ::operator delete[](buckets);
            }
        }

        size_t get_bucket_index(const K &key, size_t bucket_count) const {
            size_t h = m_hasher(key);
            size_t idx = h % bucket_count;
            return idx;
        }

    public:
        umap() = default;

        explicit umap(size_t initial_buckets) {
            (void)rehash(initial_buckets);
        }

        ~umap() {
            clear();
            free_buckets(m_buckets);
        }

        umap(const umap&) = delete;
        umap &operator=(const umap&) = delete;

        umap(umap &&other) noexcept
            : m_buckets(other.m_buckets), m_bucket_count(other.m_bucket_count), 
              m_size(other.m_size), m_hasher(static_cast<HashFunc&&>(other.m_hasher)) {
            other.m_buckets = nullptr;
            other.m_bucket_count = 0;
            other.m_size = 0;
        }

        umap &operator=(umap &&other) noexcept {
            if (this != &other) {
                clear();
                free_buckets(m_buckets);

                m_buckets = other.m_buckets;
                m_bucket_count = other.m_bucket_count;
                m_size = other.m_size;
                m_hasher = static_cast<HashFunc&&>(other.m_hasher);

                other.m_buckets = nullptr;
                other.m_bucket_count = 0;
                other.m_size = 0;
            }
            return *this;
        }

        [[nodiscard]] bool rehash(size_t new_bucket_count) {
            if (new_bucket_count <= m_bucket_count) return true;

            Node **new_buckets = allocate_buckets(new_bucket_count);
            if (!new_buckets) {
                return false;
            }

            for (size_t i = 0; i < m_bucket_count; ++i) {
                Node *current = m_buckets[i];
                while (current) {
                    Node *next = current->next;
                    size_t new_idx = get_bucket_index(current->data.key, new_bucket_count);

                    current->next = new_buckets[new_idx];
                    new_buckets[new_idx] = current;

                    current = next;
                }
            }

            free_buckets(m_buckets);
            m_buckets = new_buckets;
            m_bucket_count = new_bucket_count;
            return true;
        }

        template <typename KeyArg, typename... Args>
        V *emplace_get(KeyArg &&key, Args &&... args) {
            if (m_bucket_count == 0) {
                if (!rehash(16)) return nullptr;
            }

            size_t idx = get_bucket_index(key, m_bucket_count);
            Node *current = m_buckets[idx];

            while (current) {
                if (current->data.key == key) {
                    current->data.value.~V();
                    ::new (static_cast<void*>(&current->data.value)) V(static_cast<Args&&>(args)...);
                    return &current->data.value;
                }
                current = current->next;
            }

            if (m_size >= m_bucket_count) {
                if (!rehash(m_bucket_count * 2)) return nullptr;
            }

            void *raw_mem = ::operator new(sizeof(Node));
            if (!raw_mem) {
                return nullptr;
            }

            Node *new_node = ::new (raw_mem) Node(static_cast<KeyArg&&>(key), static_cast<Args&&>(args)...);

            size_t final_idx = get_bucket_index(new_node->data.key, m_bucket_count);

            new_node->next = m_buckets[final_idx];
            m_buckets[final_idx] = new_node;
            m_size++;

            return &new_node->data.value;
        }

        template <typename KeyArg, typename... Args>
        [[nodiscard]] bool emplace(KeyArg &&key, Args&&... args) {
            return emplace_get(static_cast<KeyArg&&>(key), static_cast<Args&&>(args)...) != nullptr;
        }

        [[nodiscard]] bool insert(const K &key, const V &value) {
            return emplace(key, value);
        }

        [[nodiscard]] bool insert(K &&key, V &&value) {
            return emplace(static_cast<K&&>(key), static_cast<V&&>(value));
        }

        V *find(const K &key) {
            if (m_bucket_count == 0) {
                return nullptr;
            }

            size_t hash_val = m_hasher(key);
            size_t idx = hash_val % m_bucket_count;
            Node *current = m_buckets[idx];

            while (current) {
                if (current->data.key == key) {
                    return &current->data.value;
                }

                current = current->next;
            }
            return nullptr;
        }

        const V *find(const K &key) const {
            return const_cast<umap*>(this)->find(key);
        }

        bool contains(const K &key) const {
            return find(key) != nullptr;
        }

        bool remove(const K &key) {
            if (m_bucket_count == 0) return false;
            size_t idx = get_bucket_index(key, m_bucket_count);
            Node *current = m_buckets[idx];
            Node *prev = nullptr;

            while (current) {
                if (current->data.key == key) {
                    if (prev) {
                        prev->next = current->next;
                    } else {
                        m_buckets[idx] = current->next;
                    }

                    current->~Node();
                    ::operator delete(current);
                    --m_size;
                    return true;
                }
                prev = current;
                current = current->next;
            }
            return false;
        }

        void clear() {
            for (size_t i = 0; i < m_bucket_count; ++i) {
                Node *current = m_buckets[i];
                while (current) {
                    Node *next = current->next;
                    current->~Node();
                    ::operator delete(current);
                    current = next;
                }
                m_buckets[i] = nullptr;
            }
            m_size = 0;
        }

        size_t size() const { return m_size; }
        size_t bucket_count() const { return m_bucket_count; }
        bool empty() const { return m_size == 0; }

        V &operator[](const K &key) {
            V *val = emplace_get(key, V());
            return *val;
        }

        V &operator[](K &&key) {
            V *val = emplace_get(static_cast<K&&>(key), V());
            if (!val) {
            }
            return *val;
        }
    };
}