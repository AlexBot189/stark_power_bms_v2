# 电池包实时故障状态 — V1.02 协议 (0x503B)
# 7字节格式: 系统/放电/充电各分一级二级 + 电池包状态

Header header

# 原始字节
uint8 sys_fault1      # 系统一级故障
uint8 sys_fault2      # 系统二级故障
uint8 dischg_fault1   # 放电一级故障
uint8 dischg_fault2   # 放电二级故障
uint8 chg_fault1      # 充电一级故障
uint8 chg_fault2      # 充电二级故障
uint8 pack_status     # 电池包状态

# 系统一级故障位 (sys_fault1)
bool comm_timeout       # bit0 通信超时
bool cell_ntc_fault     # bit1 电芯NTC故障
bool mos_ntc_fault      # bit2 MOS NTC故障
bool cell_ntc_short     # bit3 电芯NTC短路
bool cell_ntc_open      # bit4 电芯NTC开路
bool mos_ntc_short      # bit5 MOS NTC短路
bool mos_ntc_open       # bit6 MOS NTC开路

# 系统二级故障位 (sys_fault2)
bool cell_fault         # bit0 电芯故障
bool afe_fault          # bit1 AFE故障
bool fuse_fault         # bit2 FUSE故障

# 放电一级故障位 (dischg_fault1)
bool overdischarge_l1       # bit0 过放一级
bool dischg_overcurrent_l1  # bit1 放电过流一级
bool dischg_cell_overtemp   # bit2 放电电芯过温
bool dischg_mos_overtemp    # bit3 放电MOS过温
bool dischg_cell_lowtemp    # bit4 放电电芯低温

# 放电二级故障位 (dischg_fault2)
bool overdischarge_l2       # bit0 过放二级
bool dischg_overcurrent_l2  # bit1 放电过流二级
bool dischg_short           # bit2 放电短路
bool dischg_mos_fault       # bit3 放电MOS异常

# 充电一级故障位 (chg_fault1)
bool overcharge_l1       # bit0 过充一级
bool chg_overcurrent_l1  # bit1 充电过流一级
bool chg_overtemp        # bit2 充电过温
bool charger_fault       # bit3 充电器故障
bool chg_mos_fault       # bit4 充电MOS故障
bool chg_timeout         # bit5 充电超时

# 充电二级故障位 (chg_fault2)
bool overcharge_l2       # bit0 过充二级
bool chg_overcurrent_l2  # bit1 充电过流二级

# 电池包状态位 (pack_status)
bool chg_mos_active          # bit0 充电MOS开启
bool dischg_mos_active       # bit1 放电MOS开启
bool chg_mos_master_ctrl     # bit2 充电MOS由主控控制
bool dischg_mos_master_ctrl  # bit3 放电MOS由主控控制
