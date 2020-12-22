#include "cstructures/btree.h"
#include "cstructures/memory.h"
#include <assert.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
static enum cs_btree_status
btree_realloc(struct cs_btree* btree, cs_btree_size new_capacity)
{
    /* clamp to minimum configured capacity */
    if (new_capacity < CSTRUCTURES_BTREE_MIN_CAPACITY)
        new_capacity = CSTRUCTURES_BTREE_MIN_CAPACITY;

    /*
     * If btree hasn't allocated anything yet, just allocated the requested
     * amount of memory and return immediately.
     */
    if (!btree->data)
    {
        btree->data = MALLOC(new_capacity * BTREE_KV_SIZE(btree));
        if (!btree->data)
            return BTREE_OOM;
        btree->capacity = new_capacity;
        return BTREE_OK;
    }

    /*
     * If the new capacity is larger than the old capacity, then we realloc
     * before shifting around the data.
     */
    if (new_capacity >= btree->capacity)
    {
        void* new_data = REALLOC(btree->data, new_capacity * BTREE_KV_SIZE(btree));
        if (!new_data)
            return BTREE_OOM;
        btree->data = new_data;
    }

    /*
     * The keys are correctly placed in memory, but now that the capacity has
     * grown, the values need to be moved forwards in the data buffer
     */
    /*if (insertion_index == BTREE_INVALID_KEY)*/
    {
        cs_btree_size old_capacity = btree->capacity;
        void* old_values = BTREE_VALUE_BEG_CAP(btree, old_capacity);
        void* new_values = BTREE_VALUE_BEG_CAP(btree, new_capacity);
        memmove(new_values, old_values, old_capacity * btree->value_size);
    }
    /* no longer needed?
    else
    {
        cs_btree_size old_capacity = btree->capacity;
        void* old_lower_values = BTREE_VALUE_BEG_CAP(btree, old_capacity);
        void* new_lower_values = BTREE_VALUE_BEG_CAP(btree, new_capacity);
        void* old_upper_values = BTREE_VALUE_CAP(btree, insertion_index, old_capacity);
        void* new_upper_values = BTREE_VALUE_CAP(btree, insertion_index + 1, new_capacity);
        cs_btree_size lower_values_to_move = insertion_index;
        cs_btree_size upper_values_to_move = old_capacity - insertion_index;
        cs_btree_key* old_upper_keys = BTREE_KEY(btree, insertion_index);
        cs_btree_key* new_upper_keys = BTREE_KEY(btree, insertion_index + 1);
        cs_btree_size keys_to_move = old_capacity - insertion_index;

        memmove(new_upper_values, old_upper_values, upper_values_to_move * btree->value_size);
        memmove(new_lower_values, old_lower_values, lower_values_to_move * btree->value_size);
        memmove(new_upper_keys, old_upper_keys, keys_to_move);
    }*/

    /*
     * If the new capacity is smaller than the old capacity, we have to realloc
     * after moving around the data as to not read from memory out of bounds
     * of the buffer.
     */
    if (new_capacity < btree->capacity)
    {
        void* new_data = REALLOC(btree->data, new_capacity * BTREE_KV_SIZE(btree));
        if (new_data)
            btree->data = new_data;
        else
        {
            /*
             * This should really never happen, but if the realloc to a smaller
             * size fails, the btree will be in a consistent state if the
             * capacity is updated to the new capacity.
             */
        }
    }

    btree->capacity = new_capacity;

    return BTREE_OK;
}

/* ------------------------------------------------------------------------- */
enum cs_btree_status
btree_create(struct cs_btree** btree, uint32_t value_size)
{
    *btree = MALLOC(sizeof **btree);
    if (*btree == NULL)
        return BTREE_OOM;
    btree_init(*btree, value_size);
    return BTREE_OK;
}

/* ------------------------------------------------------------------------- */
void
btree_init(struct cs_btree* btree, uint32_t value_size)
{
    assert(btree);
    btree->data = NULL;
    btree_count(btree) = 0;
    btree->capacity = 0;
    btree->value_size = value_size;
}

/* ------------------------------------------------------------------------- */
void btree_deinit(struct cs_btree* btree)
{
    assert(btree);
    btree_clear(btree);
    btree_compact(btree);
}

/* ------------------------------------------------------------------------- */
void
btree_free(struct cs_btree* btree)
{
    assert(btree);
    btree_deinit(btree);
    FREE(btree);
}

/* ------------------------------------------------------------------------- */
enum cs_btree_status btree_reserve(struct cs_btree* btree, cs_btree_size size)
{
    if (btree->capacity < size)
    {
        enum cs_btree_status status;
        if ((status = btree_realloc(btree, size)) != BTREE_OK)
            return status;
    }

    return BTREE_OK;
}

