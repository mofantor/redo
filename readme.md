# Redo: A Command-Line Utility for Repeating Executions
## Introduction
Redo is a lightweight command-line tool designed to facilitate repeated execution of a specific command according to given criteria. It supports setting a timeout for command executions and specifying the number of repetitions. Moreover, it includes an option to keep repeating a command until it succeeds.

## Features
+ ```-e```, ```--timeout```: Sets a timeout for each command execution. Supports time strings with units (s, m, or h). For example, -e 10s represents a 10-second timeout.
+ ```-r```: Specifies how many times the command should be repeated.
+ ```-u```: Continuously repeats the command until it succeeds (exits with code 0).
## Usage Example
```Bash
redo -r 5 -e 10s ping google.com
```
This command would execute ping google.com five times, with a maximum execution time of 10 seconds per attempt.

```Bash
redo "ps aux | grep redo"
```
This will execute the command 'ps aux | grep redo'
## Installation
```Bash
git clone https://github.com/yourusername/redo.git
cd redo
make
sudo make install
```