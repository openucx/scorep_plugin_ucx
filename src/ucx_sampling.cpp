/**
* Copyright (C) Huawei Technologies Co., Ltd. 2020.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */

#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdlib.h>
#include <math.h>

#include <getopt.h>

#include <utils.h>

#include "ucx_sampling.h"

#define THRESHOLD_FOR_STARTING_UCX_MEASUREMENTS (UCX_NUM_TEMPORARY_COUNTERS*25)

/* Enable verbose mode */
//#define UCX_SAMPLING_VERBOSE_MODE_ENABLE

/* Constructor */
ucx_sampling::ucx_sampling()
{
    m_counters_initialized_on_scorep = 0;
    m_statistics_server_process_enable = 0;
}

/* Destructor */
ucx_sampling::~ucx_sampling()
{
}

int ucx_sampling::ucx_statistics_server_start(int port)
{
    ucs_stats_node_t *root;
    ucs_list_link_t *stats;
    ucs_status_t status;
    int ret = 0;

    DEBUG_PRINT("ucx_statistics_server_start()\n");
    DEBUG_PRINT("UCX Port used = %d\n", port);

    m_statistics_server_process_enable = 0;
    status = ucs_stats_server_start(port, &m_ucx_stats_server);
    if (status != UCS_OK) {
        printf("ucs_stats_server_start() Failed! status=%u\n", status);
        ret = -1;
        goto exit_and_return_metric_properties;
    }

    DEBUG_PRINT("ucs_stats_server_start() Succes! status=%u\n", status);
    m_statistics_server_process_enable = 1;

exit_and_return_metric_properties:
    return ret;
}

uint64_t
ucx_sampling::recursive_scan_counters_list(ucs_stats_node_t *root,
        ucs_list_link_t *stats,
        const char *counters_root_name,
        scorep_counters_list_t *ucx_counters_list,
        uint64_t num_objects,
        uint64_t& num_counters,
        int initialize_counters_enable)
{
    ucs_status_t status;
    ucs_stats_node_t *data_node;
    size_t statistics_len;
    char counter_name_prefix[UCX_SAMPLING_COUNTER_NAME_PREFIX_MAX_SIZE];
    char counter_name_temp[UCX_SAMPLING_COUNTER_NAME_PREFIX_MAX_SIZE];
    uint64_t n_local_objects = 0;
    scorep_counter_data_t *ucx_performance_counter = NULL;

    ucs_list_for_each(data_node, &root->children[UCS_STATS_ACTIVE_CHILDREN], list) {
#if defined(UCX_SAMPLING_VERBOSE_MODE_ENABLE)
        std::cout << "name = " << std::string(data_node->cls->name) <<
                     " num_counters=" << data_node->cls->num_counters <<
                     std::endl;
#endif

        if (initialize_counters_enable) {
            snprintf(counter_name_prefix, sizeof(counter_name_prefix),
                     "%s-%s", counters_root_name, data_node->cls->name);
            snprintf(counter_name_temp, sizeof(counter_name_temp),
                     "ucx-object-%lu-%s", num_objects, counter_name_prefix);
        }

        for (int k = 0; k < data_node->cls->num_counters; k++) {
            if (initialize_counters_enable) {
                ucx_performance_counter = new scorep_counter_data_t();
                ucx_performance_counter->scorep_counter_id = num_counters;
                ucx_performance_counter->metric_handle = 0;
                ucx_performance_counter->scorep_metric_already_renamed = 0;
                snprintf(ucx_performance_counter->name,
                         sizeof(ucx_performance_counter->name),
                         "%s-%s",
                         counter_name_temp,
                         data_node->cls->counter_names[k]);
                ucx_performance_counter->value = data_node->counters[k];
                ucx_performance_counter->prev_value = (uint64_t)-1;

                ucx_counters_list->push_back(ucx_performance_counter);
            }
            else {
                if (num_counters >= ucx_counters_list->size()) {
                    /* We are not updating the counters list in runtime */
                    goto ucx_scan_counters_complete_exit;
                }
                (*ucx_counters_list)[num_counters]->value = data_node->counters[k];
            }

#if defined(UCX_SAMPLING_VERBOSE_MODE_ENABLE)
            std::cout << ucx_performance_counter->name <<
                         " " << data_node->counters[k] << std::endl;
#endif

            num_counters++;
        }

        /* Number of UCX objects counted */
        num_objects++;
        n_local_objects++;

        /* Recursively iterate through counters */
        num_objects += recursive_scan_counters_list(data_node,
              stats, counter_name_prefix, ucx_counters_list,
              num_objects, num_counters, initialize_counters_enable);
    }

ucx_scan_counters_complete_exit:
    return n_local_objects;
}

