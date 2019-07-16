# InertialFlowCutter
C++ Implementation and evaluation scripts for the InertialFlowCutter algorithm to compute Customizable Contraction Hierarchy orders.

If you use this code in a publication, please cite our TR https://arxiv.org/abs/1906.11811

All code in src/ other than flow_cutter_accelerated.h, ford_fulkerson.h was written by Ben Strasser. We modified separator.h, min_fill_in.h and inertial_flow.h. Find his code at https://github.com/kit-algo/flow-cutter.

To compile the code you need a recent C++14 ready compiler (e.g. g++ version >= 8.2) and the Intel Threading Building Blocks (TBB) library.
You also need the readline library https://tiswww.case.edu/php/chet/readline/rltop.html.

We use KaHiP and RoutingKit as submodules. For these, you will also need OpenMP.


## Building

Clone this repository `git clone --recursive` and navigate to the root directory. Then run the following commands.

```shell
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Building on macOS
If you want to build InertialFlowCutter on macOS additional steps are required. As macOS does not support `aligned_alloc` you need to modify `extern/RoutingKit/generate_make_file` and change line 10 to
```shell
compiler_options = ["-Wall", "-DNDEBUG", "-march=native", "-ffast-math", "-std=c++11", "-O3", "-DROUTING_KIT_NO_ALIGNED_ALLOC"]
``` 
Then, run `generate_make_file` to build a suitable Makefile. 

When running cmake, add the flags `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`. This ensures that gcc is used, as CMake fails to properly autodetect gcc on macOS.

## Computing a Contraction Order

Currently, we expect the input graph in RoutingKit's format, i.e. a directory containing five files `first_out`, `head`, `travel_time`, `latitude`, `longitude` in RoutingKit's binary vector format.
The first two represent the graph in CSR format, the third contains the metric information, the fourth and fifth the geo-coordinates.
You can convert from Metis and Dimacs format using tools in this repository, see evaluation/README.md for further details; or write your own converter.

Run
```shell
python3 inertialflowcutter_order.py <graph_path> <order_path>
```
to obtain a CCH order and store it at `<order_path>`, again in the RoutingKit binary vector format.
You can instead get it in text format by uncommenting line 44.
This will use the suggested default parameters from the paper.
For more parallelism (yes, please), increase the thread_count parameter in line 36.
You can specify the number of cutters in line 32, however more than 8 do not seem particularly useful.