/* ------------------------------------------------------------------------- */
/* algorithm taken from GNU GCC stdlibc++'s lower_bound function, line 2121 in stl_algo.h */
/* https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a02014.html */
/*
 * 1) If the key exists, then a pointer to that key is returned.
 * 2) If the key does not exist, then the first valid key who's value is less
 *    than the key being searched for is returned.
 * 3) If there is no key who's value is less than the searched-for key, the
 *    returned pointer will point to the address after the last valid key in
 *    the array.
 */
static cs_btree_key*
btree_find_lower_bound(const struct cs_btree* btree, cs_btree_key key)
{
    cs_btree_size half;
    cs_btree_key* middle;
    cs_btree_key* found;
    cs_btree_size len;

    assert(btree);

    found = BTREE_KEY(btree, 0);  /* start search at key index 0 */
    len = btree_count(btree);

    while (len > 0)
    {
        half = len >> 1;
        middle = found + half;
        if (*middle < key)
        {
            found = middle;
            ++found;
            len = len - half - 1;
        }
        else
            len = half;
    }

    return found;
}

/* ------------------------------------------------------------------------- */
enum cs_btree_status
btree_insert_new(struct cs_btree* btree, cs_btree_key key, const void* value)
{
    cs_btree_key* lower_bound;
    cs_btree_size insertion_index;
    cs_btree_size entries_to_move;
    enum cs_btree_status status;

    assert(btree);

    /* May need to realloc */
    if (BTREE_NEEDS_REALLOC(btree))
        if ((status = btree_realloc(btree, btree->capacity * CSTRUCTURES_BTREE_EXPAND_FACTOR)) != BTREE_OK)
            return status;

    /* lookup location in btree to insert */
    lower_bound = btree_find_lower_bound(btree, key);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
        return BTREE_EXISTS;
    insertion_index = BTREE_KEY_TO_IDX(btree, lower_bound);

    /* Move entries out of the way to make space for new entry */
    entries_to_move = btree_count(btree) - insertion_index;
    memmove(lower_bound + 1, lower_bound, entries_to_move * sizeof(cs_btree_key));
    memmove(BTREE_VALUE(btree, insertion_index + 1), BTREE_VALUE(btree, insertion_index), entries_to_move * btree->value_size);

    /* Copy key/value into storage */
    memcpy(BTREE_KEY(btree, insertion_index), &key, sizeof(cs_btree_key));
    if (btree->value_size)
        memcpy(BTREE_VALUE(btree, insertion_index), value, btree->value_size);
    btree->count++;

    return BTREE_OK;
}

/* ------------------------------------------------------------------------- */
enum cs_btree_status
btree_set_existing(struct cs_btree* btree, cs_btree_key key, const void* value)
{
    void* found;
    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    if (btree->value_size == 0)
        return BTREE_OK;

    if ((found = btree_find(btree, key)) == NULL)
        return BTREE_NOT_FOUND;

    memcpy(found, value, btree->value_size);
    return BTREE_OK;
}

/* ------------------------------------------------------------------------- */
enum cs_btree_status
btree_insert_or_get(struct cs_btree* btree, cs_btree_key key, const void* value, void** inserted_value)
{
    cs_btree_key* lower_bound;
    cs_btree_size insertion_index;
    cs_btree_size entries_to_move;
    enum cs_btree_status status;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);
    assert(inserted_value);

    /* May need to realloc */
    if (BTREE_NEEDS_REALLOC(btree))
        if ((status = btree_realloc(btree, btree->capacity * CSTRUCTURES_BTREE_EXPAND_FACTOR)) != BTREE_OK)
            return status;

    /* lookup location in btree to insert */
    lower_bound = btree_find_lower_bound(btree, key);
    insertion_index = BTREE_KEY_TO_IDX(btree, lower_bound);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
    {
        /*memcpy(BTREE_VALUE(btree, insertion_index), *value, btree->value_size);*/
        *inserted_value = BTREE_VALUE(btree, insertion_index);
        return BTREE_EXISTS;
    }

    /* Move entries out of the way to make space for new entry */
    entries_to_move = btree_count(btree) - insertion_index;
    memmove(lower_bound + 1, lower_bound, entries_to_move * sizeof(cs_btree_key));
    memmove(BTREE_VALUE(btree, insertion_index + 1), BTREE_VALUE(btree, insertion_index), entries_to_move * btree->value_size);

    memcpy(BTREE_KEY(btree, insertion_index), &key, sizeof(cs_btree_key));
    memcpy(BTREE_VALUE(btree, insertion_index), value, btree->value_size);
    *inserted_value = BTREE_VALUE(btree, insertion_index);
    btree->count++;

    return BTREE_NOT_FOUND;
}