void
ucx_sampling::scan_counters_list(ucs_list_link_t *stats,
     const char *counters_root_name,
     scorep_counters_list_t *ucx_counters_list,
     int initialize_counters_enable)
{
    ucs_status_t status;
    ucs_stats_node_t *data_node;
    ucs_stats_node_t *root;
    size_t statistics_len;
    uint64_t num_objects;
    uint64_t num_counters;

    statistics_len = ucs_list_length(stats);
#if defined(UCX_SAMPLING_VERBOSE_MODE_ENABLE)
    std::cout<< "stats len: " << statistics_len << std::endl;
#endif
    /* Recursively iterate through counters */
    root = ucs_list_head(stats, ucs_stats_node_t, list);
    num_objects = 0;
    num_counters = 0;
    recursive_scan_counters_list(root, stats, "cnt", ucx_counters_list,
            num_objects, num_counters, initialize_counters_enable);
}


void
ucx_sampling::ucx_statistics_all_counters_update(
    scorep_counters_list_t *new_counters_list,
    int initialize_counters_enable)
{
    ucs_list_link_t *stats;

    /* Trigger dumping UCX statistics */
    ucs_stats_dump();

    /* Wait for statistics to be sent to Server by UCX */
    do {
    } while (ucs_stats_server_rcvd_packets(m_ucx_stats_server) == 0);

#if defined(UCX_SAMPLING_VERBOSE_MODE_ENABLE)
    std::cout << "UCS statistics server started, getting statistics...";
#endif
    /* Get stats */
    stats = ucs_stats_server_get_stats(m_ucx_stats_server);

    /* Scan counters list */
    scan_counters_list(stats, "", new_counters_list,
       initialize_counters_enable);

#if defined(UCX_SAMPLING_VERBOSE_MODE_ENABLE)
    std::cout << "Done: statistics ptr returned=" << stats << "\n";
#endif
    /* Release stats */
    ucs_stats_server_purge_stats(m_ucx_stats_server);
}

int
ucx_sampling::ucx_statistics_current_value_get(int mpi_rank, uint32_t index,
        scorep_counters_list_t *ucx_counters_list,
        uint64_t *value, uint64_t *prev_value)
{
    ucs_status_t status;
    ucs_stats_node_t *data_node;
    ucs_stats_node_t *root;
    size_t rx_packets;
    size_t statistics_len;
    int initialize_counters_enable = 0;
    static uint64_t counter = 0;

    /* UCX Statistics server already up and running and counters were updated */
    if (m_counters_initialized_on_scorep) {
        if (index == 0) {
            ucx_statistics_all_counters_update(ucx_counters_list,
                initialize_counters_enable);
        }

        if (index < ucx_counters_list->size()) {
            *value = (*ucx_counters_list)[index]->value;
            *prev_value = (*ucx_counters_list)[index]->prev_value;
            (*ucx_counters_list)[index]->prev_value = *value;
        }
        else {
            *value = 0;
            *prev_value = 0;
        }

        return 0;
    }

    if (ucx_counters_list->size() == 0) {
        if ((counter & 0xFF) == 0) {
           /* Initialize counters list */
           initialize_counters_enable = 1;
           ucx_statistics_all_counters_update(ucx_counters_list,
              initialize_counters_enable);

           printf("Detected UCX counters after scan, size = %u\n",
                ucx_counters_list->size());

           if (ucx_counters_list->size() != 0) {
              m_counters_initialized_on_scorep = 1;
              printf("Added UCX counters after scan, size = %u\n",
                   ucx_counters_list->size());
              /* Indicates initialized counters list */
              return 1;
           }
        }
    }
    counter++;

    return 0;
}

int
ucx_sampling::ucx_statistics_aggregate_counter_get(uint32_t index, uint64_t *value)
{
    int ret = 1;

    if (unlikely(index == 0)) {
        m_aggrgt_sum_size = ucs_stats_aggregate(m_aggrgt_sum_counters, ARRAY_SIZE(m_aggrgt_sum_counters));
        if (m_aggrgt_sum_size == 0) {
            printf("Warning: ucs_stats_aggregate() aggrgt_sum_size == 0");
            ret = 0;
            *value = 0;
        }

        *value = m_aggrgt_sum_counters[0];
    }
    else {
        *value = m_aggrgt_sum_counters[index];
    }

    return ret;
}

int
ucx_sampling::ucx_statistics_aggregate_counter_names_get(const ucs_stats_aggrgt_counter_name_t **names_p,
    size_t *size_p)
{
    /* Get counters names */
    ucs_stats_aggregate_get_counter_names(&m_aggrgt_sum_counter_names, &m_aggrgt_sum_counter_names_size);

    *names_p = m_aggrgt_sum_counter_names;
    *size_p = m_aggrgt_sum_counter_names_size;

    return (m_aggrgt_sum_counter_names_size > 0);
}

