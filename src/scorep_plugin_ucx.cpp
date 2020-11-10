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

scorep_plugin_ucx::scorep_plugin_ucx()
{
    DEBUG_PRINT("Loading Metric Plugin: UCX Sampling\n");

    m_mpi_t_initialized = 0;
}

scorep_plugin_ucx::~scorep_plugin_ucx()
{
    uint64_t i;

    for (i = 0; i < m_ucx_counters_list.size(); i++) {
        delete m_ucx_counters_list[i];
    }
}


std::vector<MetricProperty>
scorep_plugin_ucx::get_metric_properties(const std::string& metric_name)
{
    unsigned long long int hex_dummy;
    int assigned_event = 0;
    std::vector<MetricProperty> metric_properties;

    DEBUG_PRINT("scorep_plugin_ucx::get_metric_properties() called with: %s\n",
            metric_name);

    auto [event, dummy] = parse_metric(metric_name, &hex_dummy);

    DEBUG_PRINT("Event=%s dummy=%u, hex_dummy=%lx\n", event, dummy, hex_dummy);

    if (event == "UCX") {
        uint32_t i;
        m_ucx_metric_name = metric_name;
        m_n_ucx_counters = UCX_NUM_TEMPORARY_COUNTERS;

        /* Allow user to set the number of UCX counters */
        if (dummy != 1) {
            m_n_ucx_counters = dummy;
        }

        DEBUG_PRINT("m_n_ucx_counters = %u\n", m_n_ucx_counters);

        /*
          Create counters as place holders, we're not sure how many we
          will need.
        */
        for (i = 0; i < m_n_ucx_counters; i++) {
            std::string temp_counter_name;

            TEMPORARY_UCX_COUNTER_NAME_GEN(temp_counter_name,
               metric_name, temp_counter_name, i);

            /* Push some temp counters as placeholders for events */
            metric_properties.insert(metric_properties.begin(),
               MetricProperty(temp_counter_name.c_str(), "", "").absolute_point().value_uint().decimal());
        }
    }
    else if ((event == SCOREP_STRICTLY_SYNCHRONOUS_METRIC_NAME_UPDATE_FUNC_NAME) ||
             (event == SCOREP_METRIC_NAME_UPDATE_FUNC_NAME)){
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
        static uint32_t current_id = (m_n_ucx_counters - 1);

        /*
           As part of the UCX workarond:
           Only temporary counters are registered at this point.
        */
        id = current_id;
        current_id--;
        if (id < 0) {
            printf("Warning, UCX counter id < SCOREP_COUNTER_ID_START_UCX: %d\n", id);
        }
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
