/**
* Copyright (C) Huawei Technologies Co., Ltd. 2020.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#include <scorep_plugin_ucx.h>
#include <iostream>
#include <sstream>
#include <utils.h>
#include <stdlib.h>


/* Global argc and argv*/
static int global_argc = 0;
static char **global_argv = NULL;

/*
   putting the constructor in the .init_array section.
   Functions in the .init_array (unlike .init) are called with the same arguments
   main will be called with: argc, argv and env.
*/
static int
printargs(int argc, char** argv, char** env)
{
    DEBUG_PRINT("In printargs:\n");
    global_argc = argc;
    global_argv = argv;

   return 0;
}

/* Put the function into the init_array */
__attribute__((section(".init_array"))) static void *ctr = (void *)&printargs;


scorep_plugin_ucx::scorep_plugin_ucx()
{
    DEBUG_PRINT("Loading Metric Plugin: UCX Sampling\n");

    m_mpi_t_initialized = 0;
}

scorep_plugin_ucx::~scorep_plugin_ucx()
{
#if defined(SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE)
    uint64_t i;

    for (i = 0; i < m_ucx_counters_list.size(); i++) {
        delete m_ucx_counters_list[i];
    }

    /* Deallocate metrics names */
    free(m_metric_names);
#endif
}

void
scorep_plugin_ucx::metrics_names_serialize(void **names, size_t *size)
{
    size_t n_counters = m_ucx_counters_list.size();
    size_t total_size;
    size_t offset;
    uint32_t i;
    char *serialized = NULL;

    *names = NULL;
    *size = 0;

    total_size = 0;
    for (i = 0; i < n_counters; i++) {
        total_size += (::strlen(m_ucx_counters_list[i]->name) + 1);
    }

    serialized = (char *)malloc(total_size);
    if (serialized == NULL) {
        printf("metrics_names_serialize(): Error! Could not allocate serialized,"
                "total_size = %zu\n", total_size);
        exit(-1);
    }

    offset = 0;
    for (i = 0; i < n_counters; i++) {
        ::strcpy(&serialized[offset], m_ucx_counters_list[i]->name);
        DEBUG_PRINT("&serialized[%zu] = %s\n", offset, &serialized[offset]);
        offset += (::strlen(m_ucx_counters_list[i]->name) + 1);
    }

    *size = total_size;
    *names = (void *)serialized;
}

