# 外骨骼助力机器人 — 电源管理系统设计

> 版本: V1.4 | 日期: 2026-08-13 | 维护: zhiqiang.yang

---

## 一、背景

外骨骼助力机器人使用 6S 锂电池供电，电源系统由以下硬件组成：

| 硬件 | 接口 | 说明 |
|------|------|------|
| IP2366 快充芯片 | I2C /dev/i2c-2, 0xEA | PD/QC 快充协议，自动管理充电曲线 |
| BMS 电池保护板 | UART /dev/ttyS2, 115200/8N1 | SOC/电压/电流/温度/故障，V1.02 协议 |
| INT GPIO | GPIO0_C7 | IP2366 唤醒/休眠/故障通知（双向） |
| CHARGE_EN GPIO | GPIO2_A4 | 硬件充电通路开关 |

电源管理需要解决的问题：
1. 充电器插入/拔出检测与适配
2. 充电过程监控（IP2366 硬件自动走涓流/CC/CV，SOC 只需监控+干预）
3. 双数据源交叉验证（IP2366 电压电流 + BMS SOC温度故障）
4. 充电异常处理（过温/过流/超时/通讯中断）
5. 故障恢复与消抖

---

## 二、架构设计

### 2.1 整体架构

```
stark_power_manager_node (同一进程)

├── BatteryDispatcher      — BMS UART 通信 (已有)
├── WebServer              — WebSocket 服务 (已有)
│
├── PowerRegistry          — 电源设备注册表 (新增)
├── PowerManager           — 充电状态机 (新增)
├── IP2366Source           — IP2366 I2C 驱动 (新增)
├── BmsUartSource          — BMS 数据包装 (已实现，对接 BatteryDispatcher)
└── StarkRosAdapter        — 电池+电源 ROS/WebSocket 统一接口 (新增)
```

### 2.2 分层

```
                     PowerManager (充电决策)
                          │
          ┌───────────────┼───────────────┐
          │               │               │
     readProps       evalFault     writeControl
          │               │               │
          └──────┬────────┴───────┬───────┘
                 │                │
    ┌────────────┴───┐   ┌────────┴──────┐
    │charger_ip2366  │   │ battery_bms   │
    │ (IPowerSource) │   │ (IPowerSource)│
    └───────┬────────┘   └───────┬───────┘
            │                    │
       IP2366 IC            BMS UART
       (I2C 0xEA)        (RS-232 115200)
```

### 2.3 参考设计

借鉴 Linux 内核 `power_supply` 子系统的多设备模型：每个物理电源设备（充电IC、电池BMS）是独立的 power source，有标准属性集，上层管理逻辑只通过属性名访问，不关心底层硬件类型。

---

## 三、核心接口

### 3.1 属性枚举

统一单位：电压 mV、电流 mA、温度 ×10（例 32.5°C=325）、SOC 百分比。

```
状态类:    STATUS("idle"/"charging"/"full"/"fault")
           HEALTH("good"/"overheat"/"overcurrent")
           ONLINE(bool 适配器插入)、CHARGE_TYPE("trickle"/"cc"/"cv")
           
测量类:    VOLTAGE_NOW(mV)、CURRENT_NOW(mA)、TEMPERATURE(×10)、CAPACITY(%)

阈值类:    VOLTAGE_MAX、CURRENT_MAX、VOLTAGE_MIN

统计类:    CYCLE_COUNT、CAPACITY_FULL(mAh)、CAPACITY_REMAIN(mAh)

电芯:      CELL_VOLTAGE_1..6(mV)

控制类:    CHARGE_ENABLE(bool)、DISCHARGE_ENABLE(bool)
           CHARGE_CURRENT_SET(mA)、CHARGE_VOLTAGE_SET(mV)
```

### 3.2 电源设备接口

