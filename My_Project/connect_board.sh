#!/bin/bash
# 开发板连接脚本
ssh -o HostKeyAlgorithms=+ssh-rsa -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@10.20.20.36
