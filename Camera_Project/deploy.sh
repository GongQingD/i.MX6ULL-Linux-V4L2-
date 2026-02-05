#!/bin/bash

# 脚本作用: 将编译好的可执行文件复制到嵌入式开发板。

# --- 请根据你的环境修改以下变量 ---

# 1. 本地可执行文件的路径
#    Qt Creator 通常会创建一个类似 'build-My_Project-Desktop-Debug' 的目录。
#    请确保此路径正确，或者直接将此脚本复制到可执行文件所在的目录中运行。
LOCAL_EXECUTABLE_PATH="../build-Camera_Project-ATK_I_MX6U-Debug/Camera_Project"

# 2. 开发板的目标目录
REMOTE_DIR="/lib/modules/4.1.15-g3dc0a4b"
# 2. 开发板的目标目录
# 3. 开发板的登录信息
REMOTE_USER="root"
REMOTE_HOST="xxx"
REMOTE_PASS="xxx"

# --- 脚本正文 ---

# 检查本地可执行文件是否存在
if [ ! -f "$LOCAL_EXECUTABLE_PATH" ]; then
    echo "错误: 未找到可执行文件 '$LOCAL_EXECUTABLE_PATH'。"
    echo "请检查路径是否正确，或者你是否已经成功编译项目。"
    exit 1
fi

echo "准备将 '$LOCAL_EXECUTABLE_PATH' 复制到 ${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_DIR}"

# 使用 scp 命令进行复制，并添加必要的 SSH 选项
sshpass -p "$REMOTE_PASS" scp -o HostKeyAlgorithms=+ssh-rsa "$LOCAL_EXECUTABLE_PATH" "${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_DIR}"

# 检查 scp 命令是否成功
if [ $? -eq 0 ]; then
    echo "复制成功！"
    echo "你可以通过 SSH 登录到开发板并运行程序。"
else
    echo "错误: 复制失败。请检查网络连接、SSH 权限和路径。"
    exit 1
fi