```cpp
class IPowerSource {
    const char* name() const;              // "charger_ip2366"
    const char* type() const;              // "charger" / "battery"
    vector<PowerProp> supportedProps();
    bool getProp(PowerProp, PowerValue&);  // 读属性
    bool setProp(PowerProp, PowerValue&);  // 写属性 (控制类)
    void subscribe(ChangeCallback);        // 状态变化通知
};
```

### 3.3 设备注册表

```cpp
PowerRegistry::instance().registerSource(make_unique<IP2366Source>(cfg));
PowerRegistry::instance().registerSource(make_unique<BmsUartSource>(cfg));

// 上层通过名字读属性, 不关心底层是什么硬件
PowerValue v;
PowerRegistry::instance().getProp("charger_ip2366", PowerProp::ONLINE, v);
PowerRegistry::instance().getProp("battery_bms",     PowerProp::CAPACITY, v);
```

---

## 四、充电状态机

### 4.1 状态定义

IP2366 硬件自动管理涓流→CC→CV→充满的充电曲线。SOC 层只需认知 5 个宏观状态：

```
IDLE → DETECT → CHARGE → FULL → FAULT
  ↑      ↑         ↑        ↑       │
  └──────┴─────────┴────────┴───────┘
```

| 状态 | 说明 | 硬件在做什么 |
|------|------|------------|
| IDLE | 未充电 | 无适配器 |
| DETECT | 检测适配器/PD协商 | IP2366 自动 PD 协商 |
| CHARGE | 充电中 | IP2366 自动走涓流→CC→CV |
| FULL | 充满 | IP2366 报 CHG_End=1 |
| FAULT | 故障 | 等待恢复 |

### 4.2 转换规则

| 当前 | 事件 | 目标 | 触发条件 |
|------|------|------|---------|
| IDLE | 适配器插入 | DETECT | charger ONLINE=true, 消抖 200ms |
| IDLE | 故障 | FAULT | BMS 故障/温度异常 |
| DETECT | PD 就绪 | CHARGE | charger STATUS="charging" |
| DETECT | 适配器拔出 | IDLE | charger ONLINE=false |
| DETECT | PD 超时 | FAULT | 10 秒未就绪 |
| CHARGE | 充满 | FULL | charger CHG_End=1 或 (SOC=100%+I<200mA) |
| CHARGE | 适配器拔出 | IDLE | — |
| CHARGE | 故障 | FAULT | 消抖 500ms |
| FULL | 适配器拔出 | IDLE | — |
| FULL | 再充电 | CHARGE | 电池电压 < 满充电压 - 200mV |
| FULL | 故障 | FAULT | — |
| FAULT | 故障恢复 | IDLE | 温度正常+BMS OK+charger OK, 消抖 2s |

### 4.3 双数据源交叉验证

充电状态需要 charger 和 battery 两个 source 都确认才信任：

- 充电确认: charger(charging) AND battery(charging)
- 充满确认: charger(full) OR (battery SOC>=100% AND current < 200mA)
- 故障: charger(fault) OR battery(fault) OR 温度>85°C OR 超时

### 4.4 消抖策略

| 消抖项 | 阈值 | 目的 |
|--------|------|------|
| 适配器插入 | 200ms | 防 Type-C 接触不良 |
| 故障触发 | 500ms | 防瞬时尖刺误报 |
| 故障恢复 | 2000ms | 防恢复后立即再故障 |

---

## 五、IP2366 驱动

### 5.1 寄存器操作约束

来自 IP2366 手册 V1.16：

1. **16 位 ADC 先低后高**：读低字节触发硬件锁存，顺序颠倒数据错
2. **读-修改-写**：寄存器写入必须先读出原值，只修改目标位，写回
3. **INT 双向协议**：上升沿→等 100ms→I2C 就绪；下降沿→16ms 内停止 I2C
4. **硬件自动充电**：IP2366 自行管理涓流/CC/CV 转换，SOC 只监控和干预

### 5.2 初始化序列

