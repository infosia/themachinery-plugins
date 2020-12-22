#include "cstructures/string.h"
#include "cstructures/vector.h"
#include "cstructures/memory.h"
#include <assert.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
struct cs_string*
string_create(void)
{
    struct cs_string* str = MALLOC(sizeof *str);
    if (str == NULL)
        return NULL;

    string_init(str);
    return 0;
}

/* ------------------------------------------------------------------------- */
void
string_init(struct cs_string* str)
{
    vector_init(&str->buf, sizeof(char));
}

/* ------------------------------------------------------------------------- */
void
string_deinit(struct cs_string* str)
{
    vector_deinit(&str->buf);
}

/* ------------------------------------------------------------------------- */
void
string_destroy(struct cs_string* str)
{
    string_deinit(str);
    FREE(str);
}

/* ------------------------------------------------------------------------- */
int
string_getline(struct cs_string* str, FILE* fp)
{
    int ret;
    assert(fp);

    vector_clear(&str->buf);
    while ((ret = fgetc(fp)) != EOF)
    {
        char c = (char)ret;

        /* Ignore carriage */
        if (c == '\r')
            continue;

        /* If we encounter a newline, complete string and return */
        if (c == '\n')
        {
            /* empty line? */
            if (vector_count(&str->buf) == 0)
                continue;

            char nullterm = '\0';
            if (vector_push(&str->buf, &nullterm) != 0)
                return -1;
            return 1;
        }

        if (vector_push(&str->buf, &c) != 0)
            return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
char*
cs_stringok(struct cs_string* str, char delimiter, char** saveptr)
{
    char* begin_ptr;
    char* end_ptr;

    if(str)
        *saveptr = (char*)vector_data(&str->buf) - 1;

    /* no more tokens */
    if(!*saveptr)
        return NULL;

    /* get first occurrence of token in string */
    begin_ptr = *saveptr + 1;
    end_ptr = (char*)strchr(begin_ptr, delimiter);
    if(!end_ptr)
        *saveptr = NULL; /* last token has been reached */
    else
    {
        /* update saveptr and replace delimiter with null terminator */
        *saveptr = end_ptr;
        **saveptr = '\0';
    }

    /* empty tokens */
    if(*begin_ptr == '\0')
        return NULL;
    return begin_ptr;
}
