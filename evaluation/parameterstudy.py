import os
import pathlib
import re
import subprocess

import configurable_inertialflowcutter_order as ifc
import numpy as np
import pandas as pd


experiments_folder = ""
graph = "col"       #TODO replace again with europe
graph_path = experiments_folder + graph + "/"
metric_path = graph_path + "travel_time"
query_sources = experiments_folder + graph + ".q.s"
query_targets = experiments_folder + graph + ".q.t"

binary_path = pathlib.Path(__file__).parent.absolute() / "../build"
order_console = str(binary_path / "console")
customization_binary = str(binary_path / "customize")
query_binary = str(binary_path / "query")


def config_contained(config, results):
    cpd = pd.DataFrame([config._asdict()])
    return len(cpd.merge(results)) > 0

def config_to_string(config):
    return '.'.join(map(str,config))

def order_path(config):
    return experiments_folder + "parameterstudy/" + graph + "." + config_to_string(config) + ".order"

def log_path(config):
    return order_path(config) + ".log"

def parse_order_log(config):
    log = open(log_path(config))
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
    return row_dict

def run_customizations(config):
    args = [customization_binary, graph_path + "first_out", graph_path + "head", order_path(config), metric_path, str(1)]
    runtimes = []
    for i in range(9):
        t = subprocess.check_output(args, universal_newlines=True)
        runtimes.append(float(t) / 1000)    #in ms
    return np.median(np.array(runtimes))

def run_queries(config):
    args = [query_binary, graph_path + "first_out", graph_path + "head", order_path(config), metric_path, query_sources, query_targets]
    t = subprocess.check_output(args, universal_newlines=True)
    return float(t)

def main():
    configs = pd.read_csv(experiments_folder + "parameterstudy_configs.csv")
    if not os.path.isfile(experiments_folder + "parameterstudy.csv"):
        x = pd.DataFrame(columns=["geo_distance_cutters","hop_distance_cutters","initial_assimilated_fraction","bulk_step_fraction","bulk_assimilation_order_threshold","bulk_assimilation_threshold"])
        x.to_csv(experiments_folder + "parameterstudy.csv", index=False)
    results = pd.read_csv(experiments_folder + "parameterstudy.csv")

    for config in configs.itertuples(index=False):
        if not config_contained(config, results):
            print("computing order with config", config)
            ifc.save_inertialflowcutter_cch_order(config, order_console, graph_path, order_path(config), log_path(config))
            row_dict = config._asdict()
            row_dict.update(parse_order_log(config))
            print("running customization")
            row_dict["median_customization_time"] = run_customizations(config)
            print("running queries")
            row_dict["avg_query_time"] = run_queries(config)
            print(row_dict)
            results = results.append(pd.DataFrame([row_dict]), ignore_index=True)

    results.sort_values([x for x in configs.columns], ascending=[True for i in configs.columns], inplace=True)
    results.to_csv(experiments_folder + "parameterstudy.csv", index=False)  #careful
if __name__ == '__main__':
    main()
