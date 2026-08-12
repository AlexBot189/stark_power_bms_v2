# 外骨骼电源管理框架设计

> 版本: V1.0 | 日期: 2026-08-12 | 维护: zhiqiang.yang

---

## 一、设计目标

构建可跨项目复用的电源管理框架。换充电IC、换BMS协议、换通信接口，框架代码不动，只增加驱动插件和改配置文件。

### 主要场景

1. 外骨骼 V1: IP2366(I2C) + BMS(UART RS-232) — 当前项目
2. 四足机器人: BQ25703(I2C) + BMS(CAN) — 换充电IC + 换BMS接口
3. 通用机器人: 任意充电IC + 任意BMS — 换驱动 .so

---

## 二、参考设计

本框架借鉴 Linux 内核 `power_supply` 子系统的核心思想:

1. **多设备模型**: 每个物理电源(充电IC/电池BMS)是独立的 power source 设备
2. **标准属性集**: 统一的属性枚举, 上层只通过属性名访问
3. **驱动层隔离**: 驱动实现 `get_property/set_property` 回调, 上层不关心硬件细节

```
Linux power_supply:               本框架:
/sys/class/power_supply/          PowerRegistry (单例注册表)
  ├── charger/                    ├── charger_ip2366 (IPowerSource)
  │   ├── online                  │   ├── ONLINE
  │   ├── status                  │   ├── STATUS
  │   └── voltage_now             │   └── VOLTAGE_NOW
  └── battery/                    └── battery_bms (IPowerSource)
      ├── capacity                    ├── CAPACITY
      └── temp                        └── TEMPERATURE
```

---

## 三、架构分层

```
                     PowerManager (充电状态机)
                          │
               ┌──────────┼──────────┐
               │          │          │
          readProps   evalFault   writeControl
               │          │          │
          ┌────┴──────────┴──────────┴────┐
          │        PowerRegistry           │
          │  registerSource()             │
          │  getSource()  findSources()   │
          │  getProp()    setProp()       │
          └────┬──────────────────────┬───┘
               │                      │
    ┌──────────┴──────┐    ┌──────────┴──────┐
    │  charger_ip2366 │    │  battery_bms     │
    │  (IPowerSource) │    │  (IPowerSource)  │  ← 驱动插件
    └────────┬───────┘    └────────┬─────────┘
             │                     │
        IP2366 IC              BMS UART
        (I2C 0xEA)          (RS-232 115200)
```

### 不改的层 (框架)

| 模块 | 文件 | 说明 |
|------|------|------|
| PowerProp | `PowerProp.h` | 40个标准属性枚举 |
| IPowerSource | `IPowerSource.h` | 统一电源设备接口 |
| PowerRegistry | `PowerRegistry.h/cpp` | 单例注册表 |
| PowerManager | `PowerManager.h/cpp` | 5状态充电管理 |
| ChargeStateMachine | `ChargeStateMachine.h/cpp` | 编译期转换表 |

### 可换的层 (驱动)

| 模块 | 文件 | 说明 |
|------|------|------|
| IP2366Source | `plugins/ip2366/` | IP2366 I2C + GPIO |
| BmsUartSource | `plugins/bms_uart/` | BMS UART (待实现) |

---

## 四、核心接口

### 4.1 标准属性枚举 (参考 Linux POWER_SUPPLY_PROP_*)

```
状态类:    STATUS, HEALTH, ONLINE, PRESENT, CHARGE_TYPE, FAULT, FAULT_REASON
测量类:    VOLTAGE_NOW(mV), CURRENT_NOW(mA), TEMPERATURE(×10), CAPACITY(%)
阈值类:    VOLTAGE_MAX, CURRENT_MAX, VOLTAGE_MIN
统计类:    CYCLE_COUNT, CAPACITY_FULL, CAPACITY_REMAIN
电芯:      CELL_VOLTAGE_1..6
控制类:    CHARGE_ENABLE, DISCHARGE_ENABLE, CHARGE_CURRENT_SET, CHARGE_VOLTAGE_SET
元数据:    MODEL_NAME, VERSION, SERIAL_NUMBER
```

### 4.2 统一接口

```cpp
class IPowerSource {
    virtual const char* name() const = 0;        // "charger_ip2366"
    virtual const char* type() const = 0;        // "charger" | "battery"
    virtual vector<PowerProp> supportedProps() = 0;
    virtual bool getProp(PowerProp, PowerValue&) = 0;
    virtual bool setProp(PowerProp, const PowerValue&) = 0;
    virtual void subscribe(ChangeCallback) = 0;
};
```

### 4.3 属性值类型

```cpp
struct PowerValue {
    enum Type { INT, BOOL, STRING } type;
    int64_t int_val;     // 测量值: mV, mA, ×10 temperature
    bool    bool_val;    // 开关状态
    string  str_val;     // 状态字符串: "charging", "good", "full"
};
```

---

## 五、充电状态机

### 5.1 状态定义

