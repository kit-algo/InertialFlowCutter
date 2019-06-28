#include "fancy_input.h"

#ifdef NO_GPL

#include <string>
#include <iostream>
using namespace std;

void set_autocomplete_command_list(std::vector<std::string>){}
bool get_command_line(std::string&line){
	for(;;){
		cout << " $ " << flush;
		if(getline(cin, line)){
			if(!line.empty())
				return true;
			else
				continue;
		}else
			return false;
	}
}

#else

// link with -lreadline

#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static std::vector<std::string>cmd;

void set_autocomplete_command_list(std::vector<std::string>cmd_list_){
	cmd = std::move(cmd_list_);
}

bool get_command_line(std::string&line){
	const char*x;
	do{
		x = readline(" $ ");
		if(!x)
			return false;
	}while(*x == '\0');
	add_history(x);
	line = x;
	return true;
}


static char** my_completion(const char*, int ,int);
static char* my_generator(const char*,int);
static char * dupstr (const char*);
static void *xmalloc (int);

static char** my_completion( const char * text , int start, int end)
{
    (void)end;
	if (start == 0)
		return rl_completion_matches ((char*)text, &my_generator);
	else{
		return nullptr;
	}
}

static char* my_generator(const char* text, int state)
{
	static int list_index, len;

	if (!state) {
		list_index = -1;
		len = strlen (text);
	}

	for(++list_index; list_index < (int)cmd.size(); ++list_index){
		if (strncmp (cmd[list_index].c_str(), text, len) == 0){
			return (dupstr(cmd[list_index].c_str()));
		}
	}
	return nullptr;
}

char* dupstr (const char* s) {
	char*r = (char*) xmalloc ((strlen (s) + 1));
	strcpy (r, s);
	return r;
}

void* xmalloc (int size)
{
	void *buf;
	buf = malloc (size);
	if (!buf) {
		fprintf (stderr, "Error: Out of memory. Exiting.'n");
		exit (1);
	}
	return buf;
}

static
void initialize_readline ()__attribute__((constructor));

static
void initialize_readline ()
{
	rl_attempted_completion_function = my_completion;
}

#endif