```
1. open("/dev/i2c-2") → ioctl(I2C_SLAVE, 0xEA)
2. 验证 I2C 通信 (读 0x31 是否成功)
3. GPIO 初始化:
   INT:      gpiochip0 line 39, 双边沿输入 (上升=唤醒, 下降=休眠/故障)
   CHARGE_EN: gpiochip2 line 4, 输出低 (初始关闭充电通路)
4. 写入充电参数:
   - PDO 选择 20V   (0x0D = 5)
   - 单节充满 4200mV (0x02)
   - 充电电流 3000mA  (0x03)
   - 涓流电流 200mA   (0x06)
   - 停充电流 100mA   (0x08)
   - INT 异常通知使能  (0x00 bit5)
   - 禁止待机          (0x09 bit7)
5. 启动 INT 监控线程
```

### 5.3 关键寄存器

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| SYS_CTL0 | 0x00 | bit0:充电使能, bit5:INT异常通知 |
| SYS_CTL2 | 0x02 | 单节充满电压 Vset=N×10+2500mV |
| SYS_CTL3 | 0x03 | 充电电流 Iset=N×100mA |
| SYS_CTL6 | 0x06 | 涓流电流 N×50mA |
| SYS_CTL8 | 0x08 | [7:4]停充电流, [3:0]再充电阈值 |
| SYS_CTL9 | 0x09 | bit7:待机使能(需设0禁止) |
| SELECT_PDO | 0x0D | [2:0]PDO: 5V=1,9V=2,12V=3,15V=4,20V=5 |
| STATE_CTL0 | 0x31 | bit5:充电中, bit4:充满, [2:0]:充电阶段 |
| STATE_CTL2 | 0x33 | bit7:Vbus有电 |
| TYPEC_STATE | 0x34 | bit7:Sink连接, bit4:PD协商完成 |
| STATE_CTL3 | 0x38 | bit5:Vsys过流, bit4:Vsys短路(需写1清0) |
| BATVADC | 0x50-51 | 电池电压 mV (先低后高) |
| BATIADC | 0x6E-6F | 电池电流 mA |
| SYSIADC | 0x70-71 | 系统电流 mA |

---

## 六、BMS 控制交互（基于 V1.02 协议）

### 6.1 SOC 主动下发的控制指令

充电流程中 SOC 通过 BatteryDispatcher 向 BMS 下发控制：

| 指令 | 协议触发条件 | 代码调用 |
|------|------------|---------|
| 0x2006 充电器接入通知 | 适配器插入/拔出 | `SetChargerStatus(true/false)` |
| 0x2007 充放电MOS控制 | 充满关充电MOS / 故障关放电 | `ControlMOS(chg, dischg)` |

### 6.2 BMS 被动上报的状态数据

BatteryDispatcher 周期性轮询 BMS，结果通过 Observer 回调通知：

| 指令 | 数据 | 充电流程中的使用 |
|------|------|----------------|
| 0x2004 电芯电压 | 总电压 + 6节电芯 mV | 单节压差检测 |
| 0x5002 电流温度 | 充放电电流 mA + BMS温度 | 充满判断(I<200mA) |
| 0x5004 SOC容量 | SOC% + 剩余容量 + BMS状态码 | SOC=100%判断, 0x0A=充电/0x1A=放电 |
| 0x503B 实时故障 | 7字节故障状态 (系统/放电/充电分级) | 故障分级判断 + 禁充/限放决策 |
| 0x503C 故障次数 | 11种历史故障计数 | 诊断参考 |

### 6.3 BmsUartSource 状态映射

将 BatteryFault + BatteryStatus 映射到标准 PowerProp：

```
STATUS:     bms_status=0x0A → "charging"
            bms_status=0x1A → "discharging"
            anyFault()       → "fault"

HEALTH:     chg_overtemp || dischg_overtemp → "overheat"
            overcharge_l1 || overdischarge_l1 → "overvoltage"
            无故障 → "good"

FAULT:      sys_fault1!=0 || sys_fault2!=0
            || chg_fault1!=0 || dischg_fault1!=0

CAPACITY:   soc_percent
TEMPERATURE: temperature_c × 10 (摄氏度×10, 例 325=32.5°C)
VOLTAGE_NOW: total_voltage_mv
CURRENT_NOW: current_ma (正=充电, 负=放电)
```

