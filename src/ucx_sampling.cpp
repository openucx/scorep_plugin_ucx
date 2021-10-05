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
#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <math.h>

#include <getopt.h>

#include <utils.h>

#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)
#include <ucs/counters/ethtool_ctrs.h>
#endif

#include "ucx_sampling.h"

#define THRESHOLD_FOR_STARTING_UCX_MEASUREMENTS (UCX_NUM_TEMPORARY_COUNTERS*25)

/* Enable verbose mode */
//#define UCX_SAMPLING_VERBOSE_MODE_ENABLE

/* Constructor */
ucx_sampling::ucx_sampling()
{
    m_counters_initialized_on_scorep = 0;
    m_statistics_server_process_enable = 0;

    /* Initialize  */
    m_aggrgt_sum_size = 0;
    memset(m_aggrgt_sum_counters, 0x00, sizeof(m_aggrgt_sum_counters));

    m_aggrgt_sum_counter_names = NULL;
    m_aggrgt_sum_counter_names_size = 0;

    /* Set NIC counters to not initialized */
    m_nic_counters_initialized = 0;

#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)
    /* allocate memory for stats_handle of the device  */
    m_nic_ndev_name = getenv(ENV_SCOREP_UCX_PLUGIN_NIC_DEVICE_NAME);
    if (m_nic_ndev_name != NULL) {
        ucs_status_t status = stats_alloc_handle(m_nic_ndev_name, &m_eth_stats_handle);
        if (status != UCS_OK) {
            printf("Warning: stats_alloc_handle() failed! status=%d\n", status);
        }
        else {
            m_nic_counters_initialized = 1;
        }
    }

    DEBUG_PRINT("m_nic_ndev_name = %s\n", m_nic_ndev_name);

    /* Initiaize NIC counters database */
    m_nic_cnts_agrgt_num = 0;
    m_num_nic_counters_total = 0;
    memset(m_nic_cnts_agrgt_mapping, 0x00, sizeof(m_nic_cnts_agrgt_mapping));
    memset(m_nic_cnts_agrgt, 0x00, sizeof(m_nic_cnts_agrgt));
    m_nic_rounds_cnt = 0;
#endif
}

/* Destructor */
ucx_sampling::~ucx_sampling()
{
#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)
    /* release the memory of stats_handle of the device  */
    if ((m_nic_counters_initialized) && (m_nic_ndev_name != NULL)) {
        ucs_status_t status = stats_release_handle(&m_eth_stats_handle);
        if (status != UCS_OK) {
            printf("Warning: stats_release_handle() failed! status=%d\n", status);
        }
    }
#endif
}

void
ucx_sampling::configuration_set(int ucx_counters_enable, int nic_counters_enable)
{
    m_ucx_counters_collect_enable = ucx_counters_enable;
    m_nic_counters_collect_enable = nic_counters_enable;
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

void
ucx_sampling::ucx_statistics_aggregate_counter_size_assign(size_t size)
{
    m_aggrgt_sum_counter_names_size = size;
}

/* NIC counters implementation */
#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)

void
ucx_sampling::nic_counters_update(size_t *num_counters)
{
    *num_counters = 0;

    if (m_nic_counters_initialized) {
        /* query device counters and store them in stats_handle  */
        void *filter = NULL;
        ucs_status_t status = stats_query_device(&m_eth_stats_handle, filter);
        if (status != UCS_OK) {
            printf("Warning: stats_query_device() failed! status=%d\n", status);
        }

        *num_counters = m_eth_stats_handle.super.n_stats;
    }
}

uint64_t
ucx_sampling::nic_counter_value_get(uint32_t index)
{
    uint64_t value = 0;
    size_t num_counters;

    if (m_nic_counters_initialized) {
        /* Update NIC counters? */
        if (index == 0) {
            if ((m_nic_rounds_cnt & (NIC_COUNTERS_UPDATE_DECIMATION-1)) == 0) {
                nic_counters_update(&num_counters);
            }
            m_nic_rounds_cnt++;
        }

        value = m_eth_stats_handle.super.stats->data[index];
    }

    return value;
}

void
ucx_sampling::nic_counter_name_get(uint32_t index, string *name)
{
    if (m_nic_counters_initialized) {
        *name =
            (const char *)&m_eth_stats_handle.super.strings->data[index * ETH_GSTRING_LEN];
    }
}

void
ucx_sampling::nic_counter_name_filter(string *name)
{
    std::string filter_chars = "0123456789:";

    name->erase(
              remove_if(name->begin(), name->end(),
                        [&filter_chars](const char &c) {
                            return filter_chars.find(c) != std::string::npos;
                        }),
                        name->end());
}

int
ucx_sampling::aggt_sum_counter_name_index_find(string *cnt_name, string *aggt_cnts_names, uint32_t aggt_cnt_num,
                   uint32_t *agrgt_counter_index)
{
    uint32_t i;

    for (i = 0; i < aggt_cnt_num; i++) {
        if (*cnt_name == aggt_cnts_names[i]) {
            *agrgt_counter_index = i;
            return 1;
        }
    }

    /* Not found */
    *agrgt_counter_index = -1;

    return 0;
}

/*
   Updates and aggregate sums counters.

   returns: The number of counters in the aggregate list.
*/
uint32_t
ucx_sampling::nic_counters_aggregate()
{
    string cnt_name;
    uint32_t index = 0;
    uint32_t filtered_index = 0;
    uint32_t agrgt_counter_index;
    int ret;

    /* Update NIC counters */
    nic_counters_update(&m_num_nic_counters_total);

    while (index < m_num_nic_counters_total) {

        /* Get the next counter name */
        nic_counter_name_get(index, &cnt_name);

        /* Filter out characters (to get aggregate_sum name) */
        nic_counter_name_filter(&cnt_name);

        /* Is counter already in list? */
        ret = aggt_sum_counter_name_index_find(&cnt_name, m_nic_cnts_agrgt_names,
                  filtered_index, &agrgt_counter_index);
        if ( (ret == 0) && (filtered_index < NUM_NIC_AGGREGATE_CNTS_MAX) ) {
            DEBUG_PRINT("Adding new counter: cnt_name = %s\n", cnt_name.c_str());

            agrgt_counter_index = filtered_index;
            m_nic_cnts_agrgt_names[agrgt_counter_index] = cnt_name;
            filtered_index++;
        }

        /* Add index of counter to database */
        m_nic_cnts_agrgt_mapping[index] = agrgt_counter_index;

        index++;
    }

    m_nic_cnts_agrgt_num = filtered_index;

    return m_nic_cnts_agrgt_num;
}

#endif /* UCX_STATS_NIC_COUNTERS_ENABLE */


