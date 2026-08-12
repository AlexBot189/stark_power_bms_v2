[TOC]
### 说明

工程根目录下的`config`目录下的`PeriphConfig.json`文件只是记录了配置文件的格式以及默认的配置参数，不用将其拷贝和安装，程序在运行的时候检测到系统没有配置文件会自动生成配置文件`/data/config/periph/PeriphConfig.json`。默认生成的配置文件中串口选项都不会生成波特率`baudrate`参数，但可以手动添加（代码中支持加载改参数，但在使用的地方需要自行编写）。

### 编译与模块依赖


**接口改动后，编译前，记得清缓存！**



### 发固件版本步骤

1. 确保doc目录下有report.json (空的就可以)
2. update.log中填写功能更新 参考如下

```json
{
    "date": "2022/10/28",
    "influence": "to_be_set",
    "eros_msg": {
        "involved": false,
        "autoupdate": true,
        "branch": "to_be_set",
        "commit": "to_be_set"
    },
    "content": [
        "1. 编写cpp demo",
        "2. 说明工程编译过程",
        "3. ...",
        "4. ...",
        "5. ..."
    ]
}
```

3. 执行编译打包命令

```shell
# 编译debug版本
./make.sh rk3576 pkg
# 编译release版本
./make.sh rk3576 release pkg
```

编译完成时注意是否有提示update.log格式有误

如果最后打印下面信息，即认为编译打包成功

>   ```ini
>   开始打包...
>   git信息：commit 48eda6a, branch master
>   ✔ 打包完成: rk3576-cpp_demo-48eda6a-master-debug.pkg.tar.bz2, 用时 0s
>   ```





