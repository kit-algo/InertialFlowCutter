# InertialFlowCutter
C++ Implementation and evaluation scripts for the InertialFlowCutter algorithm to compute Customizable Contraction Hierarchy orders.

If you use this code in a publication, please cite our TR https://arxiv.org/abs/1906.11811

All code in src/ other than flow_cutter_accelerated.h, ford_fulkerson.h was written by Ben Strasser. We modified separator.h, min_fill_in.h and inertial_flow.h. Find his code at https://github.com/kit-algo/flow-cutter.

To compile the code you need a recent C++14 ready compiler (e.g. g++ version >= 8.2) and the Intel Threading Building Blocks (TBB) library.
You also need the readline library https://tiswww.case.edu/php/chet/readline/rltop.html.

We use KaHiP and RoutingKit as submodules. For these, you will also need OpenMP.


## Building

Clone this repository and navigate to the root directory. Then run the following commands.

```shell
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Building on macOS
If you want to build InertialFlowCutter on macOS additional steps are required. As macOS does not support `aligned_alloc` you need to modify `extern/RoutingKit/generate_make_file` and change line 10 to
```python3
compiler_options = ["-Wall", "-DNDEBUG", "-march=native", "-ffast-math", "-std=c++11", "-O3", "-DROUTING_KIT_NO_ALIGNED_ALLOC"]
``` 
Then, run `generate_make_file` to build a suitable Makefile. 

When running cmake, add the flags `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`. This ensures that gcc is used, as CMake fails to properly autodetect gcc on macOS.
