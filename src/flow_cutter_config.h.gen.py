#!/usr/bin/env python2

import sys

enum_members = {}
enum_var_name = {}

var_names = []
var_type = {}
var_requirement = {}
var_default_value = {}

for line in sys.stdin:
	line = line.split()
	if line[0] == "var":
		var_names.append(line[2])
		var_type[line[2]] = line[1]
		var_requirement[line[2]] = line[3]
		var_default_value[line[2]] = line[4]
	else:
		enum_members[line[0]] = line[2:]
		enum_var_name[line[0]] = line[1]


print """
#ifndef FLOW_CUTTER_CONFIG_H
#define FLOW_CUTTER_CONFIG_H
#include <string>
#include <stdexcept>
#include <iomanip>
#include <sstream>

namespace flow_cutter{
	struct Config{
""",

for x in var_names:
	print "\t\t"+var_type[x]+" "+x+";"

print "\n",

for x in enum_members.keys():
	print "\t\tenum class "+x+"{\n\t\t\t" + ",\n\t\t\t".join(enum_members[x])+"\n\t\t};"
	print "\t\t"+x+" "+enum_var_name[x]+";\n" 


print("\t\tConfig():\n\t\t\t" +
	",\n\t\t\t".join([x + "("+str(var_default_value[x])+")" for x in var_names]) + ",\n\t\t\t" +
	",\n\t\t\t".join([enum_var_name[x]+"("+x+"::"+enum_members[x][0]+")" for x in enum_members.keys()]) + "{}\n")

print "\t\tvoid set(const std::string&var, const std::string&val){\n\t\t\t",

#print "int val_id; try{val_id = std::stoi(val);}catch(...){val_id = -1;};\n\t\t\t",
print "int val_id = -1;\n\t\t\t",

for x in enum_members.keys():
	print "if(var == \""+x+"\" || var == \""+enum_var_name[x]+"\"){\n\t\t\t\t",
	for y in enum_members[x]:
		print "if(val == \""+y+"\" || val_id == static_cast<int>("+x+"::"+y+")) \n\t\t\t\t\t" + enum_var_name[x] + " = "+x+"::"+y+";\n\t\t\t\telse",
	print "throw std::runtime_error(\"Unknown config value \"+val+\" for variable "+x+"; valid are "+", ".join(enum_members[x])+"\");\n",

	print "\t\t\t}else",

for x in var_names:
	print "if(var == \""+x+"\"){\n\t\t\t\t",
	if var_requirement[x] != "true":
		if var_type[x] == "int":
			print "int x = std::stoi(val);\n\t\t\t\t",
		elif var_type[x] == "float":
			print "float x = std::stof(val);\n\t\t\t\t",
		elif var_type[x] == "double":
			print "double x = std::stof(val);\n\t\t\t\t",
		else:
			raise Exception("unsupported type "+var_type[x])
		print "if(!("+var_requirement[x]+"))\n\t\t\t\t\tthrow std::runtime_error(\"Value for \\\""+x+"\\\" must fullfill \\\""+var_requirement[x]+"\\\"\");\n\t\t\t\t",
		print x+" = x;",
	else:
		if var_type[x] == "int":
			print x+" = std::stoi(val);",
		if var_type[x] == "float":
			print x+" = std::stof(val);",
	print "\n\t\t\t}else",

print "throw std::runtime_error(\"Unknown config variable \"+var+\"; valid are "+", ".join(enum_members.keys())+", "+", ".join(var_names)+"\");\n",
print "\t\t}\n",

print "\t\tstd::string get(const std::string&var)const{\n\t\t\t",
for x in enum_members.keys():
	print "if(var == \""+x+"\" || var == \""+enum_var_name[x]+"\"){\n\t\t\t\t",

	for y in enum_members[x]:
		print "if("+enum_var_name[x]+" == "+x+"::"+y+") return \""+y+"\";\n\t\t\t\telse",
	print "{assert(false); return \"\";}\n",

	print "\t\t\t}else",

for x in var_names:
	print "if(var == \""+x+"\"){\n\t\t\t\t",
	print "return std::to_string("+x+");\n",
	print "\t\t\t}else",

print "throw std::runtime_error(\"Unknown config variable \"+var+\"; valid are "+",".join(enum_members.keys())+", "+", ".join(var_names)+"\");\n",
print "\t\t}\n",

print("\t\tstd::string get_config()const{\n\t\t\tstd::ostringstream out;\n\t\t\tout\n\t\t\t\t" + 
	"\n\t\t\t\t".join(["<< std::setw(30) << \""+x+"\" << \" : \" << get(\""+x+"\") << '\\n'" for x in enum_members.keys()]) + "\n\t\t\t\t" + 
	"\n\t\t\t\t".join(["<< std::setw(30) << \""+x+"\" << \" : \" << get(\""+x+"\") << '\\n'" for x in var_names]) +
	";\n\t\t\treturn out.str();\n\t\t}"
)

print """
	};
}
#endif
""",
