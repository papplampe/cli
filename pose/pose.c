#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>

#define ARR_LEN(x) \
	(sizeof(x)/sizeof(x[0]))

typedef enum {
	ArgProg,
	ArgBool,
	ArgValue
} ArgType;

typedef struct {
	ArgType type;
	char *key;
	union {
		struct {
			char *value;
			int len;
		};
		int isset;
	};
} Arg;

typedef struct {
	HANDLE stdout;
} Console;

typedef struct {
	Console *c;
	char *search;
	char *contains;
	int stoponfirst;
	int absolutepath;
	int searchignorecase;
	int containsignorecase;
} Finder;

static char *bstrcpy(char *dst, const char *src)
{
	while(*src) {
		*dst++ = *src++;		
	}
	*dst = 0;
	return dst;
}

static char *bstrcat(char *dst, const char *src)
{
	while(*dst) {
		dst++;
	}
	while(*src) {
		*dst++ = *src++;		
	}
	*dst = 0;
	return dst;
}

static int endswith(const char *s, char c)
{
	while(*s) {
		s++;
	}
	s--;
	return *s == c;
}

static void args_make(Arg *arg, ArgType type, char *key)
{
	arg->type = type;
	arg->key = key;
	if(type == ArgBool) {
		arg->isset = 0;
	} else if(type == ArgValue || type == ArgProg) {
		arg->value = NULL;
		arg->len = 0;
	}
}

static void args_get(Arg **args, int count, const char *cmdline)
{
	static char cmd[8*1024];
	int i, o, n, ignorespace;
	Arg *key;

	lstrcpy(cmd, cmdline);

	/* parse program name */
	o = 0;
	while(cmd[o] && cmd[o] != ' ') {
		o++;
	}
	cmd[o] = 0;
	for(i=0; i<count; i++) {
		if(args[i]->type == ArgProg) {
			args[i]->value = &cmd[0];
			args[i]->len = o;
			break;
		}
	}
	o++;
	if(!cmd[o]) {
		return;
	}
	while(cmd[o] == ' ' || cmd[o] == '\t') {
		o++;
	}

	/* begin key value pairs */
	key = NULL;
	n = o;
	ignorespace = 0;
	while(cmd[o]) {
		if(cmd[o] == '"') {
			ignorespace = !ignorespace;
			o++;
		} else if(cmd[o] == ' ' && !ignorespace) {
			cmd[o] = 0;
			if(key) {
				key->value = &cmd[n];
				key->len = o-n;
				if(key->value[0] == '"') {
					key->value++;
					key->len--;
				}
				if(key->value[key->len-1] == '"') {
					key->value[--key->len] = 0;
				}
				key = NULL;
			} else {
				for(i=0; i<count; i++) {
					if(lstrcmp(args[i]->key, &cmd[n]) == 0) {
						if(args[i]->type == ArgBool) {
							args[i]->isset = 1;
						} else {
							key = args[i];
						}
						break;
					}
				}
			}
			o++;
			if(!cmd[o]) {
				break;
			}
			while(cmd[o] == ' ' || cmd[o] == '\t') {
				o++;
			}
			n = o;
		} else {
			o++;
		}
	}

	/* the last value, if any */
	if(key) {
		key->value = &cmd[n];
		key->len = o-n;
		if(key->value[0] == '"') {
			key->value++;
			key->len--;
		}
		if(key->value[key->len-1] == '"') {
			key->value[--key->len] = 0;
		}
	} else {
		for(i=0; i<count; i++) {
			if(lstrcmp(args[i]->key, &cmd[n]) == 0) {
				if(args[i]->type == ArgBool) {
					args[i]->isset = 1;
				}
				break;
			}
		}
	}
}

static void console_init(Console *c)
{
	AllocConsole();
	c->stdout = GetStdHandle(STD_OUTPUT_HANDLE);
}

static void console_deinit(Console *c)
{
	FreeConsole();
}

static unsigned long console_write(Console *c, const void *msg, size_t len)
{
	unsigned long n;
	WriteConsoleA(c->stdout, msg, len, &n, NULL);
	return n;
}

static unsigned long console_put(Console *c, const char b)
{
	char msg[1] = {b};	
	return console_write(c, msg, sizeof(msg));
}

static unsigned long console_println(Console *c, const void *msg)
{
	unsigned long n;
	n = console_write(c, msg, lstrlen(msg));
	return n + console_put(c, '\n');
}

static void finder_find(Finder *f, const char *basedir)
{
	char path[MAX_PATH] = {0};
	WIN32_FIND_DATA fd;
	HANDLE hfind = NULL;
	
	char *pathend = bstrcpy(path, basedir);
	if(!endswith(path, '\\')) {
		bstrcat(pathend, "\\*");
	} else {
		bstrcat(pathend, "*");
	}
	
	if((hfind = FindFirstFileEx(path, FindExInfoBasic, &fd,
		FindExSearchNameMatch, NULL, 0)) == INVALID_HANDLE_VALUE) {
        return;
    }
	do {
        if(lstrcmp(fd.cFileName, ".")==0 || lstrcmp(fd.cFileName, "..")==0) {
			continue;
		}
		if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			char *pathend = bstrcpy(path, basedir);
			if(!endswith(path, '\\')) {
				pathend = bstrcat(pathend, "\\");
			}
			bstrcat(pathend, fd.cFileName);
			finder_find(f, path);
		} else {
			console_println(f->c, fd.cFileName);
		}
    } while(FindNextFile(hfind, &fd));
    FindClose(hfind);
}

void _main(void) asm("_main");
void _main(void)
{
	Finder f;
	Console c;
	Arg search, base, contains,
		stoponfirst, absolutepath,
		searchignorecase, containsignorecase;
	Arg *args[] = {
		&search, &base,	&contains,
		&stoponfirst, &absolutepath,
		&searchignorecase, &containsignorecase
	};
	
	console_init(&c);
	args_make(&search, ArgValue, "-s"); 
	args_make(&base, ArgValue, "-b");
	args_make(&contains, ArgValue, "-c");
	args_make(&stoponfirst, ArgBool, "-f");
	args_make(&absolutepath, ArgBool, "-a");
	args_make(&searchignorecase, ArgBool, "-is");
	args_make(&containsignorecase, ArgBool, "-ic"); 
	args_get(args, ARR_LEN(args), GetCommandLine());
	
	if(!search.value) {
		console_println(&c, "Usage of pose:\n \
  -s:  name regex\n \
  -b:  base directory\n \
  -c:  contains regex\n \
  -f:  stop at first occurance\n \
  -a:  show absolute paths\n \
  -is: ignore case in name regex\n \
  -ic: ignore case in contains regex");
		goto end;
	}
	
	if(!base.value) {
		char basepath[MAX_PATH] = {0};
		GetCurrentDirectory(sizeof(basepath), basepath);
		base.value = basepath;
	}
	
	f.c = &c;
	f.search = search.value;
	if(contains.value) {
		f.contains = contains.value;
	} else {
		f.contains = NULL;
	}
	f.stoponfirst = stoponfirst.isset;
	f.absolutepath = absolutepath.isset;
	f.searchignorecase = searchignorecase.isset;
	f.containsignorecase = containsignorecase.isset;

	finder_find(&f, base.value);
end:
	console_deinit(&c);
	ExitProcess(0);
}