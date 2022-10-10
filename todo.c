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
#define TODO_FILE "todolist"
#define DONE_CHAR '#'
#define TODO_CHAR '.'
#define BLOCK_LINES 8
#define BLOCK 128
char **read_lines(FILE *in_f) {
	errno = 1;
	char **l = NULL;
	size_t len = 0;
	size_t real_len = 0;
	size_t lines = 0;
	size_t real_lines = 0;
	bool start = 1;
	bool linestart = 1;
	while (1) {
		int c = fgetc(in_f);
		if (c == '\r' || c == '\0' || c == '\v') continue;
		if (c < 0 || c == '\n' || start) {
			if (start || (len > 0 && l && l[lines-1])) {
				start = 0;
				if (lines > 0 && len > 0 && l && l[lines-1]) {
					l[lines-1][len] = '\0';
				}
				++lines;
				linestart = 1;
				len = 0;
				real_len = 0;
				if (lines >= real_lines) {
					real_lines += BLOCK_LINES;
					l = realloc(l, (real_lines+4) * sizeof(char*));
					if (!l) {
						eprintf("realloc: %s\n", strerr);
						return NULL;
					}
				}
			}
			if (c < 0) break;
			if (c == '\n') continue;
		}
		++len;
		if (len >= real_len) {
			real_len += BLOCK;
			if (linestart) {
				l[lines-1] = NULL;
			}
			linestart = 0;
			l[lines-1] = realloc(l[lines-1], real_len+1);
			if (!l[lines-1]) {
				eprintf("realloc: %s\n", strerr);
				return NULL;
			}
		}
		if (c < 0) break;
		l[lines-1][len-1] = c;
	}
	l[lines-1] = NULL;
	return l;
	
}
char **read_todo(char *file_name) {
	FILE *f = fopen(file_name, "r");
	if (!f) {
		if (errno != ENOENT)
			eprintf("fopen: %s: %s\nFailed to read to-do list\n", file_name, strerr);
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
		fputs((char*)*lines, fp);
		fputc('\n', fp);
	}
	return TRUE;
}
enum bool_error write_todo(char **lines, char *file_name) {
	FILE *f = fopen(file_name, "w");
	if (!f) {
		eprintf("fopen: %s: %s\nFailed to write to-do list\n", file_name, strerr);
		return ERROR;
	}
	return write_lines(lines, f);
}
int usage(char *argv0) {
	eprintf("\
Usage: %s\n\
	add [text]          : adds to list\n\
	remove [index]      : removes from list at index\n\
	todo [index]        : marks entry at index as to-do\n\
	done [index]        : marks entry at index as done\n\
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
long parselong(char *a) {
	char *b = a;
	for (size_t i = 0; *b; ++i) { if (i > 16) return 5; if (*b < '0' || *b > '9') return 0; ++b; }
	return atol(a);
}
char *todo_string(char *a) {
	if (!a) return NULL;
	size_t s = strlen(a);
	char *b = malloc(s + 2);
	if (!b) {
		eprintf("malloc: %s\n", strerr);
		return NULL;
	}
	b[0] = TODO_CHAR;
	memcpy(b+1, a, s+1);
	return b;
}
bool has_prefix(char c) {
	return c == TODO_CHAR || c == DONE_CHAR;
}
char *prefix_color(bool is_color) {
	if (is_color) return "\x1b[0;38;5;7m";
	return "";
}
char *remove_prefix(char *line) {
	return line + (has_prefix(line[0]) ? 1 : 0);
}
char *prefix(bool is_color, char c) {
	bool is_done = c == DONE_CHAR;
	char *p = malloc(64);
	if (!p) return NULL;
	if (is_color) {
		snprintf(p, 64, "\x1b[0;38;5;7m[\x1b[38;5;%im%s\x1b[38;5;7m]\x1b[0m ", is_done ? 9 : 10, is_done ? "DONE" : "TODO");
	} else {
		snprintf(p, 16, "[%s] ", is_done ? "DONE" : "TODO");
	}
	return p;
}
bool list_lines(bool is_color, char **lines, size_t lines_len) {
	if (!lines || !*lines || lines_len <= 0) {
		printf("There is nothing on to-do list\n");
		return 0;
	} else {
		printf("To-do list:\n");
		for (size_t i = 0; i < lines_len; ++i) {
			printf("%s% 4li. %s%s\x1b[0m\n",
				prefix_color(is_color), i+1,
				prefix(is_color, lines[i][0]),
				remove_prefix(lines[i]));
		}
		return 1;
	}
}
int main(int argc, char *argv[]) {
#define INVALID return usage(argv[0])
	char *file_name;
	char *file_name_env = getenv("TODO_LOCATION");
	if (file_name_env) {
		file_name = file_name_env;
	} else {
		char *config_dir = getenv("XDG_CONFIG_HOME");
		if (config_dir) {
   			file_name = malloc(strlen(TODO_FILE) + strlen(config_dir) + 4);
			if (!file_name) {
				eprintf("malloc: %s\n", strerr);
				return 1;
			}
			sprintf(file_name, "%s/%s", config_dir, TODO_FILE);
		} else {
			char *home = get_home();
			if (!home) {
				eprintf("Failed to get home directory\n");
				return 1;
			}
   			file_name = malloc(strlen(TODO_FILE) + strlen(home) + 12);
			if (!file_name) {
				eprintf("malloc: %s\n", strerr);
				return 1;
			}
			sprintf(file_name, "%s/.config/%s", home, TODO_FILE);
		}
	}
	if (argc < 2) {
		printf("To-do file location: %s\n", file_name);
		usage(argv[0]);
	}
	char **lines = NULL;
	lines = read_todo(file_name);
	if (!lines && errno != ENOENT) return errno||1;
	bool is_color = isatty(STDOUT_FILENO);
	size_t lines_len = 0;
	if (lines) for (char **t = lines; *t; ++t, ++lines_len);
	if (argc < 2) {
		list_lines(is_color, lines, lines_len);
	} else if (argc == 3 && strcmp(argv[1], "add") == 0) {
		char *u = todo_string(argv[2]);
		if (!u) return 1;
		if (!lines) {
			lines = malloc(sizeof(char*) * 2);
			if (!lines) {
				eprintf("malloc: %s\n", strerr);
				return errno;
			}
		}
		lines[lines_len] = u;
		lines[lines_len+1] = NULL;
		printf("Added entry: %s%li. %s%s\n", prefix_color(is_color), lines_len+1, prefix(is_color, u[0]), remove_prefix(u));
		list_lines(is_color, lines, lines_len+1);
		write_todo(lines, file_name);
	} else if (argc == 3 && strcmp(argv[1], "todo") == 0) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Invalid index\n"); return 1;
		}
		bool w = 0;
		if (!has_prefix(lines[i-1][0])) {
			lines[i-1] = todo_string(lines[i-1]);
			w = 1;
		}
		if (lines[i-1][0] == TODO_CHAR) {
			eprintf("Entry is already marked as to-do: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, TODO_CHAR), remove_prefix(lines[i-1]));
		} else {
			lines[i-1][0] = TODO_CHAR;
			printf("Marked entry as to-do: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, TODO_CHAR), remove_prefix(lines[i-1]));
			w = 1;
		}
		list_lines(is_color, lines, lines_len);
		if (w) write_todo(lines, file_name);
	} else if (argc == 3 && strcmp(argv[1], "done") == 0) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Invalid index\n"); return 1;
		}
		bool w = 0;
		if (!has_prefix(lines[i-1][0])) {
			lines[i-1] = todo_string(lines[i-1]);
			w = 1;
		}
		if (lines[i-1][0] == DONE_CHAR) {
			eprintf("Entry is already marked as done: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, DONE_CHAR), remove_prefix(lines[i-1]));
		} else {
			lines[i-1][0] = DONE_CHAR;
			printf("Marked entry as done: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, DONE_CHAR), remove_prefix(lines[i-1]));
			w = 1;
		}
		list_lines(is_color, lines, lines_len);
		if (w) write_todo(lines, file_name);
	} else if (argc == 3 && strcmp(argv[1], "remove") == 0) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Invalid index\n"); return 1;
		}
		size_t d = lines_len-(i-1);
		printf("Removed entry: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, lines[i-1][0]), remove_prefix(lines[i-1]));
		free(lines[i-1]);
		memmove(&lines[i-1], &lines[i], d*sizeof(char*));
		--lines_len;
		list_lines(is_color, lines, lines_len);
		write_todo(lines, file_name);
	} else if (argc == 4 && strcmp(argv[1], "edit") == 0) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Invalid index\n"); return 1;
		}
		char *u = todo_string(argv[3]);
		if (!u) return 1;
		if (lines[i-1][0] == DONE_CHAR) u[0] = DONE_CHAR;
		lines[i-1] = u;
		printf("Edited entry: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, lines[i-1][0]), remove_prefix(u));
		list_lines(is_color, lines, lines_len);
		write_todo(lines, file_name);
	} else if (argc == 2 && strcmp(argv[1], "list") == 0) {
		return list_lines(is_color, lines, lines_len) ? 0 : 1;
	} else if (argc == 2 && strcmp(argv[1], "clear") == 0) {
		if (!lines || !*lines) {
			printf("There already is nothing on to-do list\n"); return 1;
		} else {
			list_lines(is_color, lines, lines_len);
			write_todo(NULL, file_name);
			printf("Cleared to-do list\n");
		}
	} else {
		INVALID;
	}
	return 0;
}
