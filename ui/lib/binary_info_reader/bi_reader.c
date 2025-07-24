#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pico/binary_info.h>
#include <binary_info_reader.h>

struct binary_info_descriptor
{
    uint32_t marker_start;
    uint32_t start;
    uint32_t end;
    uint32_t mapping_table;
    uint32_t marker_end;
};

const struct binary_info_descriptor *pr_bi_desc_find(void)
{
    for (uint32_t ptr = XIP_BASE; ptr < XIP_BASE + 512; ptr += 4) {
        const struct binary_info_descriptor *desc = (const struct binary_info_descriptor *)ptr;

        if (desc->marker_start == BINARY_INFO_MARKER_START
            && desc->marker_end == BINARY_INFO_MARKER_END)
            return desc;
    }

    return NULL;
}

uint32_t pr_bi_start(const struct binary_info_descriptor *desc)
{
    return desc->start;
}

uint32_t pr_bi_next(const struct binary_info_descriptor *desc,
                      uint32_t state)
{
    if (state && state < desc->end)
        return state + 4;
    return 0;
}

const binary_info_core_t *pr_bi_get(const struct binary_info_descriptor *desc,
                                      uint32_t state)
{
    if (state && state < desc->end)
        return *(binary_info_core_t * const *)state;
    return NULL;
}

const binary_info_core_t *pr_bi_find(const struct binary_info_descriptor *desc,
                                       uint32_t *state,
                                       uint16_t type, uint16_t tag)
{
    uint32_t default_state = 0;

    if (!state)
        state = &default_state;

    if (*state == 0)
        *state = pr_bi_start(desc);

    while (*state) {
        const binary_info_core_t *c = pr_bi_get(desc, *state);

        *state = pr_bi_next(desc, *state);

        if (c && c->type == type && c->tag == tag)
            return c;
    }

    return NULL;
}

// BINARY_INFO_TYPE_RAW_DATA
const void *pr_bi_find_raw(const struct binary_info_descriptor *desc,
                             uint32_t *state,
                             uint16_t tag)
{
    const binary_info_raw_data_t *bi;

    bi = (const binary_info_raw_data_t *)
        pr_bi_find(desc, state, BINARY_INFO_TYPE_RAW_DATA, tag);
    if (!bi)
        return NULL;

    return bi->bytes;
}

// BINARY_INFO_TYPE_SIZED_DATA
const void *pr_bi_find_sized_data(const struct binary_info_descriptor *desc,
                                    uint32_t *state,
                                    uint16_t tag, uint32_t *length)
{
    const binary_info_sized_data_t *bi;

    bi = (const binary_info_sized_data_t *)
        pr_bi_find(desc, state, BINARY_INFO_TYPE_SIZED_DATA, tag);
    if (!bi)
        return NULL;

    *length = bi->length;
    return bi->bytes;
}

// BINARY_INFO_TYPE_ID_AND_INT
int32_t pr_bi_find_int(const struct binary_info_descriptor *desc,
                         uint32_t *state,
                         uint16_t tag, uint32_t id)
{
    uint32_t default_state = 0;

    if (!state)
        state = &default_state;

    do {
        const binary_info_id_and_int_t *bi;

        bi = (const binary_info_id_and_int_t *)
            pr_bi_find(desc, state, BINARY_INFO_TYPE_ID_AND_INT, tag);
        if (!bi)
            return -1;

        if (bi->id == id)
            return bi->value;
    } while (state && *state);

    return -1;
}

// BINARY_INFO_TYPE_ID_AND_STRING
const char *pr_bi_find_string(const struct binary_info_descriptor *desc,
                                uint32_t *state,
                                uint16_t tag, uint32_t id)
{
    uint32_t default_state = 0;

    if (!state)
        state = &default_state;

    do {
        const binary_info_id_and_string_t *bi;

        bi = (const binary_info_id_and_string_t *)
            pr_bi_find(desc, state, BINARY_INFO_TYPE_ID_AND_STRING, tag);
        if (!bi)
            return NULL;

        if (bi->id == id)
            return bi->value;
    } while (state && *state);

    return NULL;
}

const char *pr_binary_info_program_name(void)
{
    const struct binary_info_descriptor *desc = pr_bi_desc_find();
    if (!desc)
        return NULL;

    return pr_bi_find_string(desc, NULL,
                               BINARY_INFO_TAG_RASPBERRY_PI,
                               BINARY_INFO_ID_RP_PROGRAM_NAME);
}

const char *pr_binary_info_build_date(void)
{
    const struct binary_info_descriptor *desc = pr_bi_desc_find();
    if (!desc)
        return NULL;

    return pr_bi_find_string(desc, NULL,
                               BINARY_INFO_TAG_RASPBERRY_PI,
                               BINARY_INFO_ID_RP_PROGRAM_BUILD_DATE_STRING);
}

const char *pr_binary_info_board_name(void)
{
    const struct binary_info_descriptor *desc = pr_bi_desc_find();
    if (!desc)
        return NULL;

    return pr_bi_find_string(desc, NULL,
                               BINARY_INFO_TAG_RASPBERRY_PI,
                               BINARY_INFO_ID_RP_PICO_BOARD);
}

