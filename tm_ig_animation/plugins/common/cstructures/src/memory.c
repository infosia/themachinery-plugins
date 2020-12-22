#include "cstructures/memory.h"
#include "cstructures/hashmap.h"
#include "cstructures/hash.h"
#include "cstructures/backtrace.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define BACKTRACE_OMIT_COUNT 2

#if defined(CSTRUCTURES_MEMORY_DEBUGGING)
static uintptr_t g_allocations = 0;
static uintptr_t d_deallocations = 0;
static uintptr_t g_ignore_hm_malloc = 0;
uintptr_t g_bytes_in_use = 0;
uintptr_t g_bytes_in_use_peak = 0;
static struct cs_hashmap g_report;

typedef struct report_info_t
{
    void* location;
    uintptr_t size;
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
    int backtrace_size;
    char** backtrace;
#   endif
} report_info_t;

/* ------------------------------------------------------------------------- */
int
memory_init(void)
{
    g_allocations = 0;
    d_deallocations = 0;
    g_bytes_in_use = 0;
    g_bytes_in_use_peak = 0;

    /*
     * Hashmap will call malloc during init, need to ignore this to avoid
     * crashing.
     */
    g_ignore_hm_malloc = 1;
        if (hashmap_init_with_options(&g_report,
                                      sizeof(void*),
                                      sizeof(report_info_t),
                                      4096,
                                      hash32_ptr) != HM_OK)
            return -1;
    g_ignore_hm_malloc = 0;

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
cstructures_malloc(uintptr_t size)
{
    void* p = NULL;
    report_info_t info = {0};

    /* allocate */
    p = malloc(size);
    if (p == NULL)
        return NULL;

    ++g_allocations;

    /*
     * Record allocation info. Call to hashmap and get_backtrace() may allocate
     * memory, so set flag to ignore the call to malloc() when inserting.
     */
    if (!g_ignore_hm_malloc)
    {
        g_bytes_in_use += size;
        if (g_bytes_in_use_peak < g_bytes_in_use)
            g_bytes_in_use_peak = g_bytes_in_use;

        g_ignore_hm_malloc = 1;

            /* record the location and size of the allocation */
            info.location = p;
            info.size = size;

            /* if (enabled, generate a backtrace so we know where memory leaks
            * occurred */
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            if (!(info.backtrace = get_backtrace(&info.backtrace_size)))
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
            if (size == 0)
            {
                int i;
                fprintf(stderr, "[memory] WARNING: malloc(0)");
                fprintf(stderr, "  -----------------------------------------\n");
                fprintf(stderr, "  backtrace to where malloc() was called:\n");
                for (i = 0; i < info.backtrace_size; ++i)
                    fprintf(stderr, "      %s\n", info.backtrace[i]);
                fprintf(stderr, "  -----------------------------------------\n");
            }
#   else
            if (size == 0)
                fprintf(stderr, "[memory] WARNING: malloc(0)");
#   endif

            /* insert info into hashmap */
            if (hashmap_insert(&g_report, &p, &info) != HM_OK)
                fprintf(stderr, "[memory] Hashmap insert failed\n");

        g_ignore_hm_malloc = 0;
    }

    /* success */
    return p;
}

/* ------------------------------------------------------------------------- */
void*
cstructures_realloc(void* p, uintptr_t new_size)
{
    void* old_p = p;
    p = realloc(p, new_size);

    /* If old pointer is NULL, this behaves the same as a malloc */
    if (old_p == NULL)
    {
        ++g_allocations;
    }
    else
    {
        /* Remove old entry in report */
        report_info_t* info = (report_info_t*)hashmap_erase(&g_report, &old_p);
        if (info)
        {
            g_bytes_in_use -= info->size;

#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            if (info->backtrace)
                free(info->backtrace);
            else
                fprintf(stderr, "[memory] WARNING: free(): Allocation didn't "
                    "have a backtrace (it was NULL)\n");
#   endif
        }
        else
        {
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            char** bt;
            int bt_size, i;
            fprintf(stderr, "  -----------------------------------------\n");
#   endif
            fprintf(stderr, "[memory] WARNING: realloc(): Reallocating something that was never malloc'd");
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            if ((bt = get_backtrace(&bt_size)))
            {
                fprintf(stderr, "  backtrace to where realloc() was called:\n");
                for (i = 0; i < bt_size; ++i)
                    fprintf(stderr, "      %s\n", bt[i]);
                fprintf(stderr, "  -----------------------------------------\n");
                free(bt);
            }
            else
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
#   endif
        }
    }

    /*
     * Record allocation info. Call to hashmap and get_backtrace() may allocate
     * memory, so set flag to ignore the call to malloc() when inserting.
     */
    if (!g_ignore_hm_malloc)
    {
        report_info_t info = {0};

        g_bytes_in_use += new_size;
        if (g_bytes_in_use_peak < g_bytes_in_use)
            g_bytes_in_use_peak = g_bytes_in_use;

        g_ignore_hm_malloc = 1;

            /* record the location and size of the allocation */
            info.location = p;
            info.size = new_size;

            /* if (enabled, generate a backtrace so we know where memory leaks
            * occurred */
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            if (!(info.backtrace = get_backtrace(&info.backtrace_size)))
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
#   endif

            /* insert info into hashmap */
            if (hashmap_insert(&g_report, &p, &info) != HM_OK)
                fprintf(stderr, "[memory] Hashmap insert failed\n");

        g_ignore_hm_malloc = 0;
    }

    return p;
}

/* ------------------------------------------------------------------------- */
void
cstructures_free(void* p)
{
    /* find matching allocation and remove from hashmap */
    if (!g_ignore_hm_malloc)
    {
        report_info_t* info = (report_info_t*)hashmap_erase(&g_report, &p);
        if (info)
        {
            g_bytes_in_use -= info->size;
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            if (info->backtrace)
                free(info->backtrace);
            else
                fprintf(stderr, "[memory] WARNING: free(): Allocation didn't "
                    "have a backtrace (it was NULL)\n");
#   endif
        }
        else
        {
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            char** bt;
            int bt_size, i;
            fprintf(stderr, "  -----------------------------------------\n");
#   endif
            fprintf(stderr, "  WARNING: Freeing something that was never allocated\n");
#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            if ((bt = get_backtrace(&bt_size)))
            {
                fprintf(stderr, "  backtrace to where free() was called:\n");
                for (i = 0; i < bt_size; ++i)
                    fprintf(stderr, "      %s\n", bt[i]);
                fprintf(stderr, "  -----------------------------------------\n");
                free(bt);
            }
            else
                fprintf(stderr, "[memory] WARNING: Failed to generate backtrace\n");
#   endif
        }
    }

    if (p)
    {
        ++d_deallocations;
        free(p);
    }
    else
        fprintf(stderr, "Warning: free(NULL)\n");
}

/* ------------------------------------------------------------------------- */
uintptr_t
memory_deinit(void)
{
    uintptr_t leaks;

    --g_allocations; /* this is the single allocation still held by the report hashmap */

    fprintf(stderr, "=========================================\n");
    fprintf(stderr, "Memory Report\n");
    fprintf(stderr, "=========================================\n");

    /* report details on any g_allocations that were not de-allocated */
    if (hashmap_count(&g_report) != 0)
    {
        HASHMAP_FOR_EACH(&g_report, void*, report_info_t, key, info)

            fprintf(stderr, "  un-freed memory at %p, size %p\n", (void*)info->location, (void*)info->size);
            mutated_string_and_hex_dump((void*)info->location, info->size);

#   if defined(CSTRUCTURES_MEMORY_BACKTRACE)
            fprintf(stderr, "  Backtrace to where malloc() was called:\n");
            {
                intptr_t i;
                for (i = BACKTRACE_OMIT_COUNT; i < info->backtrace_size; ++i)
                    fprintf(stderr, "      %s\n", info->backtrace[i]);
            }
            free(info->backtrace); /* this was allocated when malloc() was called */
            fprintf(stderr, "  -----------------------------------------\n");
#   endif

        HASHMAP_END_EACH

        fprintf(stderr, "=========================================\n");
    }

    /* overall report */
    leaks = (g_allocations > d_deallocations ? g_allocations - d_deallocations : d_deallocations - g_allocations);
    fprintf(stderr, "allocations: %lu\n", g_allocations);
    fprintf(stderr, "deallocations: %lu\n", d_deallocations);
    fprintf(stderr, "memory leaks: %lu\n", leaks);
    fprintf(stderr, "peak memory usage: %lu bytes\n", g_bytes_in_use_peak);
    fprintf(stderr, "=========================================\n");

    ++g_allocations; /* this is the single allocation still held by the report hashmap */
    g_ignore_hm_malloc = 1;
        hashmap_deinit(&g_report);
    g_ignore_hm_malloc = 0;

    return leaks;
}

/* ------------------------------------------------------------------------- */
uintptr_t
memory_get_num_allocs(void)
{
    return hashmap_count(&g_report);
}

/* ------------------------------------------------------------------------- */
uintptr_t
memory_get_memory_usage(void)
{
    return g_bytes_in_use;
}

#else /* CSTRUCTURES_MEMORY_DEBUGGING */

int memory_init(void)                   { return 0; }
uintptr_t memory_deinit(void)           { return 0; }
uintptr_t memory_get_num_allocs(void)   { return 0; }
uintptr_t memory_get_memory_usage(void) { return 0; }

#endif /* CSTRUCTURES_MEMORY_DEBUGGING */

/* ------------------------------------------------------------------------- */
void
mutated_string_and_hex_dump(const void* data, uintptr_t length_in_bytes)
{
    char* dump;
    uintptr_t i;

    /* allocate and copy data into new buffer */
    if (!(dump = malloc(length_in_bytes + 1)))
    {
        fprintf(stderr, "[memory] WARNING: Failed to malloc() space for dump\n");
        return;
    }
    memcpy(dump, data, length_in_bytes);
    dump[length_in_bytes] = '\0';

    /* mutate null terminators into dots */
    for (i = 0; i != length_in_bytes; ++i)
        if (dump[i] == '\0')  /* valgrind will complain about conditional jump depending on uninitialized value here -- that's ok */
            dump[i] = '.';

    /* dump */
    fprintf(stderr, "  mutated string dump: %s\n", dump);
    fprintf(stderr, "  hex dump: ");
    for (i = 0; i != length_in_bytes; ++i)
        fprintf(stderr, " %02x", (unsigned char)dump[i]);
    fprintf(stderr, "\n");

    free(dump);
}
