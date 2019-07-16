import pandas as pd
import numpy as np
import subprocess
import sys
import re

experiments_folder = ""

binary_path = "./../build/"
console = binary_path + "console"

def order_path(G,P):
    return experiments_folder + G + "." + P + ".order"

def graph_path(G):
    return experiments_folder + G + "/"

def log_path(G,P):
    return order_path(G,P) + ".log"

def parse_order_log(G,P):
    log = open(log_path(G,P))
    row_dict = dict()
    for l in log:
        m = re.match(r"^\s*([a-zA-Z_ ]+) : ([0-9.]+)[^0-9]*$", l)
        assert(m)
        name = m.group(1).replace(" ", "_")
        value = m.group(2)
        if '.' in value:
            value = float(value)
        else:
            value = int(value)
        if "running_time" in name:
            name = "order_running_time"
            value /= 1000000        #in seconds
        row_dict[name] = value
    log.close()
    return row_dict

def save_inertialflowcutter_cch_order(cutters, G):
    P = "inertialflowcutter" + str(cutters)

    args = [console]
    args.append("load_routingkit_unweighted_graph")
    args.append(graph_path(G) + "first_out")
    args.append(graph_path(G) + "head")

    args.append("load_routingkit_longitude")
    args.append(graph_path(G) + "longitude")
    args.append("load_routingkit_latitude")
    args.append(graph_path(G) + "latitude")

    args.append("add_back_arcs")
    args.append("remove_multi_arcs")
    args.append("remove_loops")

    args.append("flow_cutter_set")
    args.append("random_seed")
    args.append("5489")
    args.append("reorder_nodes_at_random")
    args.append("reorder_nodes_in_preorder")
    args.append("sort_arcs")

    args.append("flow_cutter_set")
    args.append("geo_pos_ordering_cutter_count")
    args.append(str(cutters))

    args.append("report_time")
    args.append("reorder_nodes_in_accelerated_flow_cutter_cch_order")
    args.append("do_not_report_time")
    args.append("examine_chordal_supergraph")

    args.append("save_routingkit_node_permutation_since_last_load")
    args.append(order_path(G,P))
    with open(log_path(G,P), 'w') as f:
        subprocess.run(args, universal_newlines=True, stdout=f)


def main():
    G = sys.argv[1]
    cutters = int(sys.argv[2])
    P = "inertialflowcutter" + str(cutters)
    save_inertialflowcutter_cch_order(cutters, G)
    order_time = parse_order_log(G,P)["order_running_time"]
    print(P, G, round(order_time, 3), sep=',')
            
if __name__ == '__main__':
    main()
