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
   Enable plugin gathering of NIC counters.
   Note, that this feature is only supporte over HUCX and not over OpenUCX.
*/
//#define UCX_STATS_NIC_COUNTERS_ENABLE

/*
   An environment variable that Sets the NIC device name for the
   NIC counters collection.
*/
#define ENV_SCOREP_UCX_PLUGIN_NIC_DEVICE_NAME "SCOREP_UCX_PLUGIN_NIC_DEVICE_NAME"

#endif
