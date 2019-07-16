# Running Experiments

Disclaimer: The evaluation scripts are not particularly clean and contain a lot of duplication because they were hacked together quickly.

## General Setup
1. Every script references an experiment folder. Make sure the path is set appropriately before you run a script. The folder should contain all the necessary files, i.e., graphs, contraction orders, logs for the specific script. Just putting everything in one folder works fine, e.g., this folder.

2. Everything related to computing contraction orders is controlled from the one monolithic console binary. Yes, we know. It emulates a terminal that can run a bunch of commands, as specified in console.cpp. 
```shell
./console interactive
```
starts the console in interactive mode. You can list all commands via the help command. Autocompletion works if you install the readline library https://tiswww.case.edu/php/chet/readline/rltop.html.

In the current state of the evaluation setup, the graphs and orders are expected in RoutingKit's binary vector format. For every graph we expect a directory containing five files `first_out`, `head`, `travel_time`, `latitude`, `longitude` in binary vector format.
The first two represent the graph in CSR format, the third contains the metric information, the fourth and fifth the geo-coordinates.
Get the graphs in Dimacs format, including geo-coordinates, at http://users.diag.uniroma1.it/challenge9/download.shtml.
Convert from Dimacs format to RoutingKit format by starting the console and loading the graph in one format (load_dimacs_graph, load_dimacs_geo_pos) and saving in another (save_routingkit_unweighted_graph, save_routingkit_latitude, save_routingkit_longitude).
The scripts expect the four graphs from the paper in directories named col, cal, europe, usa.

3. Get python3.

## Metis
For reproducing the experiments, you should get Metis from http://glaros.dtc.umn.edu/gkhome/metis/metis/download and put `ndmetis` and `gpmetis`in this directory. If you don't want to do that, just remove Metis from the list of partitioners, where appropriate.

## Top-Level Cut Experiments 
1. Build toplevel cut stats using
```shell
python3 toplevel_cut_experiments.py
```
Only non-existing files will be generated in the format `Graph.Partitioner.cut` for all graphs and partitioners.

2. Build pareto experiment table using
```shell
python3 build_pareto_table.py <graphname>
```
for the specified graph. The latex table will be written to stdout.
If you did not obtain Metis or KaHiP, you should remove them from the partitioners list in both scripts.

## Contraction Order Experiments

1. Compute contraction orders on the europe graph with all considered partitioners (Metis, Inertial Flow, FlowCutter, KaHiP and InertialFlowCutter) by calling the super-script, which calls the scripts for the different partitioners
```shell
chmod +x metis_order.sh compute_order_for_all_partitioners.sh
./compute_orders_for_all_partitioners.sh europe
```
 This will take a while, especially if you run FlowCutter20/100 and KaHiP.
 If you did not get Metis or KaHiP, just remove them from the script.
 The orders are stored in RoutingKit's binary vector format, with filenames in the format `Graph.Partitioner.order`, as well as a log with a `.log`suffix.

2. Generate random test queries
```shell
./../extern/RoutingKit/bin/generate_test_queries <node_count> <query_count> <random_seed> <source_file> <target_file>
```
`<source_file>` must be `graph.q.s`, e.g. `europe.q.s` for the europe graph. Similarly `europe.q.t` for `<target_file>`.

3. Run customizations and queries: `python3 order_experiments.py` and generate the table from the paper `python3 build_order_table.py order_experiments.csv order_table.tex`
If an order file is missing, the script will issue a warning and continue. You can repeatedly call the script and it will only run configurations that were not run before. If you want to rerun certain configurations, delete the corresponding lines in `order_experiments.csv`

## Parameter Study

1. Run
```shell
python3 parameterstudy.py
python3 build_parameterstudy_table.py parameterstudy.csv parameterstudy_table.tex
```

This will take a while since it runs 42 different configurations of InertialFlowCutter.