std::vector<MetricProperty>
scorep_plugin_ucx::get_metric_properties(const std::string& metric_name)
{
    unsigned long long int hex_dummy;
    int assigned_event = 0;
    std::vector<MetricProperty> metric_properties;
    uint32_t i;
    int ret;
    int is_initialized;
    uint64_t prev_val;
    int flag;
    int is_value_updated;
    uint32_t index = 0;
    int j;
    int world_size;
    uint64_t data[2];
    size_t count = 1;
    int root = 0;
    size_t metric_names_size;
    uint32_t offset;
    uint64_t value;
    const ucs_stats_aggrgt_counter_name_t *counter_names;
    size_t size;

    DEBUG_PRINT("scorep_plugin_ucx::get_metric_properties() called with: %s\n",
            metric_name);

    auto [event, dummy] = parse_metric(metric_name, &hex_dummy);

    DEBUG_PRINT("Event=%s dummy=%u, hex_dummy=%lx\n", event, dummy, hex_dummy);

    if (event == "UCX") {
        m_ucx_metric_name = metric_name;
        m_n_ucx_counters = UCX_NUM_TEMPORARY_COUNTERS;

        /* Don't re-initialize MPI if already iniitalized */
        ret = PMPI_Initialized(&is_initialized);
        DEBUG_PRINT("MPI_Init(): Checking if initialized: %d\n", is_initialized);
        if (!is_initialized) {
           DEBUG_PRINT("get_metric_properties: global_argc=%d, global_argv=%p",
               global_argc, global_argv);
           PMPI_Init(&global_argc, &global_argv);
        }

        m_mpi_t_initialized = 1;
        /* get global rank */
        PMPI_Comm_rank(MPI_COMM_WORLD, &m_mpi_rank);

        /*
           Need MPI_T initialization here since the UDP port number of the
           UCX statistics server is derived from the process_id.
           Also, all statistics are aggregated by MPI_rank 0 (root).
        */
        DEBUG_PRINT("m_mpi_rank = %d\n", m_mpi_rank);

        /* Legacy mode? ==> Use all counters */
#if defined(SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE)
        if (m_mpi_rank == 0) {
            /* Start UCX statistics server */
            ret = m_ucx_sampling.ucx_statistics_server_start(UCS_STATS_DEFAULT_UDP_PORT);

            /* Get UCX statistics */
            ret = m_ucx_sampling.ucx_statistics_current_value_get(m_mpi_rank, index,
                      &m_ucx_counters_list, &value, &prev_val);
            PMPI_Barrier(MPI_COMM_WORLD);
            // If we are the root process, send our data to everyone
            PMPI_Comm_size(MPI_COMM_WORLD, &world_size);

            /* Serialize metric names */
            metrics_names_serialize((void **)&m_metric_names, &metric_names_size);

            /* 1. Broadcast size */
            data[0] = m_ucx_counters_list.size();
            MPI_Bcast(&data[0], 1, MPI_LONG_LONG, root, MPI_COMM_WORLD);

            data[1] = (uint64_t)metric_names_size;
            MPI_Bcast(&data[1], 1, MPI_LONG_LONG, root, MPI_COMM_WORLD);

            /* 2. Broadcast names */
            MPI_Bcast((void *)m_metric_names, metric_names_size, MPI_CHAR, root, MPI_COMM_WORLD);
        }
        else {
            PMPI_Barrier(MPI_COMM_WORLD);
            // If we are a receiver process, receive the data from the root
            MPI_Bcast(&data[0], 1, MPI_LONG_LONG, root, MPI_COMM_WORLD);

            MPI_Bcast(&data[1], 1, MPI_LONG_LONG, root, MPI_COMM_WORLD);
            metric_names_size = (size_t)data[1];

            m_metric_names = (char *)malloc(metric_names_size);
            if (m_metric_names == NULL) {
                printf("UCX get_metric_properties(): Error! could not allocate metric_names,"
                    " size=%zu\n", metric_names_size);
            }

            /* 2. Receive metrics names */
            MPI_Bcast((void *)m_metric_names, metric_names_size, MPI_CHAR, root, MPI_COMM_WORLD);
        }
        DEBUG_PRINT("ucx_statistics_current_value_get() - Done\n");

        /* Allow user to set the number of UCX counters */
        m_n_ucx_counters = (size_t)data[0];
        if (dummy != 1) {
            m_n_ucx_counters = std::min((size_t)m_n_ucx_counters, (size_t)dummy);
        }

        DEBUG_PRINT("Number of UCX counters detected: %u, metric_names_size=%zu\n",
                m_n_ucx_counters, metric_names_size);

        /*
          Create counters as place holders, we're not sure how many we
          will need.
        */
        offset = 0;
        for (i = 0; i < m_n_ucx_counters; i++) {
            char *name;
            std::string temp_counter_name;

            if (m_mpi_rank == 0) {
                name = m_ucx_counters_list[i]->name;
            }
            else {
                name = &m_metric_names[offset];
                offset += (::strlen(&m_metric_names[offset]) + 1);
            }

            DEBUG_PRINT("[%d] Adding metric name: %s\n", m_mpi_rank, temp_counter_name.c_str());

            temp_counter_name = metric_name + "_" + name;

            metric_properties.insert(metric_properties.end(),
               MetricProperty(temp_counter_name.c_str(), "", "").absolute_point().value_uint().decimal());
        }
    }
#else
        /* ===> New mode: Use the UCX aggregate-sum API to reduce the amount of collected information */
        /* For now, we need to enable the server to enable UCX counters collection */
        if (m_mpi_rank == 0) {
            /* Start UCX statistics server */
            ret = m_ucx_sampling.ucx_statistics_server_start(UCS_STATS_DEFAULT_UDP_PORT);
        }

        index = 0;
        ret = m_ucx_sampling.ucx_statistics_aggregate_counter_get(index, &value);
        if (!ret) {
            printf("Warning! ucx_statistics_aggregate_counter_get() failed, ret=%d\n", ret);
        }

        ret = m_ucx_sampling.ucx_statistics_aggregate_counter_names_get(&counter_names, &size);
        if (!ret) {
            printf("Warning! ucx_statistics_aggregate_counter_get() failed, ret=%d\n", ret);
        }

        /* Update Score-P with metrics names */
        for (i = 0; i < size; i++) {
            std::string temp_counter_name;

            DEBUG_PRINT("[%d] Adding metric name: %s\n", m_mpi_rank, temp_counter_name.c_str());

            temp_counter_name = metric_name + "_" + counter_names[i].class_name + "_" + counter_names[i].counter_name;

            metric_properties.insert(metric_properties.end(),
               MetricProperty(temp_counter_name.c_str(), "", "").absolute_point().value_uint().decimal());
        }

    }
