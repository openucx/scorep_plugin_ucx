/**
* Copyright (C) Huawei Technologies Co., Ltd. 2020.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#if !defined(_SCOREP_PLUGIN_UCX_H_)
#define _SCOREP_PLUGIN_UCX_H_

#include <scorep/plugin/plugin.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <x86intrin.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <mpi.h>
#ifdef __cplusplus
}
#endif

#include <scorep_plugin_ucx_config.h>

#include <ucx_sampling.h>
#include <plugin_types.h>

#define METRIC_NAMES_FILENAME "ucx_plugin_metric_names.txt"

using namespace scorep::plugin::policy;
using ThreadId = std::thread::id;
using TimeValuePair = std::pair<scorep::chrono::ticks, double>;
using MetricProperty = scorep::plugin::metric_property;
using ThreadEventPair = std::tuple<ThreadId, std::string>;

class scorep_plugin_ucx : public scorep::plugin::base<scorep_plugin_ucx,
    sync, per_thread, scorep_clock>
{
    public:
        scorep_plugin_ucx();

        ~scorep_plugin_ucx();

        std::vector<MetricProperty>
        get_metric_properties(const std::string& metric_name);

        int32_t
        add_metric(const std::string& metric);

        void
        start();

        void
        stop();

        inline int
        current_value_get(int32_t id, uint64_t *value, uint64_t *prev_value);

        template <typename Proxy>
        void get_current_value(int32_t id, Proxy& proxy);

        template <typename Proxy>
        void get_optional_value(int32_t id, Proxy& proxy);

        /* Override, in order to set the delta_t */
        static SCOREP_Metric_Plugin_Info
        get_info()
        {
            const char *profiling_enabled_env = getenv("SCOREP_ENABLE_PROFILING");

            SCOREP_Metric_Plugin_Info info =
            scorep::plugin::base<scorep_plugin_ucx, sync, per_thread, scorep_clock>::get_info();

            /* Cannot update the delta_t when profiling */
            if (strcmp(profiling_enabled_env, "true") != 0) {
                /* Update the delta_t: Required for reduction of TRACING overhead */
                info.delta_t = 8*80000;
            }

            return info;
        }

    private:
        /* Indicates whether or not MPI is initialized */
        int m_mpi_t_initialized;

        /* Enable UCX counters collection. */
        int m_ucx_counters_collect_enable;

        /* Enable NIC counters collection. */
        int m_nic_counters_collect_enable;

        /* MPI rank of this process */
        int m_mpi_rank;

        char *m_metric_names;

        /* UCX sampling object */
        ucx_sampling m_ucx_sampling;
        std::string m_ucx_metric_name;
        size_t m_n_ucx_counters;

        /* UCX counters list + Score-P handles */
        scorep_counters_list_t m_ucx_counters_list;

        /* Pointer to the Score-P framework metric rename function */
        SCOREP_metric_name_update_t m_pSCOREP_metric_name_update_func;

#if defined(SCOREP_PLUGIN_MICROBENCHMARK_ENABLE)
        double m_ticks_cnt_get_total;
        double m_ticks_cnt_get_num_times;
#endif

        void
        metrics_names_serialize(void **names, size_t *size);

        void
        ucx_counters_scorep_update(void);

        /* Renames a Score-P metric (for dynamic allocation of metrics) */
        int
        scorep_metric_rename(uint32_t counter_id, const char *counter_new_name, size_t num_metrics_set);

        /* UCX statistics - Legacy enumerator */
        void
        ucx_statistics_enumerate_legacy(uint64_t dummy);

        void
        metric_name_add_to_file(char *metric_name);

        int
        metric_name_read_counters_list_from_file(std::vector<std::string> *counters_list);

};


