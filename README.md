# Scorep UCX plugin

Score-P UCX data acquisition plugin

First, download, build and install the Huawei updated Score-P (currently not supported in the Score-P release),
```
git clone https://github.com/shuki-zanyovka/scorep_6.0
```

Then, download and build the UCX plugin,

```
git clone --recurse-submodules  https://github.com/shuki-zanyovka/scorep_plugin_ucx
cd scorep_plugin_ucx
```

To build:

```
mkdir BUILD
cd BUILD

cmake ../ -DCMAKE_C_STANDARD_COMPUTED_DEFAULT=GNU -DCMAKE_CXX_STANDARD_COMPUTED_DEFAULT=GNU -DCMAKE_CXX_COMPILER=mpic++ -DCMAKE_C_COMPILER=mpicc
make
```

# OpenMPI patches

For using with OpenMPI, please review and apply all patches under ./openmpi-patches directory (already applied in HMPI).

# To use the plugin for data acquisition, please enable the plugin as follows,

```
export LOCAL_USER_SCOREP_INSTALL_PATH=<Score-P 6.0 Install Path>/scorep-6.0
export SCOREP_PLUGIN_UCX_PATH=<Plugin Path>/scorep_plugin_ucx/BUILD
export OMPI_PATH=<OpenMPI Install Path>
```

# Set path for the UCX installation 

```
export UCX_INSTALL_PATH=<UCX installation path, profiling enabled>/ucx-prof
export UCS_LIB_PATH="$UCX_INSTALL_PATH/lib:$UCX_INSTALL_PATH/lib/ucx"

export PATH=$OMPI_PATH/bin:$LOCAL_USER_SCOREP_INSTALL_PATH/bin:$PATH

export LD_LIBRARY_PATH=$SCOREP_PLUGIN_UCX_PATH:$SCOREP_PLUGIN_MPI_PATH:$OMPI_PATH:$UCS_LIB_PATH:$LOCAL_USER_SCOREP_INSTALL_PATH:$LD_LIBRARY_PATH

export SCOREP_METRIC_PLUGINS="scorep_plugin_ucx"
```

# When using UCX@N, the number of UCX counters stored is limited to max = N. For example,

```
export SCOREP_METRIC_SCOREP_PLUGIN_MPI=UCX@20
```

# Disable profiling and enable tracing,

```
export SCOREP_ENABLE_PROFILING=false
export SCOREP_ENABLE_TRACING=true
export SCOREP_TOTAL_MEMORY=4000M
```

# It is recommended to use a Score-P filter file to reduce Score-P overhead,

```
export SCOREP_FILTERING_FILE=./filter.scorep
```

# It is recommended to apply a UCX statistics filter as follows,

```
export UCX_STATS_FILTER="rx_am*,bytes_short,bytes_bcopy,bytes_zcopy,rx*,tx*"
```

# UCX should be configured to send statistics via UDP,

```
export UCX_STATS_DEST="udp:localhost:37873"
```

# Contact us at,

shuki.zanyovka@huawei.com

# Enable tracing

```
export SCOREP_ENABLE_TRACING=true
```

# Note, that profiling must be disabled when using the UCX data acquisition plugin

```
export SCOREP_ENABLE_PROFILING=false

export SCOREP_TOTAL_MEMORY=1000M

export UCX_STATS_DEST="udp:localhost:37873"
```

# Apply UCX statistics filtering as follows,

```
export UCX_STATS_FILTER="rx_am*,bytes_short,bytes_bcopy,bytes_zcopy,rx*,tx*"

```
# Following configuration, execute the MPI application as follows,
```
mpirun -n 2 <mpi_application>
```

# UCX aggregate-sum counters statistics
```
The Score-P UCX plugin uses the UCX aggregate-sum counters statistics by default - This basically means that,
1) All counters values of the same class/type will be saved in the same counter in the trace.
2) The user can build the plugin without using this feature by enabling the SCOREP_PLUGIN_UCX_STATISTICS_LEGACY_ENABLE flag in scorep_plugin_ucx.h

Please note, that the user should set the environment variables as follows to collect the UCX software counters,
# Enable UCX SW counters collection.
export SCOREP_UCX_PLUGIN_UCX_COLLECTION_ENABLE=1
# Enable NIC counters collection.
export SCOREP_UCX_PLUGIN_NIC_COLLECTION_ENABLE=0
```

# UCX Nvidia Connectx-* NIC counters aggregate-sum counters statistics
```
In a separate run, the user can enable collecting the aggregate-sum of the Nvidia Connectix-* NIC counters by setting the following environment variables:

# interface name, can be acquired using the 'ibdev2netdev' command as seen in the example below.
export SCOREP_UCX_PLUGIN_NIC_DEVICE_NAME="ens2f0"
# Enable UCX SW counters collection.
export SCOREP_UCX_PLUGIN_UCX_COLLECTION_ENABLE=0
# Enable NIC counters collection.
export SCOREP_UCX_PLUGIN_NIC_COLLECTION_ENABLE=1

$ ibdev2netdev
mlx5_0 port 1 ==> ens3f0 (Up)
mlx5_1 port 1 ==> ib0 (Down)
mlx5_2 port 1 ==> ens2f0 (Up)
mlx5_3 port 1 ==> ib1 (Down)

```

