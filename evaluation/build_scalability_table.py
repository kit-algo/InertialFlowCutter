import sys
import pandas as pd
import re

def format_running_time(r):
    r = r.replace("running time : ", "")
    r = r.replace("musec", "")
    return round(int(r) / 1000000, 1)

graph_names = {"col" : "Col", "cal" : "Cal", "europe" : "Eur" , "usa" : "USA"}
df = pd.read_csv(sys.argv[1])
df["running_time_sec"] = df["running_time_musec"].map(format_running_time)
df["graph"] = df["graph"].map(graph_names)


print(r"\begin{tabular}{ll *{5}{r}}")
print(r"\toprule")

print(r"\multirow{2}{*}{Graph} &  & \multicolumn{5}{c}{Cores} \\")
print(r"\cmidrule(lr){3-7}")
print(r" & & ",end='')
print(" & ".join(map(str,[1,2,4,8,16])))
print(r"\\")



for G in ["Col","Cal","Eur","USA"]:
    print(r"\midrule")
    print(r"\multirow{2}{*}{" + G + r"}", end='')
    print(" & Time [s]", end='')
    for T in [1,2,4,8,16]:
        time = float(df[(df.graph==G) & (df.cores==T)].running_time_sec)
        print(" &", time, end='') #slow but who cares
    print(r"\\")
    seq = time = float(df[(df.graph==G) & (df.cores==1)].running_time_sec)
    print(" & Speedup", end='')
    for T in [1,2,4,8,16]:
        time = float(df[(df.graph==G) & (df.cores==T)].running_time_sec)
        print(" &", round(seq / time, 1), end='')
    print(r"\\")
print(r"\bottomrule")
print(r"\end{tabular}")
