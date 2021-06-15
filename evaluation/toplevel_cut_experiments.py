import os
import pathlib
import re

import flowcutter_cut as flowcutter
import inertialflow_cut as inertialflow
import inertialflowcutter_cut as ifc
import kahip_cut as kahip
import metis_cut as metis
import pandas as pd

experiments_folder = ""
graphs = ["col", "cal", "europe", "usa"]
partitioners = ["metis", "kahip_v2_11", "inertial_flow",  "flowcutter3", "flowcutter20", "inertialflowcutter4", "inertialflowcutter8", "inertialflowcutter12", "inertialflowcutter16"]
imbalances = [0.0, 0.01, 0.03, 0.05, 0.1, 0.2, 0.3, 0.5, 0.7, 0.9]

binary_path = pathlib.Path(__file__).parent.absolute() / "../build"
console = str(binary_path / "console")
metis_path = "./" + experiments_folder + "gpmetis"

def graph_path(G):
    return experiments_folder + G + "/"

def output_file(G, P):
    return experiments_folder + G + "." + P + ".cut"

def compute_cuts(G, P):
    if P == "metis":
        return compute_metis_cuts(G)
    elif P == "kahip_v2_11":
        return compute_kahip_cuts(G, P, old=False)
    elif P.startswith("flowcutter"):
        cutters = int(re.match(r"flowcutter([0-9]+)", P).group(1))
        return compute_flow_cutter_cuts(G, cutters)
    elif P == 'inertial_flow':
        return compute_inertial_flow_cuts(G)
    elif P.startswith("inertialflowcutter"):
        cutters = int(re.match(r"inertialflowcutter([0-9]+)", P).group(1))
        return compute_inertial_flow_cutter_cuts(G, cutters)
    else:
        assert(false)

def compute_metis_cuts(G):
    rows = []
    for epsilon in imbalances:
        row_dict = {}
        metrics = metis.metis_cut(metis_path, console, graph_path(G), epsilon)
        row_dict["epsilon"] = epsilon
        row_dict["achieved_epsilon"] = metrics["epsilon"]
        row_dict["cut_size"] = metrics["cut_size"]
        row_dict["running_time"] = metrics["running_time"]
        row_dict["connected"] = metrics["left_components"] == 1 and metrics["right_components"] == 1
        rows.append(row_dict)
    results = pd.DataFrame(rows)
    return results.set_index("epsilon").sort_index()

def compute_kahip_cuts(G, P, old):
    rows = []
    for epsilon in imbalances:
        row_dict = {}
        metrics = kahip.kahip_cut(console, graph_path(G), epsilon)
        row_dict["epsilon"] = epsilon
        row_dict["achieved_epsilon"] = metrics["epsilon"]
        row_dict["cut_size"] = metrics["cut_size"]
        row_dict["running_time"] = metrics["running_time"]
        row_dict["connected"] = metrics["left_components"] == 1 and metrics["right_components"] == 1
        rows.append(row_dict)
    results = pd.DataFrame(rows)
    return results.set_index("epsilon").sort_index()

def compute_inertial_flow_cuts(G):
    rows = []
    for epsilon in imbalances:
        row_dict = {}
        metrics = inertialflow.inertialflow_cut(console, graph_path(G), (1 - epsilon) / 2)
        row_dict["epsilon"] = epsilon
        row_dict["achieved_epsilon"] = metrics["epsilon"]
        row_dict["cut_size"] = metrics["cut_size"]
        row_dict["running_time"] = metrics["running_time"]
        row_dict["connected"] = metrics["left_components"] == 1 and metrics["right_components"] == 1
        rows.append(row_dict)
    results = pd.DataFrame(rows)
    return results.set_index("epsilon").sort_index()

def compute_flow_cutter_cuts(G, cutters):
    cuts = flowcutter.flowcutter_pareto(console, graph_path(G), cutters)
    node_count = cuts.iloc[-1]["small_side_size"] + cuts.iloc[-1]["large_side_size"]
    cuts['imbalance'] = cuts["large_side_size"] / ((node_count + 1) // 2) - 1
    rows = []
    for epsilon in imbalances:
        row_dict = {}
        cut = cuts[cuts.imbalance <= epsilon].iloc[0]
        row_dict["epsilon"] = epsilon
        row_dict["achieved_epsilon"] = float(cut["imbalance"])
        row_dict["cut_size"] = int(cut["cut_size"])
        row_dict["running_time"] = cut["time"] * 1e-6
        row_dict["connected"] = True
        rows.append(row_dict)
    results = pd.DataFrame(rows)
    return results.set_index("epsilon").sort_index()

def compute_inertial_flow_cutter_cuts(G, cutters):
    cuts = ifc.inertialflowcutter_pareto(console, graph_path(G), cutters)
    node_count = cuts.iloc[-1]["small_side_size"] + cuts.iloc[-1]["large_side_size"]
    cuts['imbalance'] = cuts["large_side_size"] / ((node_count + 1) // 2) - 1
    rows = []
    for epsilon in imbalances:
        row_dict = {}
        cut = cuts[cuts.imbalance <= epsilon].iloc[0]
        row_dict["epsilon"] = epsilon
        row_dict["achieved_epsilon"] = float(cut["imbalance"])
        row_dict["cut_size"] = int(cut["cut_size"])
        row_dict["running_time"] = cut["time"] * 1e-6
        row_dict["connected"] = True
        rows.append(row_dict)
    results = pd.DataFrame(rows)
    return results.set_index("epsilon").sort_index()

def main():
    for G in graphs:
        for P in partitioners:
            if not os.path.exists(output_file(G, P)):
                print(P,G,"compute cuts")
                cuts = compute_cuts(G, P)
                cuts.to_csv(output_file(G, P))
            else:
                print(P,G,"skip cuts")

if __name__ == '__main__':
    main()
