#include <string.h>
#include <assert.h>
#include "cstructures/vector.h"
#include "cstructures/memory.h"

#define VEC_INVALID_INDEX (cs_vec_idx)-1

#define VECTOR_NEEDS_REALLOC(x) \
        ((x)->count == (x)->capacity)

/* ----------------------------------------------------------------------------
 * Static functions
 * ------------------------------------------------------------------------- */

/*!
 * @brief Expands the underlying memory.
 *
 * This implementation will expand the memory by a factor of 2 each time this
 * is called. All elements are copied into the new section of memory.
 * @param[in] insertion_index Set to VEC_INVALID_INDEX if (no space should be
 * made for element insertion. Otherwise this parameter specifies the index of
 * the element to "evade" when re-allocating all other elements.
 * @param[in] new_count The number of elements to allocate memory for.
 * @note No checks are performed to make sure the target size is large enough.
 */
static int
vector_realloc(struct cs_vector *vector,
              cs_vec_idx insertion_index,
              cs_vec_size new_count);

/* ----------------------------------------------------------------------------
 * Exported functions
 * ------------------------------------------------------------------------- */
struct cs_vector*
vector_create(const cs_vec_size element_size)
{
    struct cs_vector* vector;
    if ((vector = MALLOC(sizeof *vector)) == NULL)
        return NULL;
    vector_init(vector, element_size);
    return vector;
}

/* ------------------------------------------------------------------------- */
void
vector_init(struct cs_vector* vector, const cs_vec_size element_size)
{
    assert(vector);
    memset(vector, 0, sizeof *vector);
    vector->element_size = element_size;
}

/* ------------------------------------------------------------------------- */
void
vector_deinit(struct cs_vector* vector)
{
    assert(vector);

    XFREE(vector->data);
}

/* ------------------------------------------------------------------------- */
void
vector_free(struct cs_vector* vector)
{
    assert(vector);
    vector_deinit(vector);
    FREE(vector);
}

/* ------------------------------------------------------------------------- */
void
vector_clear(struct cs_vector* vector)
{
    assert(vector);
    /*
     * No need to free or overwrite existing memory, just reset the counter
     * and let future insertions overwrite
     */
    vector->count = 0;
}

/* ------------------------------------------------------------------------- */
void
vector_compact(struct cs_vector* vector)
{
    assert(vector);

    if (vector->count == 0)
    {
        XFREE(vector->data);
        vector->data = NULL;
        vector->capacity = 0;
    }
    else
    {
        /* If this fails (realloc shouldn't fail when specifying a smaller size
         * but who knows) it doesn't really matter. The vector will be in an
         * unchanged state and functionally still be identical */
        vector_realloc(vector, VEC_INVALID_INDEX, vector_count(vector));
    }
}

/* ------------------------------------------------------------------------- */
void
vector_clear_compact(struct cs_vector* vector)
{
    assert(vector);

    XFREE(vector->data);
    vector->count = 0;
    vector->capacity = 0;
    vector->data = NULL;
}

