#ifndef PR_BINARY_INFO_READER_H
#define PR_BINARY_INFO_READER_H

#include <stdint.h>
#include <pico/binary_info.h>

struct binary_info_descriptor;

const struct binary_info_descriptor *pr_bi_desc_find(void);

uint32_t pr_bi_start(const struct binary_info_descriptor *desc);

uint32_t pr_bi_next(const struct binary_info_descriptor *desc,
                    uint32_t state);

const binary_info_core_t *pr_bi_get(const struct binary_info_descriptor *desc,
                                    uint32_t state);

const binary_info_core_t *pr_bi_find(const struct binary_info_descriptor *desc,
                                     uint32_t *state,
                                     uint16_t type, uint16_t tag);

// BINARY_INFO_TYPE_RAW_DATA
const void *pr_bi_find_raw(const struct binary_info_descriptor *desc,
                           uint32_t *state,
                           uint16_t tag);

// BINARY_INFO_TYPE_SIZED_DATA
const void *pr_bi_find_sized_data(const struct binary_info_descriptor *desc,
                                  uint32_t *state,
                                  uint16_t tag, uint32_t *length);

// BINARY_INFO_TYPE_ID_AND_INT
int32_t pr_bi_find_int(const struct binary_info_descriptor *desc,
                       uint32_t *state,
                       uint16_t tag, uint32_t id);

// BINARY_INFO_TYPE_ID_AND_STRING
const char *pr_bi_find_string(const struct binary_info_descriptor *desc,
                              uint32_t *state,
                              uint16_t tag, uint32_t id);

const char *pr_binary_info_program_name(void);
const char *pr_binary_info_build_date(void);
const char *pr_binary_info_board_name(void);

#endif
