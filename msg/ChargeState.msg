# 充电状态 — 电源 → 上层
# 发布方: PowerRosAdapter (1Hz)
Header header

# 充电状态 (对齐 PowerManager::ChargeState)
uint8 STATE_IDLE   = 0
uint8 STATE_DETECT = 1
uint8 STATE_CHARGE = 2
uint8 STATE_FULL   = 3
uint8 STATE_FAULT  = 4

uint8  state          # 充电状态
bool   adapter_online # 适配器是否插入
uint16 batt_voltage_mv # 电池电压 mV
int32  batt_current_ma # 电池电流 mA (正=充电 负=放电)
int8   batt_temp_c    # 电池温度 °C
uint8  batt_soc       # 电池 SOC %
string fault_reason   # 故障原因 (空=无故障)