#endif /* SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE */
    else if ((event == SCOREP_STRICTLY_SYNCHRONOUS_METRIC_NAME_UPDATE_FUNC_NAME) ||
             (event == SCOREP_METRIC_NAME_UPDATE_FUNC_NAME)) {
        assigned_event = 1;
        m_pSCOREP_metric_name_update_func =
            (SCOREP_metric_name_update_t)hex_dummy;
    }

    /* Debug print */
    if (assigned_event) {
        DEBUG_PRINT("%s = %lx\n", event, (uintptr_t)hex_dummy);
    }

    return metric_properties;
}


int32_t
scorep_plugin_ucx::add_metric(const std::string& metric)
{
    int32_t id = 0;
    unsigned long long int hex_dummy;

    auto [event, period] = parse_metric(metric, &hex_dummy);

    DEBUG_PRINT("add_metric() called with: %s\n", metric);

    /* UCX? */
    if (event == "UCX") {
        static uint32_t current_id = 0;

        id = current_id;
        current_id++;
    }

    return id;
}


void
scorep_plugin_ucx::start()
{
    DEBUG_PRINT("scorep_plugin_ucx::start()\n");
}


void
scorep_plugin_ucx::stop()
{
    DEBUG_PRINT("scorep_plugin_ucx::stop()\n");
}


int
scorep_plugin_ucx::scorep_metric_rename(uint32_t counter_id, const char *counter_new_name,
        size_t num_metrics_set)
{
    std::string temp_counter_name;

    TEMPORARY_UCX_COUNTER_NAME_GEN(temp_counter_name,
        m_ucx_metric_name, temp_counter_name, counter_id);

    m_pSCOREP_metric_name_update_func(temp_counter_name.c_str(),
        counter_new_name, m_ucx_metric_name.c_str(), num_metrics_set);

    return 0;
}


void
scorep_plugin_ucx::ucx_counters_scorep_update(void)
{
    uint32_t i;

    DEBUG_PRINT("ucx_counters_scorep_update() called\n");

    /* Check if Score-P function pointers are initalized */
    if (!m_pSCOREP_metric_name_update_func) {
        printf("ucx_counters_scorep_update() error! SCOREP_metric_name_update_func is NULL,\n"
                "was not provided by the Score-P framework.\n"
                "Please use the Huawei Score-P 6.0 (branch: huawei-ucx-workaround) at this point\n");
        return;
    }

    for (i = 0; i < m_ucx_counters_list.size(); i++) {
         if (m_ucx_counters_list[i]->scorep_metric_already_renamed == 0) {
             size_t num_metrics_set = m_ucx_counters_list.size();
             DEBUG_PRINT("Renaming counter: %p\n", m_ucx_counters_list[i]->name);
             scorep_metric_rename(i, m_ucx_counters_list[i]->name, 0);
             m_ucx_counters_list[i]->scorep_metric_already_renamed = 1;
         }
    }
}


SCOREP_METRIC_PLUGIN_CLASS(scorep_plugin_ucx, "scorep_plugin_ucx")
