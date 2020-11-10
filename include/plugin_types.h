/**
* Copyright (C) Huawei Technologies Co., Ltd. 2020.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#if !defined (_PLUGIN_TYPES_H)
#define _PLUGIN_TYPES_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <sstream>
#include <list>


#include <scorep/SCOREP_MetricTypes.h>
#include <scorep/SCOREP_PublicTypes.h>

/* Counter IDs range (as provided to Score-P) */
#define SCOREP_COUNTER_ID_START_UCX   0

#define UCX_SAMPLING_METRIC_NAME "UCX@1"

/* Temporary counter name which will be renamed later if used */
#define TEMPORARY_COUNTER_NAME_TEMPLATE "temporary_counter_name_______________________________________"

/* TODO: Check if we can calculate this by the numbe of nodes */
#define UCX_NUM_TEMPORARY_COUNTERS 50

#define TEMPORARY_UCX_COUNTER_NAME_GEN(output_str, metric_name, counter_name, idx) {\
        char temp_str[10];\
        snprintf(temp_str, sizeof(temp_str), "%u", idx);\
        output_str = metric_name + "_" + TEMPORARY_COUNTER_NAME_TEMPLATE + temp_str;\
    }

/* The maximum size of the prefix of a counter name */
#define UCX_SAMPLING_COUNTER_NAME_PREFIX_MAX_SIZE 512

/* *Workaround*: The name of the Score-P framework rename metric function */
#define SCOREP_STRICTLY_SYNCHRONOUS_METRIC_NAME_UPDATE_FUNC_NAME "SCOREP_Strictly_Synchronous_metric_name_update"
#define SCOREP_METRIC_NAME_UPDATE_FUNC_NAME "SCOREP_metric_name_update"

typedef void
( *SCOREP_metric_name_update_t)(const char *current_name,
        const char *new_name, const char *metric_name, size_t num_metrics_set);

typedef struct tag_scorep_counter_data {
    /* name of the counter */
    char name[UCX_SAMPLING_COUNTER_NAME_PREFIX_MAX_SIZE];

    /* Counter ID */
    uint32_t scorep_counter_id;

    /* Score-P metric handle */
    SCOREP_MetricHandle metric_handle;

    /* Score-P metric already renamed */
    uint32_t scorep_metric_already_renamed;

    /* Recorded metric value */
    uint64_t value;

    /* Recorded metric previous value */
    uint64_t prev_value;

} scorep_counter_data_t;

typedef std::vector<scorep_counter_data_t *> scorep_counters_list_t;

#endif /* _PLUGIN_TYPES_H */
