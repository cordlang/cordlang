#ifndef CORDLANG_CLI_INTERNAL_H
#define CORDLANG_CLI_INTERNAL_H
void cli_print_usage(void);
char *cli_resolve_check_entry(const char *path);
int cli_cmd_check(int argc, char **argv);
int cli_cmd_analyze(int argc, char **argv);
int cli_cmd_compile(int argc, char **argv);
int cli_cmd_fmt(int argc, char **argv);
int cli_cmd_ai(int argc, char **argv);
int cli_cmd_build(int argc, char **argv);
#endif
