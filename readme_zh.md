# Redo: 一个用于重复执行命令的命令行工具
## 简介
Redo是一款轻量级的命令行工具，旨在让用户轻松地重复执行某一特定命令，直到满足指定条件为止。它支持设置命令执行的超时时间和重复次数，并允许在命令连续失败时持续重试直至成功。

## 功能特性
+ ```-e```, ```--timeout```: 设置命令执行的超时时间，可接受带单位（s/m/h）的时间字符串，如-e 10s表示10秒超时。
+ ```-r```: 设置命令重复执行次数。
+ ```-u```: 直到命令执行成功（返回码为0）才停止重复执行。
### 使用示例
```Bash
redo -r 5 -e 10s ping google.com
```
上述命令将连续执行ping google.com五次，每次执行的最长时间限制为10秒。

## 安装
```Bash
git clone https://github.com/yourusername/redo.git
cd redo
make
sudo make install
```