/* ------------------------------------------------------------------------- */
int
vector_reserve(struct cs_vector* vector, cs_vec_size size)
{
    assert(vector);

    if (vector->capacity < size)
    {
        if (vector_realloc(vector, VEC_INVALID_INDEX, size) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int
vector_resize(struct cs_vector* vector, cs_vec_size size)
{
    assert(vector);

    if (vector_reserve(vector, size) != 0)
        return -1;

    vector->count = size;

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
vector_emplace(struct cs_vector* vector)
{
    void* emplaced;
    assert(vector);

    if (VECTOR_NEEDS_REALLOC(vector))
        if (vector_realloc(vector,
                           VEC_INVALID_INDEX,
                           vector_count(vector) * CSTRUCTURES_VEC_EXPAND_FACTOR) != 0)
            return NULL;

    emplaced = vector->data + (vector->element_size * vector->count);
    ++(vector->count);

    return emplaced;
}

/* ------------------------------------------------------------------------- */
int
vector_push(struct cs_vector* vector, const void* data)
{
    void* emplaced;

    assert(vector);
    assert(data);

    if ((emplaced = vector_emplace(vector)) == NULL)
        return -1;

    memcpy(emplaced, data, vector->element_size);

    return 0;
}

/* ------------------------------------------------------------------------- */
int
vector_push_vector(struct cs_vector* vector, const struct cs_vector* source_vector)
{
    assert(vector);
    assert(source_vector);

    /* make sure element sizes are equal */
    if (vector->element_size != source_vector->element_size)
        return -2;

    /* make sure there's enough space in the target vector */
    if (vector->count + source_vector->count > vector->capacity)
        if (vector_realloc(vector, VEC_INVALID_INDEX, vector->count + source_vector->count) != 0)
            return -1;

    /* copy data */
    memcpy(vector->data + (vector->count * vector->element_size),
           source_vector->data,
           source_vector->count * vector->element_size);
    vector->count += source_vector->count;

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
vector_pop(struct cs_vector* vector)
{
    assert(vector);

    if (!vector->count)
        return NULL;

    --(vector->count);
    return vector->data + (vector->element_size * vector->count);
}

/* ------------------------------------------------------------------------- */
void*
vector_back(const struct cs_vector* vector)
{
    assert(vector);

    if (!vector->count)
        return NULL;

    return vector->data + (vector->element_size * (vector->count - 1));
}

/* ------------------------------------------------------------------------- */
void*
vector_insert_emplace(struct cs_vector* vector, cs_vec_idx index)
{
    cs_vec_idx offset;

    assert(vector);

    /*
     * Normally the last valid index is (capacity-1), but in this case it's valid
     * because it's possible the user will want to insert at the very end of
     * the vector.
     */
    assert(index <= vector->count);

    /* re-allocate? */
    if (vector->count == vector->capacity)
    {
        if (vector_realloc(vector,
                           index,
                           vector_count(vector) * CSTRUCTURES_VEC_EXPAND_FACTOR) != 0)
            return NULL;
    }
    else
    {
        /* shift all elements up by one to make space for insertion */
        cs_vec_size total_size = vector->count * vector->element_size;
        offset = vector->element_size * index;
        memmove(vector->data + offset + vector->element_size,
                vector->data + offset,
                total_size - (cs_vec_size)offset);
    }

    /* element is inserted */
    ++vector->count;

    /* return pointer to memory of new element */
    return (void*)(vector->data + index * vector->element_size);
}

/* ------------------------------------------------------------------------- */
int
vector_insert(struct cs_vector* vector, cs_vec_idx index, void* data)
{
    void* emplaced;

    assert(vector);
    assert(data);

    if ((emplaced = vector_insert_emplace(vector, index)) == NULL)
        return -1;

    memcpy(emplaced, data, vector->element_size);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
vector_erase_index(struct cs_vector* vector, cs_vec_idx index)
{
    assert(vector);
    assert (index < vector->count);

    if (index == vector->count - 1)
        /* last element doesn't require memory shifting, just pop it */
        vector_pop(vector);
    else
    {
        /* shift memory right after the specified element down by one element */
        cs_vec_idx offset = vector->element_size * index;                  /* offset to the element being erased in bytes */
        cs_vec_size total_size = vector->element_size * vector->count;     /* total current size in bytes */
        memmove(vector->data + offset,                                    /* target is to overwrite the element specified by index */
                vector->data + offset + vector->element_size,             /* copy beginning from one element ahead of element to be erased */
                total_size - (cs_vec_size)offset - vector->element_size);  /* copying number of elements after element to be erased */
        --vector->count;
    }
}

/* ------------------------------------------------------------------------- */
void
vector_erase_element(struct cs_vector* vector, void* element)
{
    void* last_element;

    assert(vector);
    last_element = vector->data + (vector->count-1) * vector->element_size;
    assert(element);
    assert(element >= (void*)vector->data);
    assert(element <= (void*)last_element);

    if (element != (void*)last_element)
    {
        memmove(element,                         /* target is to overwrite the element */
                (uint8_t*)element + vector->element_size,  /* read everything from next element */
                (cs_vec_size)((uint8_t*)last_element - (uint8_t*)element));  /* ptr1 - ptr2 yields signed result, but last_element is always larger than element */
    }
    --vector->count;
}

/* ------------------------------------------------------------------------- */
void*
vector_get_element(const struct cs_vector* vector, cs_vec_idx index)
{
    assert(vector);
    assert(index < vector->count);
    return vector->data + index * vector->element_size;
}

/* ------------------------------------------------------------------------- */
cs_vec_idx
vector_find_element(const struct cs_vector* vector, const void* element)
{
    cs_vec_idx i;
    for (i = 0; i != vector_count(vector); ++i)
    {
        void* current_element = vector_get_element(vector, i);
        if (memcmp(current_element, element, vector->element_size) == 0)
            return i;
    }

    return vector_count(vector);
}

#define vector_get_scratch_element(vector) \
    ((vector)->data + (vector)->capacity * (vector)->element_size)

/* ------------------------------------------------------------------------- */
void
vector_reverse(struct cs_vector* vector)
{
    assert(vector);

    uint8_t* begin = vector->data;
    uint8_t* end = vector->data + (vector->count - 1) * vector->element_size;
    uint8_t* tmp = vector_get_scratch_element(vector);

    while (begin < end)
    {
        memcpy(tmp, begin, vector->element_size);
        memcpy(begin, end, vector->element_size);
        memcpy(end, tmp, vector->element_size);

        begin += vector->element_size;
        end -= vector->element_size;
    }
}

/* ----------------------------------------------------------------------------
 * Static functions
 * ------------------------------------------------------------------------- */
static int
vector_realloc(struct cs_vector *vector,
              cs_vec_idx insertion_index,
              cs_vec_size new_capacity)
{
    void* new_data;

    /*
     * If vector hasn't allocated anything yet, just allocated the requested
     * amount of memory and return immediately. Make space at end of buffer
     * for one scratch element, which is required for swapping elements in
     * vector_reverse().
     */
    if (!vector->data)
    {
        new_capacity = (new_capacity == 0 ? CSTRUCTURES_VEC_MIN_CAPACITY : new_capacity);
        vector->data = MALLOC((new_capacity + 1) * vector->element_size);
        if (!vector->data)
            return -1;
        vector->capacity = new_capacity;
        return 0;
    }

    /* Realloc the data. Make sure to have space for the swap element at the end */
    if ((new_data = REALLOC(vector->data, (new_capacity  + 1) * vector->element_size)) == NULL)
        return -1;
    vector->data = new_data;

    /* if no insertion index is required, copy all data to new memory */
    if (insertion_index != VEC_INVALID_INDEX)
    {
        void* old_upper_elements = vector->data + (insertion_index + 0) * vector->element_size;
        void* new_upper_elements = vector->data + (insertion_index + 1) * vector->element_size;
        cs_vec_size upper_element_count = (cs_vec_size)(vector->capacity - insertion_index);
        memmove(new_upper_elements, old_upper_elements, upper_element_count * vector->element_size);
    }

    vector->capacity = new_capacity;

    return 0;
}
