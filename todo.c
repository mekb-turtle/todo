#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define strerr strerror(errno)
#define TODO_FILE ".todolist"
#define BLOCK 128
char **read_lines(FILE *in_f) {
	char **l = NULL;
	size_t len = 0;
	size_t real_len = 0;
	size_t lines = 0;
	size_t real_lines = 0;
	bool start = 1;
	bool linestart = 1;
	while (1) {
#define line l[lines-1]
		int c = fgetc(in_f);
		if (c == '\r' || c == '\0' || c == '\v') continue;
		if (c < 0 || c == '\n' || start) {
			if (start || (len > 0 && line)) {
				start = 0;
				if (lines > 0 && len > 0 && line) {
					line[len] = '\0';
				}
				++lines;
				linestart = 1;
				len = 0;
				real_len = 0;
				if (lines >= real_lines) {
					real_lines += BLOCK;
					l = realloc(l, (real_lines+2) * sizeof(char*));
				}
			}
			if (c < 0) break;
			if (c == '\n') continue;
		}
		++len;
		if (len >= real_len) {
			real_len += BLOCK;
			if (linestart) {
				line = NULL;
			}
			linestart = 0;
			line = realloc(line, real_len+1);
			if (!line) return NULL;
		}
		if (c < 0) break;
		line[len-1] = c;
	}
	l[lines-1] = NULL;
	return l;
	
}
char **read_todo(char *file_name) {
	FILE *f = fopen(file_name, "r");
	if (!f) {
		eprintf("fopen: %s: %s\n", file_name, strerr);
		return NULL;
	}
	return read_lines(f);
}
enum bool_error {
	FALSE, TRUE, ERROR
};
enum bool_error write_lines(char **lines, FILE *fp) {
	if (!lines) return FALSE;
	for (; *lines; ++lines) {
		puts((char*)*lines);
		putc('\n', fp);
	}
	return TRUE;
}
enum bool_error write_todo(char **lines, char *file_name) {
	FILE *f = fopen(file_name, "w");
	if (!f) {
		eprintf("fopen: %s: %s\n", file_name, strerr);
		return ERROR;
	}
	return write_lines(lines, f);
}
enum bool_error exists(char *file_name) {
	struct stat *s = malloc(sizeof(struct stat));
	if (!s) {
		eprintf("malloc: %s\n", strerr);
		return ERROR;
	}
	int l = stat(file_name, s);
	free(s);
	if (l == 0) return TRUE;
	if (errno == ENOENT) return FALSE;
	eprintf("stat: %s: %s\n", file_name, strerr);
	return ERROR;
}
int usage(char *argv0) {
	eprintf("\
Usage: %s\n\
	add [text]          : adds to list\n\
	remove [index]      : removes from list at index\n\
	todo [index]        : marks at index as todo\n\
	done [index]        : marks at index as done\n\
	edit [index] [text] : edits text at index\n\
	list                : list all items\n\
	clear               : clear completely\n\
", argv0);
	return 2;
};
char *get_home() {
	uid_t my_uid = getuid();
	struct passwd *my_pwd = getpwuid(my_uid);
	if (!my_pwd) return NULL;
	return my_pwd->pw_dir;
}
int parseint(char *a) {
	char *b = a;
	for (size_t i = 0; *b; ++i) { if (i > 16) return 5; if (*b < '0' || *b > '9') return 0; ++b; }
	return atoi(a);
}
char *todo_string(char *a) {
	if (!a) return NULL;
	size_t s = strlen(a);
	char *b = malloc(s + 2);
	if (!b) return NULL;
	b[0] = '.';
	memcpy(b+1, a, s+1);
	return b;
}
int main(int argc, char *argv[]) {
#define INVALID return usage(argv[0])
	if (argc < 2 || argc > 3) {
		INVALID;
	}
	char *home = get_home();
	if (!home) {
		eprintf("Failed to get home directory\n");
		return 1;
	}
	char *file_name = malloc(strlen(TODO_FILE) + strlen(home) + 8);
	if (!file_name) {
		eprintf("malloc: %s\n", strerr);
		return 1;
	}
	sprintf(file_name, "%s/%s", home, TODO_FILE);
	enum bool_error b = exists(file_name);
	char **lines = NULL;
	if (b == ERROR) return 1;
	else if (b == TRUE) {
		lines = read_todo(file_name);
	}
	bool isatty_stdout = isatty(STDOUT_FILENO);
	bool is_add    = strcmp(argv[1], "add")    == 0 && argc == 3;
	bool is_remove = strcmp(argv[1], "remove") == 0 && argc == 3;
	bool is_todo   = strcmp(argv[1], "todo")   == 0 && argc == 3;
	bool is_done   = strcmp(argv[1], "done")   == 0 && argc == 3;
	bool is_edit   = strcmp(argv[1], "edit")   == 0 && argc == 4;
	bool is_list   = strcmp(argv[1], "list")   == 0 && argc == 2;
	bool is_clear  = strcmp(argv[1], "clear")  == 0 && argc == 2;
	size_t lines_len = 0;
	for (char **t = lines; *t; ++t, ++lines_len);
	printf("AA %s\n",lines[lines_len]);
	if (is_add) {
		char *u = todo_string(argv[3]);
		if (!u) return 1;
		lines[lines_len] = u;
		write_todo(lines, file_name);
	} else if (is_remove) {
		int i = parseint(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Invalid index");
		}
	} else if (is_edit) {
		int i = parseint(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Invalid index");
		}
		char *u = todo_string(argv[3]);
		if (!u) return 1;
		lines[i-1] = u;
		printf("Edited entry at %i to\n%s\n", i, argv[3]);
		write_todo(lines, file_name);
	} else if (is_list) {
		if (!lines || !*lines) {
			printf("Nothing on todo list\n");
			return 1;
		} else {
			for (size_t i = 1; i <= lines_len; ++i) {
				bool is_done = *(lines[i]) == '#';
				if (isatty_stdout) {
					printf("\x1b[0;38;5;7m% 4li. [\x1b[38;5;%im%s\x1b[38;5;7m]\x1b[0m %s\x1b[0m\n",
						i, is_done ? 11 : 10, is_done ? "DONE" : "TODO",
						lines[i] + (lines[i][0] == '.' || lines[i][0] == '#' ? 1 : 0));
				} else {
					printf("%li. \t[%s] %s\n", i, is_done ? "DONE" : "TODO", *lines);
				}
			}
		}
	} else if (is_clear) {
		write_todo(NULL, file_name);
	} else {
		INVALID;
	}
	return 0;
}
