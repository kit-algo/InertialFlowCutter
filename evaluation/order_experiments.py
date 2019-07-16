import pandas as pd
import numpy as np
import re
import subprocess
import os

experiments_folder = ""

graphs = ["col", "cal", "europe", "usa"]
partitioners = ["metis", "kahip_v0_71", "kahip_v1_00_cut", "kahip_v2_11", "inertial_flow", "flowcutter3", "flowcutter20", "flowcutter100","inertialflowcutter4", "inertialflowcutter8", "inertialflowcutter12", "inertialflowcutter16"]

binary_path = "./../build/"
console = binary_path + "console"
customization_binary = binary_path + "customize"
query_binary = binary_path + "query"

def config_contained(G, P, results):
    cpd = pd.DataFrame.from_dict({
                                 'graph' : [G],
                                 'partitioner' : [P],
                                 })
    return len(cpd.merge(results)) > 0

def partitioner_id(P):
    return next(i for i,v in enumerate(partitioners) if v == P)

def graph_id(G):
    return next(i for i,v in enumerate(graphs) if v == G)

def order_path(G,P):
    return experiments_folder + G + "." + P + ".order"

def graph_path(G):
    return experiments_folder + G + "/"

def metric_file(G):
    return graph_path(G) + "travel_time"

def query_file(G):
    return experiments_folder + G + ".q."

def parse_order_log(G,P):
    args = [console]
    args.append("load_routingkit_unweighted_graph")
    
    args.append(graph_path(G) + "first_out")
    args.append(graph_path(G) + "head")
    args.append("add_back_arcs")
    args.append("remove_multi_arcs")
    args.append("remove_loops")
    
    args.append("permutate_nodes_routingkit")
    args.append(order_path(G,P))

    args.append("examine_chordal_supergraph")
    log = subprocess.check_output(args, universal_newlines=True)
    print(log)
    row_dict = dict()
    for l in log.splitlines():
        if l == "":
            continue
        m = re.match(r"^\s*([a-zA-Z_ ]+) : ([0-9.]+)[^0-9]*$", l)
        assert(m)
        name = m.group(1).replace(" ", "_")
        value = m.group(2)
        if '.' in value:
            value = float(value)
        else:
            value = int(value)
        row_dict[name] = value
    return row_dict

def run_customizations(G,P):
    q = query_file(G)
    g = graph_path(G)
    args = [customization_binary, g + "first_out", g + "head", order_path(G,P), metric_file(G), str(1)]
    for x in args:
        print(x, end=' ')
    print()
    runtimes = []
    for i in range(9):
        t = subprocess.check_output(args, universal_newlines=True)
        runtimes.append(float(t) / 1000)    #in ms
        print(t.strip())
    return np.median(np.array(runtimes))

def run_queries(G,P):
    q = query_file(G)
    g = graph_path(G)
    args = [query_binary, g + "first_out", g + "head", order_path(G,P), metric_file(G), q + "s", q + "t"]
    for x in args:
        print(x, end=' ')
    print()
    t = subprocess.check_output(args, universal_newlines=True)
    print(t.strip())
    return float(t)

def main():
    
    order_times = pd.read_csv(experiments_folder + "order_running_time.csv")

    if not os.path.isfile(experiments_folder + "order_experiments.csv"):    #Create nonsensical file
       f = open(experiments_folder + "order_experiments.csv", 'w')
       f.write("graph,partitioner\n")   #Could be anything in csv
       f.close()
    results = pd.read_csv(experiments_folder + "order_experiments.csv")

    for G in graphs:
        for P in partitioners:
            if not os.path.isfile(order_path(G,P)):
                print("Warning: order for partitioner", P, "on graph", G, "missing. Skip.")
                continue
            if config_contained(G, P, results):
                print("Skipping", P, G, "because this config was already run")
                continue
            print("Running", P, G)
            row_dict = dict()
            row_dict["graph"] = G
            row_dict["partitioner"] = P
            print("parsing order log")
            row_dict.update(parse_order_log(G,P))
            row_dict["order_running_time"] = float(order_times[(order_times.partitioner==P) & (order_times.graph==G)].order_running_time_sec)   #If this fails, the order exists, but the running time is not in order_running_time.csv
            print("running customization")
            row_dict["median_customization_time"] = run_customizations(G,P)
            print("running queries")
            row_dict["avg_query_time"] = run_queries(G,P)
            print(row_dict)
            results = results.append(pd.DataFrame([row_dict]), ignore_index=True)

    print("Order experiments done.")
    new_cols = list(results.columns)
    new_cols.remove("graph")
    new_cols.remove("partitioner")
    new_cols = ["graph", "partitioner"] + new_cols
    results = results[new_cols]
    results["graph_id"] = results["graph"].map(graph_id)
    results["partitioner_id"] = results["partitioner"].map(partitioner_id)
    results.sort_values(["graph_id", "partitioner_id"], ascending=[True,True], inplace=True)
    results.drop(columns=["graph_id", "partitioner_id"])
    results.to_csv(experiments_folder + "order_experiments.csv", index=False)
    


if __name__ == '__main__':
    main()
