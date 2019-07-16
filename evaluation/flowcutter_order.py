import subprocess
import re
import sys

experiments_folder = ""

binary_path = "./../build/"
console = binary_path + "console"

def graph_path(G):
    return experiments_folder + G + "/"

def order_path(G,ncutters):
    return experiments_folder + G + ".flowcutter" + str(ncutters) + ".order"

def log_path(G,ncutters):
    return order_path(G,ncutters) + ".log"

def save_flowcutter_cch_order(console, ncutters, graph_path, order_path, info_log):
    args = [console]

    args.append("load_routingkit_unweighted_graph")
    args.append(graph_path + "first_out")
    args.append(graph_path + "head")

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
    args.append("cutter_count")
    args.append(str(ncutters))

    args.append("report_time")
    args.append("reorder_nodes_in_flow_cutter_cch_order")
    args.append("do_not_report_time")
    args.append("examine_chordal_supergraph")

    #args.append("save_permutation_of_nodes_since_last_file_load")
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
    G = sys.argv[1]
    q = int(sys.argv[2])
    P = "flowcutter" + str(q)
    save_flowcutter_cch_order(console, q, graph_path(G), order_path(G,q), log_path(G,q))
    order_time = parse_order_log(log_path(G,q))["order_running_time"]
    print(P, G, round(order_time, 3), sep=',')

if __name__ == '__main__':
    main()
