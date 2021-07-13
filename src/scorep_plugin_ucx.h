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

/*
   Enable plugin in UCX Statistics legacy mode
   (i.e. don't use aggregate-sum)
*/
//#define SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE

#ifdef __cplusplus
extern "C" {
#endif
#include <mpi.h>
#ifdef __cplusplus
}
#endif

#include <ucx_sampling.h>
#include <plugin_types.h>

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

        void
        metrics_names_serialize(void **names, size_t *size);

        void
        ucx_counters_scorep_update(void);

        /* Renames a Score-P metric (for dynamic allocation of metrics) */
        int
        scorep_metric_rename(uint32_t counter_id, const char *counter_new_name, size_t num_metrics_set);
};


inline int
scorep_plugin_ucx::current_value_get(int32_t id, uint64_t *value, uint64_t *prev_value)
{
    uint64_t prev_val;
    int ret;
    int flag;
    int is_value_updated;

    *value = 0;
    *prev_value = 0;

#if defined(SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE)
    /* UCX counter */
    /* Enumerate PVARs */
    if (0 == m_mpi_t_initialized) {

       ret = MPI_Initialized(&flag);
       if (flag) {
         m_mpi_t_initialized = 1;
         /* get global rank */
         PMPI_Comm_rank(MPI_COMM_WORLD, &m_mpi_rank);
         //std::cout << "MPI_rank = " << m_mpi_rank << std::endl;
       }
       is_value_updated = 1;
    }
    else {
        /*
           Need MPI_T initialization here since the UDP port number of the
           UCX statistics server is derived from the process_id.
           Also, all statistics are aggregated by MPI_rank 0 (root).
        */
        if (m_mpi_rank == 0) {
            /* Get UCX statistics */
            ret = m_ucx_sampling.ucx_statistics_current_value_get(m_mpi_rank, id,
                      &m_ucx_counters_list, value, &prev_val);
#if 0
            /* Rename UCX counters in Score-P log? only if ret != 0 */
            if (ret) {
               ucx_counters_scorep_update();
            }
#endif

            if (*value != prev_val) {
                is_value_updated = 1;
            }
            else {
                is_value_updated = 0;
            }
        }
    }
#else
    /* Update counter via Aggregate-sum API */
    ret = m_ucx_sampling.ucx_statistics_aggregate_counter_get(id, value);
    is_value_updated = 1;
#endif //SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE

    return is_value_updated;
}

template <typename Proxy>
void
scorep_plugin_ucx::get_current_value(int32_t id, Proxy& proxy)
{
    int is_value_updated;
    uint64_t value;
    uint64_t prev_value;

    is_value_updated = current_value_get(id, &value, &prev_value);

    proxy.write(value);
}


template <typename Proxy>
void
scorep_plugin_ucx::get_optional_value(int32_t id, Proxy& proxy)
{
    int is_value_updated;
    uint64_t value;
    uint64_t prev_value;

    is_value_updated = current_value_get(id, &value, &prev_value);
    proxy.write(value);
}

#endif /* _SCOREP_PLUGIN_UCX_H_ */
