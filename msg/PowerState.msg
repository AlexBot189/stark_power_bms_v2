# 系统电源状态汇总 — 电源 → 上层
# 发布方: PowerRosAdapter (1Hz)
# 充电状态 + 电池 + 充电器 + 故障 汇总
Header header

# 充电状态 (对齐 ChargeState)
uint8 STATE_IDLE   = 0
uint8 STATE_DETECT = 1
uint8 STATE_CHARGE = 2
uint8 STATE_FULL   = 3
uint8 STATE_FAULT  = 4

uint8  charge_state   # 充电宏观状态
bool   adapter_online # 适配器是否插入

# 电池 (BMS)
uint16 batt_voltage_mv  # 电池电压 mV
int32  batt_current_ma  # 电池电流 mA (正=充电 负=放电)
int8   batt_temp_c      # 电池温度 °C
uint8  batt_soc         # SOC %

# 充电器 (IP2366)
uint8  charge_type      # 充电阶段 0=none 1=trickle 2=cc 3=cv
uint16 charge_current_ma # 充电电流 mA

# 故障
bool   fault            # 是否有故障
string fault_reason     # 故障原因 (空=无故障)