/* ------------------------------------------------------------------------- */
void*
btree_find(const struct cs_btree* btree, cs_btree_key key)
{
    cs_btree_key* lower_bound;
    cs_btree_size idx;

    assert(btree);
    assert(btree->value_size > 0);

    lower_bound = btree_find_lower_bound(btree, key);
    if (lower_bound >= BTREE_KEY_END(btree) || *lower_bound != key)
        return NULL;

    idx = BTREE_KEY_TO_IDX(btree, lower_bound);
    return BTREE_VALUE(btree, idx);
}

/* ------------------------------------------------------------------------- */
static cs_btree_size
btree_find_index_of_matching_value(const struct cs_btree* btree, const void* value)
{
    void* current_value;
    cs_btree_size i;

    for (i = 0, current_value = BTREE_VALUE_BEG(btree);
         i != btree_count(btree);
        ++i, current_value = BTREE_VALUE(btree, i))
    {
        if (memcmp(current_value, value, btree->value_size) == 0)
            return i;
    }

    return (cs_btree_size)-1;
}

/* ------------------------------------------------------------------------- */
cs_btree_key*
btree_find_key(const struct cs_btree* btree, const void* value)
{
    cs_btree_size i;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    if ((i = btree_find_index_of_matching_value(btree, value)) == (cs_btree_size)-1)
        return NULL;

    return BTREE_KEY(btree, i);
}

/* ------------------------------------------------------------------------- */
int
btree_find_and_compare(const struct cs_btree* btree, cs_btree_key key, const void* value)
{
    void* inserted_value;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    inserted_value = btree_find(btree, key);
    if (inserted_value == NULL)
        return 0;

    return memcmp(inserted_value, value, btree->value_size) == 0;
}

/* ------------------------------------------------------------------------- */
void*
btree_get_any_value(const struct cs_btree* btree)
{
    assert(btree);
    assert(btree->value_size > 0);

    if (btree_count(btree) == 0)
        return NULL;
    return BTREE_VALUE(btree, 0);
}

/* ------------------------------------------------------------------------- */
int
btree_key_exists(struct cs_btree* btree, cs_btree_key key)
{
    cs_btree_key* lower_bound;

    assert(btree);

    lower_bound = btree_find_lower_bound(btree, key);
    if (lower_bound < BTREE_KEY_END(btree) && *lower_bound == key)
        return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
cs_btree_key
btree_find_unused_key(struct cs_btree* btree)
{
    cs_btree_key key = 0;

    assert(btree);

    if (btree->data)
        while (*BTREE_KEY(btree, key) == key)
            key++;

    return key;
}

/* ------------------------------------------------------------------------- */
cs_btree_key
btree_erase_index(struct cs_btree* btree, cs_btree_size idx)
{
    cs_btree_key* lower_bound;
    cs_btree_key key;
    cs_btree_size entries_to_move;

    lower_bound = BTREE_KEY(btree, idx);
    key = *lower_bound;
    entries_to_move = btree_count(btree) - idx;
    memmove(lower_bound, lower_bound+1, entries_to_move * sizeof(cs_btree_key));
    memmove(BTREE_VALUE(btree, idx), BTREE_VALUE(btree, idx+1), entries_to_move * btree->value_size);
    btree->count--;

    return key;
}

/* ------------------------------------------------------------------------- */
enum cs_btree_status
btree_erase(struct cs_btree* btree, cs_btree_key key)
{
    cs_btree_key* lower_bound;

    assert(btree);

    lower_bound = btree_find_lower_bound(btree, key);
    if (lower_bound >= BTREE_KEY_END(btree) || *lower_bound != key)
        return BTREE_NOT_FOUND;

    btree_erase_index(btree, BTREE_KEY_TO_IDX(btree, lower_bound));

    return BTREE_OK;
}

/* ------------------------------------------------------------------------- */
cs_btree_key
btree_erase_value(struct cs_btree* btree, const void* value)
{
    cs_btree_size idx;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);

    idx = btree_find_index_of_matching_value(btree, value);
    if (idx == (cs_btree_size)-1)
        return BTREE_INVALID_KEY;

    return btree_erase_index(btree, idx);
}

/* ------------------------------------------------------------------------- */
cs_btree_key
btree_erase_internal_value(struct cs_btree* btree, const void* value)
{
    cs_btree_size idx;

    assert(btree);
    assert(btree->value_size > 0);
    assert(value);
    assert(BTREE_VALUE_BEG(btree) <= value);
    assert(value < BTREE_VALUE_END(btree));

    idx = BTREE_VALUE_TO_IDX(btree, value);
    return btree_erase_index(btree, idx);
}

/* ------------------------------------------------------------------------- */
void
btree_clear(struct cs_btree* btree)
{
    assert(btree);

    btree->count = 0;
}

/* ------------------------------------------------------------------------- */
void
btree_compact(struct cs_btree* btree)
{
    assert(btree);

    if (btree_count(btree) == 0)
    {
        XFREE(btree->data);
        btree->data = NULL;
        btree->capacity = 0;
    }
    else
    {
        btree_realloc(btree, btree_count(btree));
    }
}