inline int
scorep_plugin_ucx::current_value_get(int32_t id, uint64_t *value, uint64_t *prev_value)
{
    uint64_t prev_val;
    int ret;
    int flag;
    int is_value_updated;
    size_t num_ucx_aggrgt_sum_cnts;
    const ucs_stats_aggrgt_counter_name_t *counter_names;
    size_t size;
    uint32_t index;
    uint64_t counter_value;

    *value = 0;
    *prev_value = 0;

    if (0 == m_mpi_t_initialized) {
        is_value_updated = 1;

        ret = MPI_Initialized(&flag);
        if (flag) {
            m_mpi_t_initialized = 1;
            /* get global rank */
            PMPI_Comm_rank(MPI_COMM_WORLD, &m_mpi_rank);

            /* ===> New mode: Use the UCX aggregate-sum API to reduce the amount of collected information */
            /* For now, we need to enable the server to enable UCX counters collection */
            if (m_mpi_rank == 0) {
                /* Start UCX statistics server */
                ret = m_ucx_sampling.ucx_statistics_server_start(UCS_STATS_DEFAULT_UDP_PORT);
            }

            index = 0;
            ret = m_ucx_sampling.ucx_statistics_aggregate_counter_get(index, &counter_value);
            if (!ret) {
                printf("Warning! ucx_statistics_aggregate_counter_get() failed, ret=%d\n", ret);
            }

            /* Initialize the UCX aggregate-sum API */
            ret = m_ucx_sampling.ucx_statistics_aggregate_counter_names_get(&counter_names, &size);
            if (!ret) {
                printf("Warning! ucx_statistics_aggregate_counter_get() failed, ret=%d\n", ret);
            }
        }
        else {
            return is_value_updated;
        }
    }

    /* Update counter via Aggregate-sum API */
    num_ucx_aggrgt_sum_cnts = m_ucx_sampling.ucx_statistics_aggrgt_sum_total_counters_num_get();
    if (id < num_ucx_aggrgt_sum_cnts) {
        /* UCX SW counters */
        ret = m_ucx_sampling.ucx_statistics_aggregate_counter_get(id, value);
        is_value_updated = 1;
    }
    else { /* NIC counters */
#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)
        /*
           Get counter value (the index of the NIC counter
           is calculated by substracting the num_ucx_aggrgt_sum_cnts from the
           Score-P counter index.
        */
        *value = m_ucx_sampling.nic_counter_value_get((id - num_ucx_aggrgt_sum_cnts));
        is_value_updated = 1;
#endif
    }

    return is_value_updated;
}

template <typename Proxy>
void
scorep_plugin_ucx::get_current_value(int32_t id, Proxy& proxy)
{
    int is_value_updated;
    uint64_t value;
    uint64_t prev_value;

#if defined(SCOREP_PLUGIN_MICROBENCHMARK_ENABLE)
    uint64_t ticks_start = __rdtsc();
#endif

    is_value_updated = current_value_get(id, &value, &prev_value);

    proxy.write(value);


#if defined(SCOREP_PLUGIN_MICROBENCHMARK_ENABLE)
    /* Update micro benchmark */
    m_ticks_cnt_get_total += (double)(__rdtsc() - ticks_start);
    m_ticks_cnt_get_num_times++;
#endif
}


template <typename Proxy>
void
scorep_plugin_ucx::get_optional_value(int32_t id, Proxy& proxy)
{
    int is_value_updated;
    uint64_t value;
    uint64_t prev_value;

#if defined(SCOREP_PLUGIN_MICROBENCHMARK_ENABLE)
    uint64_t ticks_start = __rdtsc();
#endif

    is_value_updated = current_value_get(id, &value, &prev_value);
    proxy.write(value);

#if defined(SCOREP_PLUGIN_MICROBENCHMARK_ENABLE)
    /* Update micro benchmark */
    m_ticks_cnt_get_total += (double)(__rdtsc() - ticks_start);
    m_ticks_cnt_get_num_times++;
#endif
}

#endif /* _SCOREP_PLUGIN_UCX_H_ */
