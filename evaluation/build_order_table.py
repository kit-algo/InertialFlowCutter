import sys
import pandas as pd
import re

def is_number(num):
    pattern = re.compile(r'^[-+]?[-0-9]\d*\.\d*|[-+]?\.?[0-9]\d*$')
    result = pattern.match(num)
    if result:
        return True
    else:
        return False


df = pd.read_csv(sys.argv[1])
f = open(sys.argv[2], 'w')

                        #Graph/Algo, search space, Arcs in CCH/Triangles/Treewidth, running times order/customization/queries
print(  r"""\begin{tabular}{ *{2}{c} *{4}{r} *{3}{r} *{3}{r} }
            \toprule
            & & \multicolumn{4}{c}{Search Space} & CCH & & Up. & \multicolumn{3}{c}{Running times} \\
            \cmidrule(lr){3-6} \cmidrule(lr){10-12}
            & & \multicolumn{2}{c}{Nodes} & \multicolumn{2}{c}{Arcs {[}$\cdot10^{3}${]}} & Arcs &  \#Tri. & Tw. & Order & Cust. & Query \\
            \cmidrule(lr){3-4} \cmidrule(lr){5-6} 
            & & Avg. & Max.& Avg. & Max. & {[}$\cdot10^{6}${]} & {[}$\cdot10^{6}${]} & Bd. & {[}s{]} & {[}ms{]} & {[}$\mu$s{]}
        """, file=f)
highlight_min = True
columns = ['partitioner',
            'average_elimination_tree_depth', 'elimination_tree_height', 'average_arcs_in_search_space', 'maximum_arcs_in_search_space', 
            'super_graph_upward_arc_count', 'number_of_triangles_in_super_graph', 'upper_tree_width_bound', 
            'order_running_time', 'median_customization_time', 'avg_query_time']


df["super_graph_upward_arc_count"] = df["super_graph_upward_arc_count"].map(lambda x : x/100000)
df["number_of_triangles_in_super_graph"] = df["number_of_triangles_in_super_graph"].map(lambda x : x/100000)
df["average_arcs_in_search_space"] = df["average_arcs_in_search_space"].map(lambda x : x/1000)
df["maximum_arcs_in_search_space"] = df["maximum_arcs_in_search_space"].map(lambda x : x/1000)

for column in columns:
    if df[column].dtype == 'float':
        df[column] = df[column].map(lambda x : round(x,1))

graph_names = {"col" : "Col", "cal" : "Cal", "europe" : "Eur" , "usa" : "USA"}
df["graph"] = df["graph"].map(graph_names)

#Why did Ben call kahip_v0_71 K0.61?
partitioner_names = {"flowcutter3" : "F3", "flowcutter20" : "F20", "flowcutter100" : "F100", "inertial_flow" : "I",
                    "metis" : "M", "inertialflowcutter" : "IFC", "kahip_v0_71" : "K0.61", "kahip_v1_00_cut" : "K1.00", "kahip_v2_11" : "K2.11",
                    "inertialflowcutter4" : "IFC4", "inertialflowcutter8" : "IFC8", "inertialflowcutter12" : "IFC12", "inertialflowcutter16" : "IFC16"}
df["partitioner"] = df["partitioner"].map(partitioner_names)


graph = "nonsense"
print(r"\\ ", end='', file=f)
for row in range(len(df)):
    new_graph = False
    if df.loc[row, "graph"] != graph:
        graph = df.loc[row,"graph"]
        new_graph = True
    gdf = df[df["graph"] == graph].copy()
    num_algos = len(gdf.index)
    if new_graph:
        print(r"\midrule", file=f)
        print(r"\multirow{" + str(num_algos) + r"}{*}{\begin{sideways}" + graph + r" \end{sideways}}", end='', file=f)
    #for i in range(num_algos):
    for column in columns:
        print(' & ', end='', file=f)
        value = gdf.loc[row, column]
        formatted_value = value
        
        if is_number(str(value)):
            comp = re.compile('([0-9]{3}(?=[0-9]))')
            formatted_value = re.sub( comp, '\\g<0>,\\\\', str(value)[::-1] )[::-1]
        
        if column != 'partitioner' and highlight_min and value == gdf[column].min() and column in columns[1:]:
            print(r"\bfseries{", formatted_value, r"}", sep='', end='', file=f)
        else:
            print(formatted_value, end='', file=f)
    print('\n', r"\\", sep='', file=f)
        #row += 1
print(r"\bottomrule", file=f)
print(r"\end{tabular}", file=f)
f.close()
