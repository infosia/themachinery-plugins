#pragma once

#include "cstructures/config.h"
#include "cstructures/hash.h"

#define HM_SLOT_UNUSED    ((cs_hash32)0)
#define HM_SLOT_RIP       ((cs_hash32)1)
#define HM_SLOT_INVALID   ((cs_hash32)-1)
#define HM_REHASH_AT_PERCENT   70
#define HM_DEFAULT_TABLE_COUNT 128
#define HM_EXPAND_FACTOR 3

C_BEGIN

enum cs_hashmap_status
{
    HM_EXISTS = 1,
    HM_OK = 0,
    HM_OOM = -1
};

struct cs_hashmap
{
    uint32_t     table_count;
    uint32_t     key_size;
    uint32_t     value_size;
    uint32_t     slots_used;
    hash32_func  hash;
    void*        storage;
#ifdef CSTRUCTURES_HASHMAP_STATS
    struct {
        uintptr_t total_insertions;
        uintptr_t total_deletions;
        uintptr_t total_tombstones;
        uintptr_t total_tombstone_reuses;
        uintptr_t total_rehashes;
        uintptr_t total_insertion_probes;
        uintptr_t total_deletion_probes;
        uintptr_t max_slots_used;
        uintptr_t max_slots_tombstoned;
        uint32_t current_tombstone_count;
    } stats;
#endif
};

/*!
 * @brief Allocates and initializes a new hashmap.
 * @param[out] hm A pointer to the new hashmap is written to this parameter.
 * Example:
 * ```cpp
 * struct hashmap_t* hm;
 * if (hashmap_create(&hm, sizeof(key_t), sizeof(value_t)) != CSTRUCTURES_OK)
 *     handle_error();
 * ```
 * @param[in] key_size Specifies how many bytes of the "key" parameter to hash
 * in the hashmap_insert() call. Due to performance reasons, all keys are
 * identical in size. If you wish to use strings for keys, then you need to
 * specify the maximum possible string length here, and make sure you never
 * use strings that are longer than that (hashmap_insert_key contains a safety
 * check in debug mode for this case).
 * @note This parameter must be larger than 0.
 * @param[in] value_size Specifies how many bytes long the value type is. When
 * calling hashmap_insert(), value_size number of bytes are copied from the
 * memory pointed to by value into the hashmap.
 * @note This parameter may be 0.
 * @return If successful, returns CSTRUCTURES_OK. If allocation fails,
 * CSTRUCTURES_ERR_OUT_OF_MEMORY is returned.
 */
CSTRUCTURES_PRIVATE_API enum cs_hashmap_status
hashmap_create(struct cs_hashmap** hm,
               uint32_t key_size,
               uint32_t value_size);

CSTRUCTURES_PRIVATE_API enum cs_hashmap_status
hashmap_create_with_options(struct cs_hashmap** hm,
                            uint32_t key_size,
                            uint32_t value_size,
                            uint32_t table_count,
                            hash32_func hash_func);

/*!
 * @brief Initializes a new hashmap. See hashmap_create() for details on
 * parameters and return values.
 */
CSTRUCTURES_PRIVATE_API enum cs_hashmap_status
hashmap_init(struct cs_hashmap* hm,
             uint32_t key_size,
             uint32_t value_size);

CSTRUCTURES_PRIVATE_API enum cs_hashmap_status
hashmap_init_with_options(struct cs_hashmap* hm,
                          uint32_t key_size,
                          uint32_t value_size,
                          uint32_t table_count,
                          hash32_func hash_func);

/*!
 * @brief Cleans up internal resources without freeing the hashmap object itself.
 */
CSTRUCTURES_PRIVATE_API void
hashmap_deinit(struct cs_hashmap* hm);

/*!
 * @brief Cleans up all resources and frees the hashmap.
 */
CSTRUCTURES_PRIVATE_API void
hashmap_free(struct cs_hashmap* hm);

CSTRUCTURES_PRIVATE_API enum cs_hashmap_status
hashmap_reserve(struct cs_hashmap* hm,
                uint32_t table_count);

/*!
 * @brief Inserts a key and value into the hashmap.
 * @note Complexity is generally O(1). Inserting may cause a rehash if the
 * table size exceeds HM_REHASH_AT_PERCENT.
 * @param[in] hm A pointer to a valid hashmap object.
 * @param[in] key A pointer to where the key is stored. key_size number of
 * bytes are hashed and copied into the hashmap from this location in
 * memory. See hashmap_create() on key_size.
 * @param[in] value A pointer to where the value is stored. value_size number
 * of bytes are copied from this location in memory into the hashmap. If
 * value_size is 0, then nothing is copied.
 * @return If the key already exists, then nothing is copied into the hashmap
 * and CSTRUCTURES_HASH_EXISTS is returned. If the key is successfully inserted, CSTRUCTURES_OK
 * is returned. If insertion failed, CSTRUCTURES_ERR_OUT_OF_MEMORY is returned.
 */
CSTRUCTURES_PRIVATE_API enum cs_hashmap_status
hashmap_insert(struct cs_hashmap* hm,
               const void* key,
               const void* value);

CSTRUCTURES_PRIVATE_API enum cs_hashmap_status
hashmap_insert_str(struct cs_hashmap* hm,
                   const char* key,
                   const void* value);

CSTRUCTURES_PRIVATE_API void*
hashmap_erase(struct cs_hashmap* hm,
              const void* key);

CSTRUCTURES_PRIVATE_API void*
hashmap_erase_str(struct cs_hashmap* hm,
                  const char* key,
                  enum cs_hashmap_status* status);

CSTRUCTURES_PRIVATE_API void*
hashmap_find(const struct cs_hashmap* hm, const void* key);

CSTRUCTURES_PRIVATE_API void*
hashmap_find_str(struct cs_hashmap* hm, const char* key);

#define hashmap_count(hm) ((hm)->slots_used)

#define HASHMAP_FOR_EACH(hm, key_t, value_t, key, value) { \
    key_t* key; \
    value_t* value; \
    cs_hash32 pos_##value; \
    for (pos_##value = 0; \
        pos_##value != (hm)->table_count && \
            ((key = (key_t*)((uint8_t*)(hm)->storage + (sizeof(cs_hash32) + (hm)->key_size) * pos_##value + sizeof(cs_hash32))) || 1) && \
            ((value = (value_t*)((uint8_t*)(hm)->storage + (sizeof(cs_hash32) + (hm)->key_size) * (hm)->table_count + (hm)->value_size * pos_##value)) || 1); \
        ++pos_##value) \
    { \
        cs_hash32 slot_##value = *(cs_hash32*)((uint8_t*)(hm)->storage + (sizeof(cs_hash32) + (hm)->key_size) * pos_##value); \
        if (slot_##value == HM_SLOT_UNUSED || slot_##value == HM_SLOT_RIP || slot_##value == HM_SLOT_INVALID) \
            continue; \


#define HASHMAP_END_EACH }}

C_END
