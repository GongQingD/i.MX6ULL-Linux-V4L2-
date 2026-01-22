# Claude Code 行为准则 (ATK-IMX6U Edition)

## 1. 连接规则 (必须遵守)
由于开发板 SSH 版本较旧，执行任何远程命令时，**必须**携带 `-o HostKeyAlgorithms=+ssh-rsa` 参数。

## 2. 常用命令封装
* **进入开发板终端**:
  `sshpass -p "2918" ssh -o HostKeyAlgorithms=+ssh-rsa root@10.20.20.36`

* **执行远程命令**:
  `sshpass -p "2918" ssh -o HostKeyAlgorithms=+ssh-rsa root@10.20.20.36 "<command>"`

* **上传文件到开发板**:
  `sshpass -p "2918" scp -o HostKeyAlgorithms=+ssh-rsa <本地文件> root@10.20.20.36:<远程路径>`

## 3. 工作流
在修改驱动代码后：
1. 在本地 Ubuntu 交叉编译生成 `.ko` 文件。
2. 使用 `scp` 命令将 `.ko` 文件上传到板子 `/lib/modules`。
3. 使用 `ssh` 命令在板子上执行 `insmod` 和 `dmesg` 查看结果。
4. 如果获取到新的硬件参数，**立即更新** `BOARD_CONTEXT.md`。
