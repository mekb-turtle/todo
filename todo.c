#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <assert.h>
#include <limits.h>
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define strerr strerror(errno)
#define TODO_FILE "todolist"
#define DONE_CHAR '#'
#define TODO_CHAR '.'
#define DOING_CHAR '/'
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
					assert(l);
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
			assert(l[lines-1]);
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
			assert(f);
		return NULL;
	}
	return read_lines(f);
}
bool write_lines(char **lines, FILE *fp) {
	if (!lines) return false;
	for (; *lines; ++lines) {
		fputs((char*)*lines, fp);
		fputc('\n', fp);
	}
	return true;
}
bool write_todo(char **lines, char *file_name) {
	FILE *f = fopen(file_name, "w");
	assert(f);
	return write_lines(lines, f);
}
char *arg_add_list[] = { "add", "new", "a", NULL };
char *arg_remove_list[] = { "remove", "delete", "r", "del", NULL };
char *arg_todo_list[] = { "todo", "t", NULL };
char *arg_done_list[] = { "done", "finish", "finished", "d", NULL };
char *arg_doing_list[] = { "doing", "dd", "wip", NULL };
char *arg_edit_list[] = { "edit", "change", "e", NULL };
char *arg_list_list[] = { "list", "l", "ls", NULL };
char *arg_clear_list[] = { "clear", "clr", "cls", "c", NULL };
char *arg_html_list[] = { "html", "htm", NULL };
bool argmatch(char *arg, char **list) {
	for (; *list; ++list) { if (strcmp(arg, *list) == 0) return true; }
	return false;
}
void printarglist(char **list) {
	eprintf("	  ");
	for (int a = 0; *list; a = 1, ++list) { if (a) eprintf("/"); eprintf("%s", *list); }
	eprintf("\n");
}
int usage(char *argv0) {
	eprintf("Usage: %s\n", argv0);
	eprintf("	add [text]          : adds to list\n");
	printarglist(arg_add_list);
	eprintf("	remove [index]      : removes from list at index\n");
	printarglist(arg_remove_list);
	eprintf("	todo [index]        : marks entry at index as to-do\n");
	printarglist(arg_todo_list);
	eprintf("	done [index]        : marks entry at index as done\n");
	printarglist(arg_done_list);
	eprintf("	doing [index]       : marks entry at index as doing\n");
	printarglist(arg_doing_list);
	eprintf("	edit [index] [text] : edits text at index\n");
	printarglist(arg_edit_list);
	eprintf("	list                : list all items\n");
	printarglist(arg_list_list);
	eprintf("	clear confirm       : clear completely\n");
	printarglist(arg_clear_list);
	eprintf("	html [file] [title] : save list to a .html file to view in a browser\n");
	printarglist(arg_html_list);
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
	for (size_t i = 0; *b; ++i, ++b) { if (i > 16) return LONG_MAX; if (*b < '0' || *b > '9') return LONG_MAX; }
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
	return c == TODO_CHAR || c == DONE_CHAR || c == DOING_CHAR;
}
char *prefix_color(bool is_color) {
	if (is_color) return "\x1b[0;38;5;7m";
	return "";
}
char *remove_prefix(char *line) {
	return line + (has_prefix(line[0]) ? 1 : 0);
}
char *prefix(bool is_color, char c) {
	char *p = malloc(64);
	if (!p) return NULL;
	if (is_color) {
		snprintf(p, 64, "\x1b[0;38;5;7m[\x1b[38;5;%im%s\x1b[38;5;7m]\x1b[0m ", c == DONE_CHAR ? 9 : c == DOING_CHAR ? 11 : 10, c == DONE_CHAR ? "DONE" : c == DOING_CHAR ? "DOING" : "TODO");
	} else {
		snprintf(p, 16, "[%s] ", c == DONE_CHAR ? "DONE" : c == DOING_CHAR ? "DOING" : "TODO");
	}
	return p;
}
bool list_lines(bool is_color, char **lines, size_t lines_len) {
	if (!lines || !*lines || lines_len <= 0) {
		printf("There is nothing on to-do list\n");
		return 0;
	}
	printf("To-do list:\n");
	for (size_t i = 0; i < lines_len; ++i) {
		printf("%s% 4li. %s%s\x1b[0m\n",
			prefix_color(is_color), i+1,
			prefix(is_color, lines[i][0]),
			remove_prefix(lines[i]));
	}
	return 1;
}
void print_escaped(FILE *out, char *text) {
	for (; *text; ++text) {
		if (*text == '<') fputs("&lt;", out);
		else if (*text == '>') fputs("&gt;", out);
		else if (*text == '&') fputs("&amp;", out);
		else putc(*text, out);
	}
}
void print_html(FILE *out, char **lines, char *title) {
	fprintf(out, "\
<html>\n\
	<head>\n");
	if (title) {
		fprintf(out, "\t\t<title>");
		print_escaped(out, title);
		fprintf(out, "\t\t</title>\n");
	}
	fprintf(out, "\
		<style>\n\
			.gray  { color: #555555; }\n\
			.todo  { color: #22CC22; }\n\
			.done  { color: #CC2222; }\n\
			.doing { color: #DDDD22; }\n\
		</style>\n\
	</head>\n\
	<body>\n");
	if (title) {
		fprintf(out, "\t\t<h2>");
		print_escaped(out, title);
		fprintf(out, "\t\t</h2>\n");
	}
	fprintf(out, "\
		<ol class=\"todolist\">\n");
	for (; *lines; ++lines) {
		char c = (*lines)[0];
		fprintf(out, "\t\t\t<li class=\"todoitem\">\n");
		fprintf(out, "\t\t\t\t<span class=\"gray\">[</span>\
<span class=\"status %s\">%s</span>\
<span class=\"gray\">]</span> \n\t\t\t\t",
			c == DONE_CHAR ? "done" : c == DOING_CHAR ? "doing" : "todo",
			c == DONE_CHAR ? "DONE" : c == DOING_CHAR ? "DOING" : "TODO");
		print_escaped(out, remove_prefix(*lines));
		fprintf(out, "\n\t\t\t</li>\n");
	}
	fprintf(out, "\
		</ol>\n\
	</body>\n\
</html>\n");
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
			assert(file_name);
			sprintf(file_name, "%s/%s", config_dir, TODO_FILE);
		} else {
			char *home = get_home();
			assert(home);
   			file_name = malloc(strlen(TODO_FILE) + strlen(home) + 12);
			assert(file_name);
			sprintf(file_name, "%s/.config/%s", home, TODO_FILE);
		}
	}
	if (argc < 2) {
		printf("To-do file location: %s\n", file_name);
		usage(argv[0]);
	}
	char **lines = NULL;
	lines = read_todo(file_name);
	bool is_color = isatty(STDOUT_FILENO);
	size_t lines_len = 0;
	if (lines) for (char **t = lines; *t; ++t, ++lines_len) {}
	if (argc < 2) {
		list_lines(is_color, lines, lines_len);
	} else if (argc == 3 && argmatch(argv[1], arg_add_list)) {
		char *u = todo_string(argv[2]);
		if (!u) return 1;
		if (!lines) {
			lines = calloc(2, sizeof(char*));
			assert(lines);
		}
		lines[lines_len] = u;
		lines[lines_len+1] = NULL;
		printf("Added entry: %s%li. %s%s\n", prefix_color(is_color), lines_len+1, prefix(is_color, u[0]), remove_prefix(u));
		list_lines(is_color, lines, lines_len+1);
		write_todo(lines, file_name);
	} else if (argc == 3 && argmatch(argv[1], arg_todo_list)) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Argument must be an integer from 1 to %li\n", lines_len);
			return 1;
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
	} else if (argc == 3 && argmatch(argv[1], arg_done_list)) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Argument must be an integer from 1 to %li\n", lines_len);
			return 1;
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
	} else if (argc == 3 && argmatch(argv[1], arg_doing_list)) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Argument must be an integer from 1 to %li\n", lines_len);
			return 1;
		}
		bool w = 0;
		if (!has_prefix(lines[i-1][0])) {
			lines[i-1] = todo_string(lines[i-1]);
			w = 1;
		}
		if (lines[i-1][0] == DOING_CHAR) {
			eprintf("Entry is already marked as doing: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, DOING_CHAR), remove_prefix(lines[i-1]));
		} else {
			lines[i-1][0] = DOING_CHAR;
			printf("Marked entry as doing: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, DOING_CHAR), remove_prefix(lines[i-1]));
			w = 1;
		}
		list_lines(is_color, lines, lines_len);
		if (w) write_todo(lines, file_name);
	} else if (argc == 3 && argmatch(argv[1], arg_remove_list)) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Argument must be an integer from 1 to %li\n", lines_len);
			return 1;
		}
		size_t d = lines_len-(i-1);
		printf("Removed entry: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, lines[i-1][0]), remove_prefix(lines[i-1]));
		free(lines[i-1]);
		memmove(&lines[i-1], &lines[i], d*sizeof(char*));
		--lines_len;
		list_lines(is_color, lines, lines_len);
		write_todo(lines, file_name);
	} else if (argc == 4 && argmatch(argv[1], arg_edit_list)) {
		long i = parselong(argv[2]);
		if (i <= 0 || i > lines_len) {
			eprintf("Argument must be an integer from 1 to %li\n", lines_len);
			return 1;
		}
		char *u = todo_string(argv[3]);
		if (!u) return 1;
		if (lines[i-1][0] == DONE_CHAR || lines[i-1][0] == DOING_CHAR) u[0] = lines[i-1][0];
		lines[i-1] = u;
		printf("Edited entry: %s%li. %s%s\n", prefix_color(is_color), i, prefix(is_color, lines[i-1][0]), remove_prefix(u));
		list_lines(is_color, lines, lines_len);
		write_todo(lines, file_name);
	} else if (argc == 2 && argmatch(argv[1], arg_list_list)) {
		return list_lines(is_color, lines, lines_len) ? 0 : 1;
	} else if ((argc == 3 || argc == 2) && argmatch(argv[1], arg_clear_list)) {
		if (!lines || !*lines) {
			eprintf("There already is nothing on to-do list\n"); return 1;
		}
		if (argc == 3 && strcmp(argv[2], "confirm") == 0) {
			list_lines(is_color, lines, lines_len);
			write_todo(NULL, file_name);
			printf("Cleared to-do list\n");
		} else {
			eprintf("Use 'clear confirm' to confirm\n"); return 1;
		}
	} else if ((argc == 3 || argc == 4) && argmatch(argv[1], arg_html_list)) {
		FILE *out;
		if (argv[2][0] == '-' && argv[2][1] == '\0') {
			out = stdout;
		} else {
			out = fopen(argv[2], "w");
		}
		assert(out);
		print_html(out, lines, argc >= 4 ? argv[3] : NULL);
	} else {
		INVALID;
	}
	return 0;
}
