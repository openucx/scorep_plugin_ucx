/**
* Copyright (C) Huawei Technologies Co., Ltd. 2020.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#if !defined(_UCX_SAMPLING_H_)
#define _UCX_SAMPLING_H_

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <ucs/stats/libstats.h>
#include <ucs/stats/stats.h>
#ifdef __cplusplus
}
#endif

#include <plugin_types.h>

#include <scorep_plugin_ucx_config.h>

#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)
#include <ucs/counters/ethtool_ctrs.h>
#endif

using namespace std;

#define THRESHOLD 						0
#define NOT_FOUND 						-1
#define FALSE 							0
#define TRUE 							1
#define STR_SZ 							256
#define NUM_PERF_VAR_SUPPORTED			50

#define NEG_INF							-10000000
#define POS_INF							10000000

/* Maximum number of aggregate-sum counters */
#define UCX_AGGREGATE_SUM_NUM_COUNTERS_MAX 64

/* Number of aggregate sum counters */
#define NUM_NIC_AGGREGATE_CNTS_MAX         1024

/* Total number of NIC counters */
#define NUM_NIC_CNTS_MAX                   (10*1024)

/*********************************/
/* Main class for ucx Sampling */
/*********************************/
class ucx_sampling {
public:
   /* Constructor */
   ucx_sampling();

   /* Destructor */
   ~ucx_sampling();

   /* Set data acquisition configuration */
   void
   configuration_set(int ucx_counters_enable, int nic_counters_enable);

   /* Read current value of a PVAR */
   int
   ucx_statistics_current_value_get(int mpi_rank, uint32_t index,
       scorep_counters_list_t *ucx_counters_list, uint64_t *value, uint64_t *prev_value);

   int
   ucx_statistics_server_start(int port);

   /* Update & initialize counters from UCX statistics */
   void
   ucx_statistics_all_counters_update(scorep_counters_list_t *new_counters_list,
       int initialize_counters_enable);


   /* Get current aggregate-sum value of a certain index */
   int
   ucx_statistics_aggregate_counter_get(uint32_t index, uint64_t *value);

   /* Get current aggregate-sum counter names of all indices */
   int
   ucx_statistics_aggregate_counter_names_get(const ucs_stats_aggrgt_counter_name_t **names_p,
       size_t *size_p);

   size_t
   ucx_statistics_aggrgt_sum_total_counters_num_get() {
       return m_aggrgt_sum_size;
   }

#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)

   /* Read NIC counters into stats handle ==> Update statistics */
   void
   nic_counters_update(size_t *num_counters);

   /* Get NIC counter value - Returns the value of the counter */
   uint64_t
   nic_counter_value_get(uint32_t index);

   /* Get NIC counter name - Returns the counter name */
   void
   nic_counter_name_get(uint32_t index, string *name);

   void
   nic_counter_name_filter(string *name);

   /* Get the total number of aggregate-sum NIC counters */
   size_t
   nic_counter_total_aggrgt_num_counters() {
       return m_nic_cnts_agrgt_num;
   }

   int
   aggt_sum_counter_name_index_find(string *cnt_name, string *aggt_cnts_names, uint32_t aggt_cnt_num,
        uint32_t *agrgt_counter_index);

   /*
      Updates and aggregate sums counters (required to be executed only once).

      returns: The number of counters in the aggregate list.
   */
   uint32_t
   nic_counters_aggregate();

#endif /* UCX_STATS_NIC_COUNTERS_ENABLE */

private:

   uint64_t
   recursive_scan_counters_list(ucs_stats_node_t *root,
       ucs_list_link_t *stats,
       const char *counters_root_name,
       scorep_counters_list_t *ucx_counters_list,
       uint64_t num_objects,
       uint64_t& num_counters,
       int initialize_counters_enable);

   /* Scan the counters list to see if there are any new counters */
   void
   scan_counters_list(ucs_list_link_t *stats,
       const char *counters_root_name,
       scorep_counters_list_t *ucx_counters_list,
       int initialize_counters_enable);

private:

   /* Handle for UCX statistics server */
   ucs_stats_server_h m_ucx_stats_server;

   /*
      Enable statistics server on process,
      if port bound successfully
   */
   int m_statistics_server_process_enable;

   /* Score-P counters initialized (updated dynamically) */
   int m_counters_initialized_on_scorep;

   /* size of aggregate-sum counters list */
   size_t m_aggrgt_sum_size;

   /* Aggregate-sum counters list */
   ucs_stats_counter_t m_aggrgt_sum_counters[UCX_AGGREGATE_SUM_NUM_COUNTERS_MAX];

   /* The counters names */
   const ucs_stats_aggrgt_counter_name_t *m_aggrgt_sum_counter_names;

   /* The size of the counters names */
   size_t m_aggrgt_sum_counter_names_size;

   /* Enable functionality (UCX / NIC counters) */
   int m_ucx_counters_collect_enable;
   int m_nic_counters_collect_enable;

#if defined(UCX_STATS_NIC_COUNTERS_ENABLE)
   /* NIC counters handle */
   ethtool_stats_handle_t m_eth_stats_handle;

   /* NIC device name */
   const char *m_nic_ndev_name;

   /* NIC counters database: Number of aggregate-sum counters */
   uint32_t m_nic_cnts_agrgt_num;

   /* NIC counters index mapping: counter_index->aggregate_sum_index */
   uint32_t m_nic_cnts_agrgt_mapping[NUM_NIC_CNTS_MAX];

   /* NIC counters aggregate counters value */
   uint64_t m_nic_cnts_agrgt[NUM_NIC_AGGREGATE_CNTS_MAX];

   /* NIC counters aggregate counters names */
   string m_nic_cnts_agrgt_names[NUM_NIC_AGGREGATE_CNTS_MAX];

   /* Total number of NIC counters */
   uint64_t m_num_nic_counters_total;

   /* Decimation count (number of rounds) */
   uint32_t m_nic_rounds_cnt;
#endif

   /* NIC counters initialized status */
   int m_nic_counters_initialized;
};


#endif /* _UCX_SAMPLING_H_ */
