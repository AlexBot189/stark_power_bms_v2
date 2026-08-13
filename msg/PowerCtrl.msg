# 电源控制指令 — 上层 → 电源
# 订阅方: PowerRosAdapter
Header header

# 指令类型
uint8 CTRL_NONE      = 0
uint8 CTRL_CHARGE    = 1  # 充电使能/禁止
uint8 CTRL_DISCHARGE = 2  # 放电使能/禁止
uint8 CTRL_CURRENT   = 3  # 充电电流限制
uint8 CTRL_SHUTDOWN  = 4  # 关机 (BMS 0x9001 断开电池输出)
uint8 CTRL_STANDBY   = 5  # 强制待机 (IP2366 0x09 进待机)

uint8  cmd              # 指令类型, 见上
bool   enable           # CTRL_CHARGE/DISCHARGE: true=使能 false=禁止
uint16 current_limit_ma # CTRL_CURRENT: 充电电流上限 mA
uint32 delay_ms         # 预留: 延时参数 (当前 BMS 关机固定 30s, 未使用)
