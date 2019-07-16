import sys
import pandas as pd
import re
import sys
from io import StringIO
from itertools import *

experiments_folder = ""
#experiments_folder = "./"

def output_file(G, P):
    return experiments_folder + G + "." + P + ".cut"

def format_bool(value):
    if value:
        return r"$\bullet$"
    else:
        return r"$\circ$"

G = sys.argv[1] 
partitioners = ["inertialflowcutter4", "inertialflowcutter8", "inertialflowcutter12", "flowcutter3", "flowcutter20", "kahip_v2_11", "metis", "inertial_flow"]
imbalances = [0.0, 0.01, 0.03, 0.05, 0.1, 0.2, 0.3, 0.5, 0.7, 0.9]
partitioner_names = {"flowcutter3" : "F3", "flowcutter20" : "F20", "flowcutter100" : "F100", "inertial_flow" : "I",
                    "metis" : "M", "inertialflowcutter" : "IFC", "kahip_v0_71" : "K0.61", "kahip_v1_00_cut" : "K1.00",
                  	"kahip_v2_11" : "K2.11",
                    "inertialflowcutter4" : "IFC4", "inertialflowcutter8" : "IFC8", "inertialflowcutter12" : "IFC12"}
data = {P: pd.read_csv(output_file(G, P)) for P in partitioners}

def is_number(num):
    pattern = re.compile(r'^[-+]?[-0-9]\d*\.\d*|[-+]?\.?[0-9]\d*$')
    result = pattern.match(num)
    if result:
        return True
    else:
        return False

def make_header(heading1, heading2):
    def midrule(start, end):
        return "\cmidrule(lr){{{}-{}}}".format(start, end)
    span = len(partitioners)
    header = f"\multirow{{2}}{{*}}{{\\rotatebox[origin=c]{{90}}{{$\max\epsilon$}}}}  & \multicolumn{{{span}}}{{c}}{{{heading1}}} & \multicolumn{{{span}}}{{c}}{{{heading2}}}\\\\" + "\n"
    header += midrule(2, 2 + span - 1) + " " + midrule(2 + span, 2 + span + span - 1) + "\n"
    for P in partitioners:
        header += f"& {partitioner_names[P]}"
    for P in partitioners:
        header += f"& {partitioner_names[P]}"
    header += "\\\\\n"
    header += r"\midrule"
    return header

def table_content(key1, format1, key2, format2):


    output = StringIO()
    for eps in imbalances:
        unbalanced = set()
        output.write(f"{int(eps * 100)}")
        for i,P in enumerate(partitioners):
            value = data[P][key1][imbalances.index(eps)]
            if key1 == "achieved_epsilon" and value > eps:
                #output.write("& " + r"\textcolor{red}{\cancel{" + f"{100*value:.3f}" + r"}}")
                output.write("& " + r"\textcolor{red}{\cancel{" + f"{format1(value)}" + r"}}")
                unbalanced.add(i)
            else:
                output.write(f"& {format1(value)}")
        for i,P in enumerate(partitioners):
            value = data[P][key2][imbalances.index(eps)]
            if i in unbalanced:
                output.write("& " + r"\textcolor{red}{\cancel{" + f"{format2(value)}" + r"}}")
            else:
                output.write(f"& {format2(value)}")
        output.write('\\\\\n')
    return output.getvalue() 

col_str = r"R{\mycolwidth}" * len(partitioners) * 2

print(
r"""
\newcolumntype{R}[1]{>{\raggedleft\arraybackslash}p{#1}}
\setlength\arraycolsep{50pt}
\setlength\tabcolsep{2pt}

\setlength\tabcolsep{3pt}
\setlength\mycolwidth{0.74cm}
\setlength\mysmallcolwidth{0.8cm}

\begin{tabular}{r""", end='')
print(col_str, end='')
print("}\n\\toprule")
print(make_header(r"Achieved $\epsilon$ [\%]", r"Cut Size"))
print(table_content("achieved_epsilon", lambda x: r"${<0.1}$" if round(100*x,1) == 0.0 and x != 0.0 else f"{100*x:.1f}", "cut_size", lambda x: x))
print(r"\midrule")
print(make_header(r"Are sides connected?", r"Running Time [s]"))
print(table_content("connected", format_bool, "running_time", lambda x: f"{x:.1f}"))
print(r"\bottomrule")
print(r"\end{tabular}")
