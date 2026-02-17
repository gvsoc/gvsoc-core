#pragma once

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>

#include <stdlib.h>
#include <stdint.h>

struct GvsocMemspec {
    uint access_size;
    uint nb_channels;
    uint nb_pseudo_channels;
    uint nb_ranks;
    uint nb_bank_groups;
    uint nb_banks;
    uint nb_rows;
    uint nb_columns;
    uint channel_stride;
    uint rank_stride;
    uint bankgroup_stride;
    uint bank_stride;
    uint row_stride;
    uint column_stride;
};

struct PimInfo {
    uint is_write;
    uint address;
    uint channel;
    uint rank;
    uint bankGroup;
    uint bank;
    uint row;
    uint column;
};

struct PimStride {
    PimInfo pimInfo;
    uint64_t base_addr;
    uint64_t length;
    uint64_t stride;
    uint64_t count;
    char* buf;
};