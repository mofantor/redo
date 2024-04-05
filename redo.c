/*
Redo: A Command-Line Utility for Repeating Executions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAX_COMMAND_ARGS 32
#define MAX_COMMAND_ARG_LEN 20
#define DEFAULT_TIMEOUT 0
#define DEFAULT_REPEAT 1
#define MAX_SUBSTRINGS 32

static int show_help = 0;
static char *PIPE_ARG = "|";

typedef struct
{
    char *command;
    char *args[MAX_COMMAND_ARGS];
    int arg_count;
} Command;

typedef struct
{
    Command **cmds;
    int cmd_count;
    int repeat_count;
    long timeout_secs;
    int until_success;
} ExecCommand;

void print_help()
{
    fprintf(stderr,
            "Usage: redo [OPTIONS] COMMAND [ARGS...]"
            "\n"
            "\n"
            "Redo command-line utility to repeatedly execute a specific command."
            "\n"
            "\n"
            "Options:"
            "\n"
            "  -?, -h          : Show this help message and exit."
            "\n"
            "  -v              : Show program's version information and exit."
            "\n"
            "  -e, --timeout N : Set a timeout for each command execution in seconds."
            "\n"
            "                    Optionally, append 's', 'm', or 'h' for seconds, minutes, or hours."
            "\n"
            "                    Example: -e 10s or -e 5m or -e 1h"
            "\n"
            "  -r, --repeat N  : Repeat the command N times."
            "\n"
            "  -u              : Repeat the command until it succeeds (exit code 0)."
            "\n"
            "\n"
            "Example:"
            "\n"
            "  redo -r 5 -e 10s ping google.com"
            "\n"
            "This will execute the command 'ping google.com' five times,"
            "\n"
            "each with a maximum execution time of 10 seconds."
            "\n");
}

long parse_time_with_units(const char *time_str)
{
    long duration = 0;
    char unit = time_str[strlen(time_str) - 1];
    char *endptr;

    errno = 0;
    long raw_duration = strtol(time_str, &endptr, 10);
    if (errno != 0 || endptr == time_str)
    {
        fprintf(stderr, "[redo]Invalid time format. Expected <number><unit> where unit is s/m/h\n");
        exit(EXIT_FAILURE);
    }
    if (*endptr != unit)
    {
        unit = 's';
    }

    switch (unit)
    {
    case 's':
        duration = raw_duration;
        break;
    case 'm':
        duration = raw_duration * 60;
        break;
    case 'h':
        duration = raw_duration * 3600;
        break;
    default:
        fprintf(stderr, "[redo]Invalid time unit. Only 's', 'm' and 'h' are supported.\n");
        exit(EXIT_FAILURE);
    }

    return duration;
}

int parse_program_arg(char *arg, ExecCommand *ex_cmd, int *cur_want)
{
    if (*cur_want == 0)
    {
        if (strcmp(arg, "-?") == 0 || strcmp(arg, "-h") == 0)
        {
            show_help = 1;
        }
        else if (strcmp(arg, "-u") == 0)
        {
            ex_cmd->until_success = 1;
        }
        else if (strcmp(arg, "-e") == 0 || strcmp(arg, "--timeout") == 0)
        {
            *cur_want = 1;
        }
        else if (strcmp(arg, "-r") == 0 || strcmp(arg, "--repeat") == 0)
        {
            *cur_want = 2;
        }
        else
        {
            return 0;
        }
    }
    else if (*cur_want == 1)
    {
        ex_cmd->timeout_secs = parse_time_with_units(arg);
        *cur_want = 0;
    }
    else if (*cur_want == 2)
    {
        char *endptr;
        errno = 0;
        ex_cmd->repeat_count = strtol(arg, &endptr, 10);
        if (errno != 0 || endptr == arg)
        {
            fprintf(stderr, "[redo]Invalid time format. Expected <number><unit> where unit is s/m/h\n");
            exit(EXIT_FAILURE);
        }
        *cur_want = 0;
    }
    else
    {
        return 0;
    }
    return 1;
}

int parse_cmd_arg(char *arg, ExecCommand *ex_cmd, int *cur_want)
{
    Command *cur_cmd;
    cur_cmd = ex_cmd->cmds[ex_cmd->cmd_count];

    if (cur_cmd->command == NULL)
    {
        cur_cmd->command = arg;
        cur_cmd->args[cur_cmd->arg_count++] = arg;
        return 1;
    }
    else
    {
        if (cur_cmd->arg_count < MAX_COMMAND_ARGS)
        {
            cur_cmd->args[cur_cmd->arg_count++] = arg;
        }
        else
        {
            fprintf(stderr, "[redo]Too many command arguments.\n");
            exit(EXIT_FAILURE);
        }
        return 1;
    }
    return 0;
}

int exist_pipe_char(const char *str)
{
    size_t i;
    size_t len = strlen(str);
    for (i = 0; i < len; i++)
    {
        if (str[i] == '|')
        {
            return 1;
        }
    }
    return 0;
}

int exist_space_char(const char *str)
{
    size_t i;
    size_t len = strlen(str);
    for (i = 0; i < len; i++)
    {
        if (str[i] == ' ')
        {
            return 1;
        }
    }
    return 0;
}


char **get_argv_by_split(const char *input)
{

    size_t wind_start, wind_end, input_len;
    char **argv;
    int cur_argc;
    char *cur_arg;
    size_t t_len;

    wind_start = 0;
    wind_end = 0;
    input_len = strlen(input);
    argv = (char **)malloc(sizeof(char *) * MAX_COMMAND_ARGS);
    cur_argc = 0;
    while (wind_end <= input_len)
    {
        if (input[wind_end] == ' ' || wind_end == input_len)
        {
            t_len = wind_end - wind_start;
            if (t_len == 0)
            {
                wind_end++;
                wind_start++;
                continue;
            }
            cur_arg = (char *)malloc(sizeof(char) * (t_len + 1));
            memcpy(cur_arg, input + wind_start, t_len);
            cur_arg[t_len] = '\0';
            argv[cur_argc++] = cur_arg;
            wind_start = ++wind_end;
        }
        else if (input[wind_end] == '|')
        {
            t_len = wind_end - wind_start;
            if (t_len > 0)
            {
                // add new arg
                cur_arg = (char *)malloc(sizeof(char) * (t_len + 1));
                memcpy(cur_arg, input + wind_start, t_len);
                cur_arg[t_len] = '\0';
                argv[cur_argc++] = cur_arg;
            }
            // add pipe arg
            argv[cur_argc++] = PIPE_ARG;
            wind_start = ++wind_end;
        }
        else
        {
            wind_end++;
        }
    }
    if (cur_argc == 0)
    {
        free(argv);
        return NULL;
    }
    argv[cur_argc] = NULL;
    return argv;
}

ExecCommand parse_args(int argc, char *argv[])
{
    ExecCommand ex_cmd = {.repeat_count = DEFAULT_REPEAT, .timeout_secs = DEFAULT_TIMEOUT, .cmd_count = 0};
    Command *cur_cmd;
    char **all_argv, **t_argv;
    int want_next = 0;
    int i;
    int all_argc, t_argc;
    size_t arg_len = 0;
    ex_cmd.until_success = 0;
    ex_cmd.cmds = (Command **)malloc(sizeof(Command *));
    cur_cmd = (Command *)malloc(sizeof(Command));
    cur_cmd->command = NULL;
    ex_cmd.cmds[ex_cmd.cmd_count] = cur_cmd;

    all_argv = (char **)malloc(sizeof(char *) * MAX_COMMAND_ARGS);
    all_argc = 0;
    t_argv = NULL;
    for (i = 0; i < argc; ++i)
    {
        arg_len = strlen(argv[i]);
        if (arg_len > 0)
        {
            if (*argv[i] == '"' && *(argv[i] + arg_len - 1) == '"')
            {
                argv[i][arg_len - 1] = '\0';
                t_argv = get_argv_by_split(argv[i] + 1);
            }
            else if (exist_pipe_char(argv[i]) == 1)
            {
                t_argv = get_argv_by_split(argv[i]);
            }
            else if (exist_space_char(argv[i]) == 1)
            {
                t_argv = get_argv_by_split(argv[i]);
            }
            else
            {
                all_argv[all_argc++] = argv[i];
            }
            t_argc = 0;
            while (t_argv != NULL && t_argc < MAX_COMMAND_ARGS && t_argv[t_argc] != NULL)
            {
                all_argv[all_argc++] = t_argv[t_argc++];
            }
            t_argv = NULL;
        }
    }
    for (i = 0; i < all_argc; ++i)
    {
        arg_len = strlen(all_argv[i]);
        if (*all_argv[i] == '|')
        {
            cur_cmd = (Command *)malloc(sizeof(Command));
            cur_cmd->command = NULL;
            ex_cmd.cmds[++ex_cmd.cmd_count] = cur_cmd;
        }
        else
        {
            if (parse_program_arg(all_argv[i], &ex_cmd, &want_next) == 1)
            {
                continue;
            }
            // TODO if not get
            parse_cmd_arg(all_argv[i], &ex_cmd, &want_next);
        }
    }
    return ex_cmd;
}

void handle_timeout()
{
    fprintf(stderr, "[redo]Command timed out\n");
    exit(EXIT_FAILURE);
}

int input_cmd(char *cmd_strs[], int *cmd_strs_len)
{
    char input_cmd[MAX_COMMAND_ARGS * MAX_COMMAND_ARG_LEN];
    int wind_start, wind_end, cmd_len, cmd_index;

    fgets(input_cmd, MAX_COMMAND_ARGS * MAX_COMMAND_ARG_LEN, stdin);
    // remove \n
    input_cmd[strcspn(input_cmd, "\n")] = '\0';
    cmd_len = strlen(input_cmd);
    wind_start = 0;
    wind_end = 0;
    cmd_index = 0;
    while (wind_end <= cmd_len && cmd_index < *cmd_strs_len)
    {
        char *cur_arg;
        if (wind_end == cmd_len || (input_cmd[wind_end] == ' ' && wind_end - wind_start > 0))
        {
            if (strncmp((const char *)(input_cmd + wind_start), "quit", 4) == 0)
            {
                return 0;
            }
            cur_arg = (char *)malloc(sizeof(char) * (wind_end - wind_start + 1));
            if (cur_arg == NULL)
            {
                perror("[redo]cannot malloc men for store args\n");
                return 0;
            }
            memcpy(cur_arg, input_cmd + wind_start, (wind_end - wind_start));
            cur_arg[wind_end - wind_start + 1] = '\n';
            cmd_strs[cmd_index++] = cur_arg;
        }
        else if (input_cmd[wind_end] != ' ')
        {
            wind_end++;
            continue;
        }
        wind_end++;
        wind_start = wind_end;
    }
    if (cmd_index == 0)
    {
        return 0;
    }
    *cmd_strs_len = cmd_index;
    return 1;
}

int exec_multi_cmds(ExecCommand cmd_spec)
{
    if (cmd_spec.cmd_count == 0 && cmd_spec.cmds[0]->command == NULL)
    {
        printf("[redo]input cmd should at least  1\n");
        return -1;
    }

    pid_t *pids = (pid_t *)calloc(cmd_spec.cmd_count + 1, sizeof(pid_t));

    /* output fd of the previous process, initialized to
     * the stdout fd the parent process pointed to
     */
    int previous_out_fd = dup(STDIN_FILENO);
    /* we should not close STDIN here otherwise the child will close it again */

    int fds[2];
    for (int i = 0; i <= cmd_spec.cmd_count; i++)
    {
        if (pipe(fds) == -1)
        {
            perror("[redo]pipe create error");
            return errno;
        }
        int ret = fork();
        if (ret < 0)
        {
            perror("[redo]fork error");
            return errno;
        }
        else if (ret == 0) // child
        {
            pids[i] = getpid();

            /* assign the previous process's output fd to the stdin of the child process */
            dup2(previous_out_fd, STDIN_FILENO);
            close(previous_out_fd);

            close(fds[0]); // close read end

            /* if it is not the last process, assgin stdout of the child process
             * to the write end of the pipe, which connects the next child process.
             */
            if (i != cmd_spec.cmd_count)
                dup2(fds[1], STDOUT_FILENO);

            close(fds[1]); // close write end
            if (cmd_spec.timeout_secs > 0)
            {
                signal(SIGALRM, handle_timeout);
                alarm(cmd_spec.timeout_secs);
            }
            if (execvp(cmd_spec.cmds[i]->command, cmd_spec.cmds[i]->args) == -1)
            {
                fprintf(stderr, "[redo]exec cmd :'%s' failed,  %s\n", cmd_spec.cmds[i]->command, strerror(errno));
            }
            exit(EXIT_FAILURE);
        }
        else // parent
        {
            /* save the read end, which is the input of the next process */
            previous_out_fd = fds[0];

            close(fds[1]); // close write end

            /* we should'nt close the read end's fd before the child duplicated it */
        }
    }

    int status;
    for (int i = 0; i <= cmd_spec.cmd_count; i++)
    {
        waitpid(pids[i], &status, 0);
        int ret = WEXITSTATUS(status);
        if (ret != 0)
        {
            // fprintf(stderr, "[redo]sub cmd %s exec exit with error code %d\n", cmd_spec.cmds[i]->command, ret);
            return ret;
        }
            
    }
    return 0;
}

int main(int argc, char *argv[])
{
    ExecCommand cmd_spec;
    char *cmd_strs[MAX_COMMAND_ARGS];
    int cmd_strs_len;
    int ret;

    cmd_strs_len = MAX_COMMAND_ARGS;
    if (argc == 1 && input_cmd(cmd_strs, &cmd_strs_len) != 1)
    {
        return 0;
    }

    if (argc > 1)
    {
        cmd_spec = parse_args(argc - 1, argv + 1);
    }
    else
    {
        cmd_spec = parse_args(cmd_strs_len, cmd_strs);
    }
    if (show_help == 1)
    {
        print_help();
    }
    else
    {
        long cmd_round = 0;
        while (1)
        {
            fprintf(stdout, "[redo]----------------round %ld-------------------\n", cmd_round);
            ret = exec_multi_cmds(cmd_spec);
            if (cmd_spec.until_success == 1 && ret == 0)
            {
                break;
            }
            else
            {
                cmd_round++;
                if (cmd_round >= cmd_spec.repeat_count)
                {
                    break;
                }
            }
        }
    }
    return 0;
}
