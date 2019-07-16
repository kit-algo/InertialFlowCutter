import subprocess

def save_inertialflowcutter_cch_order(config, console, graph_path, order_path, info_log):
    args = [console]

    args.append("load_routingkit_unweighted_graph")
    args.append(graph_path + "first_out")
    args.append(graph_path + "head")

    args.append("load_routingkit_longitude")
    args.append(graph_path + "longitude")
    args.append("load_routingkit_latitude")
    args.append(graph_path + "latitude")

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
    args.append("max_cut_size")
    args.append("100000000")

    args.append("flow_cutter_set")
    args.append("distance_ordering_cutter_count")
    args.append(str(config.hop_distance_cutters))

    args.append("flow_cutter_set")
    args.append("geo_pos_ordering_cutter_count")
    args.append(str(config.geo_distance_cutters))

    args.append("flow_cutter_set")
    args.append("bulk_assimilation_threshold")
    args.append(str(config.bulk_assimilation_threshold))

    args.append("flow_cutter_set")
    args.append("bulk_assimilation_order_threshold")
    args.append(str(config.bulk_assimilation_order_threshold))

    args.append("flow_cutter_set")
    args.append("bulk_step_fraction")
    args.append(str(config.bulk_step_fraction))

    args.append("flow_cutter_set")
    args.append("initial_assimilated_fraction")
    args.append(str(config.initial_assimilated_fraction))

    args.append("report_time")
    args.append("reorder_nodes_in_accelerated_flow_cutter_cch_order")
    args.append("do_not_report_time")
    args.append("examine_chordal_supergraph")

    #args.append("save_permutation_of_nodes_since_last_file_load")
    args.append("save_routingkit_node_permutation_since_last_load")
    args.append(order_path)
    with open(info_log, 'w') as f:
        subprocess.run(args, universal_newlines=True, stdout=f)