### 6.4 充电控制发起路径

```
IP2366 INT 上升沿 (适配器插入)
  → intThreadFunc() 等 100ms → readChargeState()
  → PowerManager::setChargerOnline(true)  [atomic 通知]

PowerManager::tick() [1Hz, poll_thread]
  → readChargerStatus()  / readBatteryStatus()
  → 评估状态转换
  → 状态变化时:
      IDLE→DETECT:  (无需 BMS 控制)
      DETECT→CHARGE: SetChargerStatus(true)   [0x2006]
      CHARGE→FULL:   ControlMOS(CLOSE, NO_ACT) [0x2007 关充电]
      CHARGE→FAULT:  setProp(CHARGE_ENABLE,false) [关IP2366]
                     + ControlMOS(CLOSE, CLOSE) [0x2007]
      FULL→CHARGE(再充电): SetChargerStatus(true) [0x2006]
```

---

## 七、BMS 数据接入

BatteryDispatcher 已有完整的 BMS UART 通信能力（V1.02 协议，12 条指令全支持）。`BmsUartSource` 将 BatteryDispatcher 包装为 IPowerSource 接口，使 PowerManager 能通过标准属性访问 BMS 数据（已实现，见 `src/power/plugins/bms_uart/`）：

```
BatteryDispatcher (现有，不改)
  └── BmsUartSource (新增, ~100行)
      ├── getProp(STATUS)     → "charging"/"discharging"/"full"
      ├── getProp(CAPACITY)   → BMS SOC
      ├── getProp(TEMPERATURE)→ BMS 温度×10
      ├── getProp(VOLTAGE_NOW)→ BMS 总电压 mV
      ├── getProp(FAULT)      → BMS 故障状态
      └── setProp(CHARGE_ENABLE) → 0x2007 ControlMOS
```

状态映射：

| BMS 数据 | PowerProp |
|----------|-----------|
| BatteryStatus::soc_percent | CAPACITY |
| BatteryStatus::temperature_c × 10 | TEMPERATURE |
| BatteryStatus::total_voltage_mv | VOLTAGE_NOW |
| BatteryStatus::current_ma | CURRENT_NOW |
| BatteryStatus::bms_status | STATUS |

---

## 八、线程模型

```
现有线程:
  recv_thread (BatteryDispatcher) — BMS UART 接收
  send_thread (BatteryDispatcher) — BMS UART 发送
  poll_thread (BatteryDispatcher) — BMS 数据轮询
  ros::spin (main)                 — ROS 消息循环

新增线程:
  int_thread (IP2366Source)        — GPIO 边沿等待 (阻塞 poll)
  
充电管理 (PowerManager):
  不在独立线程中, 在 poll_thread 里 1Hz tick
  setChargerOnline() 由 int_thread 通过 atomic 通知
```

---

## 九、文件清单

```
src/power/
├── PowerProp.h                   82行  属性枚举
├── IPowerSource.h                46行  统一接口
├── PowerRegistry.h/cpp          171行  注册表
├── ChargeStateMachine.h/cpp     114行  状态机转换表
├── PowerManager.h/cpp           697行  充电管理器
├── plugins/ip2366/
│   ├── IP2366Reg.h              145行  寄存器定义(逐bit对照手册)
│   ├── IP2366Source.h           147行  驱动声明
│   └── IP2366Source.cpp         687行  驱动实现
└── plugins/bms_uart/
    ├── BmsUartSource.h           60行  适配器声明
    └── BmsUartSource.cpp        230行  BatteryDispatcher → PowerProp 映射
```

---

## 十、框架复用（后续项目）

本架构设计时预留了跨项目复用能力：

