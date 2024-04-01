/*
redo命令行工具
重复执行某一个具体的命令，知道命令执行成功或者达到退出条件。

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAX_COMMAND_ARGS 32
#define MAX_COMMAND_LENGTH 1024
#define DEFAULT_TIMEOUT 0
#define DEFAULT_REPEAT 1

static int show_help = 0;

typedef struct
{
    char *command;
    int repeat_count;
    long timeout_secs;
    char *args[MAX_COMMAND_ARGS];
    int args_count;
    int until_success;
} CommandSpec;

void print_help();
// 解析带单位的时间字符串为秒数
long parse_time_with_units(const char *time_str)
{
    long duration = 0;
    char unit = time_str[strlen(time_str) - 1];
    char *endptr;

    errno = 0;
    long raw_duration = strtol(time_str, &endptr, 10);
    if (errno != 0 || endptr == time_str || *endptr != unit)
    {
        fprintf(stderr, "Invalid time format. Expected <number><unit> where unit is s/m/h\n");
        exit(EXIT_FAILURE);
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
        fprintf(stderr, "Invalid time unit. Only 's', 'm' and 'h' are supported.\n");
        exit(EXIT_FAILURE);
    }

    return duration;
}

// 解析命令行参数
CommandSpec parse_args(int argc, char *argv[])
{
    CommandSpec spec = {.command = NULL, .repeat_count = DEFAULT_REPEAT, .timeout_secs = DEFAULT_TIMEOUT};
    spec.args_count = 0;
    spec.until_success = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "-h") == 0)
        {
            show_help = 1;
            return spec;
        }
        else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--timeout") == 0)
        {
            if (++i < argc && *(argv[i]) != '-')
            {
                // 添加对单位的解析
                spec.timeout_secs = parse_time_with_units(argv[i]);
                continue;
            }
            else
            {
                fprintf(stderr, "Timeout value missing after -e/--timeout\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--repeat") == 0)
        {
            if (++i < argc && *(argv[i]) != '-')
            {
                spec.repeat_count = strtol(argv[i], NULL, 10);
                continue;
            }
            else
            {
                fprintf(stderr, "Repeat count missing after -r/--repeat\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (strcmp(argv[i], "-u") == 0)
        {
            spec.until_success = 1;
        }
        else if (spec.command == NULL)
        {
            spec.command = argv[i];
            continue;
        }
        else
        {
            if (spec.args_count < MAX_COMMAND_ARGS)
            {
                spec.args[spec.args_count++] = argv[i];
            }
            else
            {
                fprintf(stderr, "Too many command arguments.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (spec.command == NULL)
    {
        fprintf(stderr, "No command provided\n");
        exit(EXIT_FAILURE);
    }

    return spec;
}

// 超时信号处理器
void handle_timeout(int signum)
{
    fprintf(stderr, "Command timed out\n");
    exit(EXIT_FAILURE);
}

// 示例主函数，调用参数解析函数并执行命令
int main(int argc, char *argv[])
{

    CommandSpec cmd_spec = parse_args(argc, argv);
    long cmd_repeated = 0;

    // 构建完整命令参数数组
    char *full_cmd[MAX_COMMAND_ARGS + 2];

    if (show_help == 1)
    {
        print_help();
        exit(0);
    }
    full_cmd[0] = cmd_spec.command;
    memcpy(&full_cmd[1], cmd_spec.args, sizeof(char *) * cmd_spec.args_count);
    full_cmd[cmd_spec.args_count + 1] = NULL;

    // 重复执行命令
    while (1)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("execute cmd failed on fork subprocess");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {                                    // 子进程
            signal(SIGALRM, handle_timeout); // 设置超时信号处理器

            // 设置超时时间
            if (cmd_spec.timeout_secs > 0)
            {
                alarm(cmd_spec.timeout_secs);
            }

            if (execvp(cmd_spec.command, full_cmd) == -1)
            {
                perror("execute cmd failed on execvp");
            }
            exit(EXIT_FAILURE);
        }
        else
        { // 父进程
            int status;
            waitpid(pid, &status, 0); // 等待子进程结束
            cmd_repeated++;

            if (WIFEXITED(status))
            {
                // 子进程正常退出
                if (cmd_spec.until_success == 1)
                {
                    break;
                }
                printf("cmd: %s execute success, round: %ld\n", cmd_spec.command, cmd_repeated);
            }
            else if (WIFSIGNALED(status))
            {
                // 子进程因信号而结束
                fprintf(stderr, "cmd: %s execute failed\n", cmd_spec.command);
                if (cmd_spec.until_success == 1)
                {
                    continue;
                }
            }
            if (cmd_spec.repeat_count > 0 && cmd_repeated == cmd_spec.repeat_count)
            {
                break;
            }
        }
    }
    return 0;
}

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
