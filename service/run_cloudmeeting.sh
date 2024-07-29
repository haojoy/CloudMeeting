#!/bin/bash

# # 检查是否提供了所需的参数
# if [ $# -ne 2 ]; then
#     echo "Usage: $0 <threads> <processes>"
#     exit 1
# fi

# # 获取命令行参数
# THREADS=$1
# PROCESSES=$2

# 获取本机 IP 地址
IP=$(hostname -I | awk '{print $1}')

if [ -z "$IP" ]; then
    echo "Failed to retrieve IP address."
    exit 1
fi

# 端口号
PORT=8888

MAX_LOG_SIZE=102400 # 100KB
# 检查日志文件大小并删除
if [ -f output.log ]; then
    LOG_SIZE=$(stat -c%s "output.log")
    if [ "$LOG_SIZE" -ge "$MAX_LOG_SIZE" ]; then
        echo "Log file too large ($LOG_SIZE bytes). Deleting..."
        rm -f output.log
    fi
fi

# 开始日志记录
exec >> output.log 2>&1
echo "================================================"

# 运行 cloudmeeting 程序
# nohup ./cloudmeeting $IP $PORT $THREADS $PROCESSES &
nohup ./cloudmeeting 0.0.0.0 8080 2 4 &