- **换充电IC**：实现 IPowerSource 接口（参考 IP2366Source），静态注册 1 行。PowerManager / PowerRegistry 不动。
- **换BMS协议**：同上，实现 IPowerSource 接口并注册。
- **换通信接口**：IPowerSource 接口屏蔽了底层（I2C/UART/CAN/SPI），上层无感知。

换硬件的改动量：约 200 行新驱动代码 + 1 行注册修改。框架核心层零改动。

---

## 十一、变更记录

### V1.1 (2026-08-13)
- BmsUartSource 落地 (`src/power/plugins/bms_uart/`)
- 静态注册取代 dlopen 插件 (去掉 driver 字段)
- 修复: 交叉验证接线 (isActuallyCharging + m_bms_available 降级)
- 修复: INT 下降沿误判故障 (只置 vbus_ok, 故障交 readChargeState 读 0x38)
- 修复: I2C 并发保护 (m_i2c_mutex)
- 修复: initialize test_val 判断 (改 bool i2c_ready)
- 修复: CC 超时误报 (基于 m_charge_type)
- 修复: readADC16 失败返回 0 (改 bool + out, 失败保留旧值)
- 新增: INT 超时兜底轮询 (1Hz, 防 INT 丢失)
- 新增: 0x38 异常位写回清除 (对齐厂商 SDK)
- 修正: TEMPERATURE 映射为 temperature_c × 10 (摄氏度×10)

### V1.2 (2026-08-13)
- 新增 PowerRosAdapter: ROS 接口适配器 (订阅 PowerCtrl, 发布 ChargeState/PowerState)
- 新增 3 个 ROS 消息: PowerCtrl.msg / ChargeState.msg / PowerState.msg (msg/)
- PowerProp 新增 SHUTDOWN / STANDBY 控制属性
- BmsUartSource: SHUTDOWN → BMS 0x9001 断开电池输出
- IP2366Source: STANDBY → 0x09 bit7 使能 + bit6 进待机
- 关机/待机走 PowerRegistry::setProp 抽象, 不直接摸 BatteryDispatcher

### V1.3 (2026-08-13)
- 修复: I2C 地址 0xEA(8位) → 0x75(7位), 之前 IP2366 从未真正工作过
- 修复: I2C 读写改用 ioctl(I2C_SMBUS) 直驱, 去掉 i2c-tools/libi2c 依赖
- 修复: 配置合并为单一 JSON (batteryOption/web/ip2366/powerManager), 读不到回退默认值
- 修复: P0 状态转换下发控制 (applyControl), 消除状态机只观察不行动
- 修复: 充满改为关 IP2366 充电使能(不关 BMS MOS), 消除满电循环
- 修复: 再充电/开始充电重新开 IP2366 使能, 修复充电状态与 BMS 状态不一致
- 新增: 统一 ROS 接口 StarkRosAdapter (合并 BatteryRosAdapter + PowerRosAdapter)
- 新增: 客户端工具 stark_power_cli (订阅电池/充电信息 + 发送控制指令)
- 新增: PowerState 增加 IP2366 原始充电状态字段 (charger_active/full/voltage/phase)
- 新增: 充电状态 + 电流/电压曲线 web 显示 (power_state WebSocket 广播, 参考电机曲线)
- 新增: 故障显示动态化 (只显示触发的故障, MOS 状态移至充电状态区)

### V1.4 (2026-08-13)
- 长按 5s 关机功能: periph 节点长按校准按键 → 发布 PowerCtrl CTRL_SHUTDOWN → 本节点 0x9001 关机
- 本节点零改动 (关机链路已有), periph 节点改动以 patch 交付 (不在本仓库)
- 框架化: 关机触发统一走 PowerCtrl → PowerProp::SHUTDOWN → IPowerSource, 换 BMS 只换驱动
- 外部触发源 (按键/命令/上层算法) 都发布标准 PowerCtrl, 电源框架不感知触发源
