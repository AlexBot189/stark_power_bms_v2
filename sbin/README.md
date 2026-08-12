## 一些脚本和示例数据

[TOC]

### 激光雷达数据可视化

#### 脚本介绍

##### `showLastPointClouds.py`

- 雷达原始点云数据可视化
- 需要将`ldlidar.csv`文件放到脚本同级目录

##### `showOriginPointClouds.py`

- 雷达坐标系转换到机器坐标系后且剔除了柱子的点云数据可视化
- 需要将`ldlidar_after.csv`文件放到脚本同级目录

#### 使用说明

如果需要进行雷达数据可视化，需要先在机器上的`/data/config/PeriphConfig.json`文件中`"lidarOption"`下增加一个`"saveData": 1`的字段（默认生成的配置文件没有该字段），然后再运行`eros_periph_manager_node`程序，会在`/tmp/log`目录下生成两个文件：`ldlidar.csv`、`ldlidar_after.csv`

```json
"saveData": 1
```

完整配置文件示例：

```json
{
    "coreOption":
    {
        "tty": "/dev/ttyS5",
        "baudRate": 115200
    },
    "lidarOption":
    {
        "lidarVendor": "LD",
        "tty": "/dev/ttyS4",
        "baudRate": 230400,
        "x": 0.0683,
        "y": 0.0,
        "theta": 85.0
    },
    "lineLaserOption":
    {
        "tty": "/dev/ttyS1",
        "vender": "HC",
        "baudRate": 921600
    }
}
```



