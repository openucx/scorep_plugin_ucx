#if !defined(_SCOREP_PLUGIN_UCX_CONFIG_H_)
#define _SCOREP_PLUGIN_UCX_CONFIG_H_

/*
   Enable plugin in UCX Statistics legacy mode
   (i.e. don't use aggregate-sum)
*/
//#define SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE

/*
   Enable plugin microbenchmark: Measure mean CPU cycles
   for getting a single counter value.
*/
//#define SCOREP_PLUGIN_MICROBENCHMARK_ENABLE

/*
    NIC counters acquisition decimation value in order to reduce overhead.
    **** Note, that this definition must be a power of 2.
*/
#define NIC_COUNTERS_UPDATE_DECIMATION (128)

/*
   Enable plugin gathering of NIC counters.
   Note, that this feature is only supporte over HUCX and not over OpenUCX.
*/
#define UCX_STATS_NIC_COUNTERS_ENABLE

/*
   An environment variable that Sets the NIC device name for the
   NIC counters collection.
*/
#define ENV_SCOREP_UCX_PLUGIN_NIC_DEVICE_NAME "SCOREP_UCX_PLUGIN_NIC_DEVICE_NAME"

/*
   An environment variable that enables UCX counters collection.
   values: "enable" / "disable"
*/
#define ENV_SCOREP_UCX_PLUGIN_UCX_COUNTERS_COLLECTION_ENABLE "SCOREP_UCX_PLUGIN_UCX_COLLECTION_ENABLE"

/*
   An environment variable that enables NIC counters collection.
   values: "enable" / "disable"
*/
#define ENV_SCOREP_UCX_PLUGIN_NIC_COUNTERS_COLLECTION_ENABLE "SCOREP_UCX_PLUGIN_NIC_COLLECTION_ENABLE"

#endif
