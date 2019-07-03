# InertialFlowCutter
C++ Implementation and evaluation scripts for the InertialFlowCutter algorithm to compute Customizable Contraction Hierarchy orders.

If you use this code in a publication, please cite our TR https://arxiv.org/abs/1906.11811

All code in src/ other than flow_cutter_accelerated.h, ford_fulkerson.h was written by Ben Strasser. We modified separator.h, min_fill_in.h and inertial_flow.h. Find his code at https://github.com/kit-algo/flow-cutter.

To compile the code you need a recent C++14 ready compiler (e.g. g++ version >= 8.2) and the Intel Threading Building Blocks (TBB) library.
You also need the readline library https://tiswww.case.edu/php/chet/readline/rltop.html.

We use KaHiP and RoutingKit as submodules. For these, you will also need OpenMP.

Steps for installation (on Ubuntu):
```
# Install the prerequisites
sudo apt install cmake mpich2 libreadline-dev libtbb-dev

# Get the code
git clone --recursive https://github.com/kit-algo/InertialFlowCutter
cd InertialFlowCutter
mkdir build && cd build

# Build
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j4
```
