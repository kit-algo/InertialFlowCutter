import subprocess
import re
import sys

experiments_folder = ""

binary_path = "./../build/"
console = binary_path + "console"

def graph_path(G):
    return experiments_folder + G + "/"

def order_path(G):
    return experiments_folder + G + ".kahip_v2_11.order"

def log_path(G):
    return order_path(G) + ".log"

def save_kahip_cch_order(console, graph_path, order_path, info_log):
    args = [console]

    args.append("load_routingkit_unweighted_graph")
    args.append(graph_path + "first_out")
    args.append(graph_path + "head")

    args.append("add_back_arcs")
    args.append("remove_multi_arcs")
    args.append("remove_loops")
    args.append("sort_arcs")

    args.append("flow_cutter_set")
    args.append("random_seed")
    args.append("5489")
    
    args.append("report_time")
    args.append("reorder_nodes_in_kahip_nested_dissection_order")
    args.append("0.2")   #maximum imbalance
    args.append("do_not_report_time")
    args.append("examine_chordal_supergraph")

    args.append("save_routingkit_node_permutation_since_last_load")
    args.append(order_path)

    with open(info_log, 'w') as f:
        subprocess.run(args, universal_newlines=True, stdout=f)

def parse_order_log(log_path):
    log = open(log_path)
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

def main():
    P="kahip_v2_11"
    G = sys.argv[1]
    save_kahip_cch_order(console, graph_path(G), order_path(G), log_path(G))
    order_time = parse_order_log(log_path(G))["order_running_time"]
    print(P, G, round(order_time, 3), sep=',')

if __name__ == '__main__':
    main()