```
IDLE → DETECT → CHARGE → FULL → FAULT
  ↑      ↑         ↑        ↑       │
  └──────┴─────────┴────────┴───────┘
            (故障恢复)
```

IP2366 硬件自动管理充电曲线(涓流→CC→CV), SOC 层只需认知宏观阶段。

### 5.2 转换规则 (12条)

| 当前 | 事件 | 目标 |
|------|------|------|
| IDLE | ADAPTER_ONLINE | DETECT |
| IDLE | FAULT_DETECTED | FAULT |
| DETECT | PD_READY | CHARGE |
| DETECT | ADAPTER_OFFLINE | IDLE |
| DETECT | FAULT_DETECTED | FAULT |
| CHARGE | CHARGE_FULL | FULL |
| CHARGE | ADAPTER_OFFLINE | IDLE |
| CHARGE | FAULT_DETECTED | FAULT |
| FULL | ADAPTER_OFFLINE | IDLE |
| FULL | ADAPTER_ONLINE | CHARGE (再充电) |
| FULL | FAULT_DETECTED | FAULT |
| FAULT | FAULT_CLEARED | IDLE |

### 5.3 消抖策略

| 消抖项 | 阈值 | 目的 |
|--------|------|------|
| 适配器插入 | 200ms | 防接触不良误判 |
| 故障触发 | 500ms | 防瞬时尖刺误报 |
| 故障恢复 | 2000ms | 防恢复后立即再故障 |

### 5.4 双数据源交叉验证

PowerManager 同时读 charger 和 battery 两个 source, 交叉验证:
- 充电确认: charger(charging) + battery(charging)
- 充满确认: charger(full) + battery(SOC>=100%, I<stop)
- 故障确认: charger(fault) | battery(fault) | 温度超限 | 超时

---

## 六、IP2366 驱动

### 6.1 硬件连接

| 信号 | 接口 | 说明 |
|------|------|------|
| I2C | /dev/i2c-2, 0xEA | 寄存器读写 |
| INT | GPIO0_C7 | 双向: 唤醒拉高100ms, 故障/休眠拉低 |
| CHARGE_EN | GPIO2_A4 | 硬件充电通路开关, 高有效 |

### 6.2 关键约束

1. **16位ADC 先低后高**: 读低字节触发硬件锁存, 颠倒顺序数据错
2. **读-修改-写**: 寄存器写入必须先读, 只改目标位, 写回
3. **INT 双向协议**: 上升沿→等100ms→I2C就绪, 下降沿→16ms内停止I2C
4. **硬件自动充电**: IP2366 自行管理涓流/CC/CV, SOC 监控+干预

### 6.3 初始化流程

```
1. open /dev/i2c-2 → ioctl I2C_SLAVE 0xEA
2. 验证 I2C (读 0x31 寄存器的存在性)
3. 初始化 GPIO: INT(双边沿输入) + CHARGE_EN(输出低)
4. 写入充电参数:
   - PDO 选择 20V (0x0D)
   - 单节充满电压 4200mV (0x02)
   - 充电电流 3000mA (0x03)
   - 涓流电流 200mA (0x06)
   - 停充电流 100mA (0x08)
5. 启动 INT 线程
```

---

## 七、跨项目复用指南

### 换充电IC (IP2366 → BQ25703)

```
1. 写 BQ25703Source.h/cpp, 实现 IPowerSource 接口 (~250行)
2. robot_power.yaml 改一行:
   driver: "plugins/libpower_bq25703.so"
3. config 加 BQ25703 特有参数
4. PowerManager / PowerProp / PowerRegistry 不动
```

### 换BMS (UART → CAN)

```
1. 写 BmsCanSource.h/cpp, 实现 IPowerSource 接口 (~200行)
2. robot_power.yaml 改 driver 字段
3. PowerManager 通过同一套 STATUS/CAPACITY/TEMPERATURE prop 访问
```

### 新驱动开发模板

```
参考 plugins/ip2366/IP2366Source.h
1. 继承 IPowerSource
2. 实现 name()/type()/supportedProps()
3. 实现 getProp() switch 映射硬件状态到 PowerProp
4. 实现 setProp() 控制类属性
5. 实现 subscribe() 变化通知
6. 初始化中打开硬件、写默认参数、启动监控线程
```

---

## 八、文件清单

```
src/power/
├── PowerProp.h                   82行  属性枚举
├── IPowerSource.h                46行  统一接口
├── PowerRegistry.h               56行  注册表声明
├── PowerRegistry.cpp            115行  注册表实现
├── ChargeStateMachine.h          65行  状态机声明
├── ChargeStateMachine.cpp        49行  状态机转换表
├── PowerManager.h               155行  充电管理器声明
├── PowerManager.cpp             542行  充电管理器实现
└── plugins/
    └── ip2366/
        ├── IP2366Reg.h          135行  寄存器映射
        ├── IP2366Source.h       147行  驱动声明
        └── IP2366Source.cpp     687行  驱动实现
```
