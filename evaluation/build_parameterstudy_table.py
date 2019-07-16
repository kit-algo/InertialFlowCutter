import sys
import re
import pandas as pd

def is_number(num):
    pattern = re.compile(r'^[-+]?[-0-9]\d*\.\d*|[-+]?\.?[0-9]\d*$')
    result = pattern.match(num)
    if result:
        return True
    else:
        return False

color = "cyan"
def get_color(value, column):
    col_min = column.min()
    col_max = column.max()
    if col_max == col_min:
        return f"{color}!{100}"    
    bucket_size = (col_max - col_min) / 10
    bucket = (value - col_min) // bucket_size
    bucket = min(bucket, 10)
    return f"{color}!{100 - bucket*10}"

df = pd.read_csv(sys.argv[1])

f = open(sys.argv[2], 'w')

                        #config, search space, Arcs in CCH/Triangles/Treewidth, running times order/customization/queries
print(  r"""\begin{tabular}{ *{4}{c} *{4}{r} *{3}{r} *{3}{r} }
            \toprule
            & & & & \multicolumn{4}{c}{Search Space} & CCH & & Up. & \multicolumn{3}{c}{Running times} \\
            \cmidrule(lr){5-8} \cmidrule(lr){12-14}
            \multicolumn{4}{c}{Configuration} & \multicolumn{2}{c}{Nodes} & \multicolumn{2}{c}{Arcs {[}$\cdot10^{3}${]}} & Arcs &  \#Tri. & Tw. & Order & Cust. & Query \\
            \cmidrule(lr){1-4} \cmidrule(lr){5-6} \cmidrule(lr){7-8}
            $\alpha$ & $\delta$ & $\gamma_a$ & $\gamma_o$ & Avg. & Max.& Avg. & Max. & {[}$\cdot10^{6}${]} & {[}$\cdot10^{6}${]} & Bd. & {[}s{]} & {[}ms{]} & {[}$\mu$s{]}\\
            \midrule
        """, file=f)
highlight_min = True
heatmap = True
columns = ['initial_assimilated_fraction', 'bulk_step_fraction', 'bulk_assimilation_threshold', 'bulk_assimilation_order_threshold', 
            'average_elimination_tree_depth', 'elimination_tree_height', 'average_arcs_in_search_space', 'maximum_arcs_in_search_space', 
            'super_graph_upward_arc_count', 'number_of_triangles_in_super_graph', 'upper_tree_width_bound', 
            'order_running_time', 'median_customization_time', 'avg_query_time']

df["super_graph_upward_arc_count"] = df["super_graph_upward_arc_count"].map(lambda x : x/100000)
df["number_of_triangles_in_super_graph"] = df["number_of_triangles_in_super_graph"].map(lambda x : x/100000)
df["average_arcs_in_search_space"] = df["average_arcs_in_search_space"].map(lambda x : x/1000)
df["maximum_arcs_in_search_space"] = df["maximum_arcs_in_search_space"].map(lambda x : x/1000)

for column in columns[4:]:
    if df[column].dtype == 'float':
        df[column] = df[column].map(lambda x : round(x,1))

for row in range(0, len(df.index)):
    for column in columns:
        if column != columns[0]:
            print(' & ', end='', file=f)
        value = df.loc[row, column]

        formatted_value = value
        if column == "order_running_time" or column == "median_customization_time":
            formatted_value = int(round(value,0))
        if is_number(str(value)):
            comp = re.compile('([0-9]{3}(?=[0-9]))')
            formatted_value = re.sub( comp, '\\g<0>,\\\\', str(formatted_value)[::-1] )[::-1]
        
        if heatmap and column in columns[4:]:
            print(r"\cellcolor{", get_color(value, df[column]), r"}", sep='', end='', file=f)
        if highlight_min and value == df[column].min() and column in columns[4:]:
            print(r"\bfseries{", formatted_value, r"}", sep='', end='', file=f)
        else:
            print(formatted_value, end='', file=f)
    print(r"\\", file=f)

print(r"\bottomrule", file=f)
print(r"\end{tabular}", file=f)
