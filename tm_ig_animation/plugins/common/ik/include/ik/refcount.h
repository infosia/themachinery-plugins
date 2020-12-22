#pragma once

#include "ik/config.h"
#include <assert.h>

C_BEGIN

/* TODO: Move this somewhere else and don't fix it to 64-bit */
#define IK_ALIGN_TO_CPU_WORD_SIZE(offset) \
        (((offset) & 0x7) == 0 ? (offset) : ((offset) & ~0x7) + 8)

/*!
 * All structures that are refcounted must have the following fields present.
 * This way, all refcounted objects can be cast to ik_refcounted_t.
 */
#define IK_REFCOUNTED_HEAD                                                    \
        struct ik_refcount* refcount;

/*!
 * Get the number of references of a refcount allocated memory block.
 */
#define IK_REFCOUNT(o) \
        ((o)->refcount->refs)

/*!
 * Get the number of objects in a refcount allocated memory block.
 */
#define IK_REFCOUNTED_OBJS(o) \
        ((o)->refcount->obj_count)

/*!
 * Adds a reference to a refcount allocated memory block.
 */
#define IK_INCREF(o) do {                                                     \
        (o)->refcount->refs++;                                                \
    } while(0)

#define IK_INCDECREF(o) do {                                                  \
        IK_INCREF(o);                                                         \
        IK_DECREF(o);                                                         \
    } while(0)

/*!
 * Removes a reference from a refcount allocated memory block. If the refcount
 * reaches 0, then the registered deinit function is called for all objects
 * in the block before freeing the memory.
 */
#define IK_DECREF(o) do {                                                     \
        struct ik_refcount* refcount = (o)->refcount;                         \
        assert(refcount->refs > 0);                                           \
        if (--(refcount->refs) == 0)                                          \
        {                                                                     \
            uint32_t decref_i;                                                \
            for (decref_i = 0; refcount->obj_count--; decref_i++)             \
                refcount->deinit((o) + decref_i);                             \
                /* XXX: This only works if o is the correct type, which       \
                    * should be the case */                                   \
            ik_refcount_free(refcount);                                       \
        }                                                                     \
    } while(0)

/*!
 * Identical to IK_INCREF with an additional NULL-check.
 */
#define IK_XINCREF(o) do {                                                    \
        if (o)                                                                \
            IK_INCREF(o);                                                     \
    } while(0)

/*!
 * Identical to IK_DECREF with an additional NULL-check.
 */
#define IK_XDECREF(o) do {                                                    \
        if (o)                                                                \
            IK_DECREF(o);                                                     \
    } while(0)

/*!
 * When the refcount of a block of memory reaches 0, a callback function of
 * this type is called.
 */
typedef void (*ik_deinit_func)(void*);

struct ik_refcount
{
    /* Handler for freeing data managed by the refcounted object */
    ik_deinit_func deinit;
    /* Reference count */
    uint32_t       refs;
    /* Number of contiguous objects pointing to this refcount */
    uint32_t       obj_count;
};

struct ik_refcounted
{
    IK_REFCOUNTED_HEAD
};

/*!
 * @brief Allocates a refcounted block of memory of the specified size.
 * @param[out] refcounted_obj Pointer to the beginning of the usable memory.
 * @param[in] bytes The number of bytes to allocate.
 * @param[in] deinit The function to call before freeing the block of memory.
 * When deinit is called, the *refcounted_obj address is passed to it as an
 * argument.
 *
 * The actual number of bytes allocated will be the requested number of bytes
 * plus the size of the ik_refcount_t structure, which is placed at the
 * beginning of the block of memory. To make this detail transparent to the
 * user, the returned pointer will point to the beginning of the usable memory,
 * after the refcount header.
 *
 *   Beginning of memory block
 *           |
 *           v_____________ _______________________
 *           | ik_refcount | N number of bytes ...
 *                           ^
 *                           |
 *                returned pointer points here
 */
IK_PRIVATE_API struct ik_refcounted*
ik_refcounted_alloc(uintptr_t bytes,
                    ik_deinit_func deinit);

/*!
 * @brief Allocates a refcounted block of memory of the specified size
 * array_length times.
 * @param[out] refcounted_obj Pointer to the beginning of the usable memory.
 * @param[in] bytes The number of bytes to allocate.
 * @param[in] deinit The function to call before freeing the block of memory.
 * When deinit is called, the *refcounted_obj address is passed to it as an
 * argument.
 *
 * The actual number of bytes allocated will be the requested number of bytes
 * multiplied by array_length plus the size of the ik_refcount structure,
 * which is placed at the beginning of the block of memory. To make this detail
 * transparent to the user, the returned pointer will point to the beginning of
 * the usable memory, after the refcount header.
 *
 *   Beginning of memory block
 *           |
 *           v_____________ _______ _______ _______ ______
 *           | ik_refcount | bytes | bytes | bytes | ...
 *                           ^
 *                           |
 *                returned pointer points here
 *
 * The deinit function will be called array_length number of times when the
 * refcount is decremented to 0, once for every contiguous block of bytes.
 * The pointer to the beginning of each block is passed to the deinit function.
 * This is similar to how the delete[] operator in C++ works and allows multiple
 * objects with deinit functions to share the same refcount.
 */
IK_PRIVATE_API struct ik_refcounted*
ik_refcounted_alloc_array(uintptr_t obj_size,
                          ik_deinit_func deinit,
                          uint32_t obj_count);

IK_PRIVATE_API void
ik_refcounted_obj_free(struct ik_refcounted* refcounted_obj);

IK_PUBLIC_API void
ik_refcount_free(struct ik_refcount* refcount);

/*!
 * Returns the beginning of the memory block of a refcounted object.
 *
 * @warning In the case of an array of refcounted objects, this only works when
 * given the first object in that array, because it calcultes the base address
 * using a fixed offset. There is no way to get the base address of a refcounted
 * object in an array in general.
 */
IK_PRIVATE_API struct ik_refcount*
ik_refcounted_obj_base_address(struct ik_refcounted* refcounted_obj);

IK_PRIVATE_API struct ik_refcounted*
ik_refcount_to_first_obj_address(struct ik_refcount* refcount);

C_END
