import subprocess
import io
import pandas as pd

def inertialflowcutter_pareto(console, graph_path, cutters):
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
    args.append("geo_pos_ordering_cutter_count")
    args.append(str(cutters))

    args.append("flow_cutter_set")
    args.append("ReportCuts")
    args.append("no")

    args.append("flow_cutter_accelerated_enum_cuts")
    args.append("-")



    output = subprocess.check_output(args, universal_newlines=True)
    rename = {'    time' : 'time'}
    return pd.read_csv(io.StringIO(output)).rename(rename, axis='columns')
