import subprocess
import re, os

def graph_to_metis(console, graph_path, out_path):
    args = [console]

    args.append("load_routingkit_unweighted_graph")
    args.append(graph_path + "first_out")
    args.append(graph_path + "head")

    args.append("load_routingkit_longitude")
    args.append(graph_path + "longitude")
    args.append("load_routingkit_latitude")
    args.append(graph_path + "latitude")

    args.append("assign_constant_arc_weights")
    args.append("1")

    args.append("add_back_arcs")
    args.append("remove_multi_arcs")
    args.append("remove_loops")

    args.append("flow_cutter_set")
    args.append("random_seed")
    args.append("5489")
    args.append("reorder_nodes_at_random")
    args.append("reorder_nodes_in_preorder")
    args.append("sort_arcs")
    
    args.append("save_metis_graph")
    args.append(out_path)
    subprocess.run(args)

def examine_cut(console, graph_path, partition):
    args = [console]
    args.append("load_metis_graph")
    args.append(graph_path)
    args.append("load_node_color_partition")
    args.append(partition)
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
        row_dict[name] = value
    return row_dict

def metis_cut(metis, console, graph_path, epsilon):
    graph_to_metis(console, graph_path, "tmp.graph")
    args = [metis]
    args.append("tmp.graph")
    args.append("2")
    if epsilon == 0:
        epsilon = 0.001
    args.append(f"-ufactor={int(epsilon * 1000)}")
    output = subprocess.check_output(args, universal_newlines=True)
    row_dict = {}
    for l in output.splitlines():
        if l.startswith("  Partitioning:"):
            print(l)
            m = re.search(r"([0-9.]+) sec", l)
            if m:
                row_dict["running_time"] = float(m.group(1))
    metrics = examine_cut(console, "tmp.graph",  "tmp.graph.part.2")
    row_dict.update(metrics)
    os.remove("tmp.graph.part.2")
    os.remove("tmp.graph")
    return row_dict
