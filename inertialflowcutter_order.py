import subprocess
import sys


binary_path = "./build/"
console = binary_path + "console"

def save_inertialflowcutter_cch_order(G, order_path):
    args = [console]
    args.append("load_routingkit_unweighted_graph")
    args.append(G + "first_out")
    args.append(G + "head")

    args.append("load_routingkit_longitude")
    args.append(G + "longitude")
    args.append("load_routingkit_latitude")
    args.append(G + "latitude")

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
    args.append("8")                                                    #should be multiple of 4. otherwise the four standard directions form the Inertial Flow paper are not chosen.

    args.append("flow_cutter_set")
    args.append("thread_count")
    args.append("1")                                                    #use more parallelism!

    args.append("report_time")
    args.append("reorder_nodes_in_accelerated_flow_cutter_cch_order")
    args.append("do_not_report_time")
    args.append("examine_chordal_supergraph")                           #print some statistics on the CCH

    args.append("save_routingkit_node_permutation_since_last_load")     #use this for binary vector format as used by RoutingKit
    #args.append("save_permutation_of_nodes_since_last_file_load")      #use this for order in text format

    args.append(order_path)
    subprocess.run(args, universal_newlines=True)
            
if __name__ == '__main__':
    save_inertialflowcutter_cch_order(sys.argv[1], sys.argv[2])
