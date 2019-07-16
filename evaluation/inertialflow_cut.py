import subprocess
import re

def inertialflow_cut(console, graph_path, balance):
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

    args.append("report_time")
    args.append("inertial_flow_cut")
    args.append(str(balance))
    args.append("do_not_report_time")
    args.append("examine_node_color_cut")

    output = subprocess.check_output(args, universal_newlines=True)
    row_dict = {}
    for l in output.splitlines():
        m = re.match(r"^\s*([a-zA-Z_ ]+) : ([0-9.]+)[^0-9]*$", l)
        assert(m)
        name = m.group(1).replace(" ", "_")
        value = m.group(2)
        if '.' in value:
            value = float(value)
        else:
            value = int(value)
        if "running_time" in name:
            value /= 1000000        #in seconds
        row_dict[name] = value
    return row_dict
