#pragma once

#include <climits>
#include <cstdint>
#include <iostream>

// clang-format off
namespace stark_power_manager
{
struct OdomData
{
    uint64_t stamp;  //时间戳(us)
    float    x{ FLT_MAX };      //x偏移量，单位m
    float    y{ FLT_MAX };      //y偏移量，单位m
    float    yaw{ FLT_MAX };    //偏航角,单位rad
    float    vx;     //x方向平移速度，单位m/s
    float    vy;     //y方向平移速度，单位m/s
    float    vw;     //旋转角速度，单位rad/s
};

struct WheelDistance
{
    float leftWheel{ FLT_MAX };  //左轮里程，单位mm
    float rightWheel{ FLT_MAX }; //右轮里程，单位mm
};

struct ImuData
{
    uint64_t stamp;        //时间戳(精确到us) //px::get_time_micros()
    float    pitch = 0.0;  //横滚角,单位rad
    float    roll = 0.0;   //俯仰角,单位rad
    float    yaw = 0.0;    //偏航角,单位rad
    float    vx = 0.0;     //x轴旋转角速度，单位rad/s
    float    vy = 0.0;     //y轴旋转角速度，单位rad/s
    float    vz = 0.0;     //z轴旋转角速度，单位rad/s
    float    ax = 0.0;     //x方向平移速度加速度，单位m/s^2
    float    ay = 0.0;     //y方向平移速度加速度，单位m/s^2
    float    az = 0.0;     //z方向平移速度加速度，单位m/s^2
};

struct ServoData
{
    uint8_t type;
    uint16_t angle;
};

struct RtcData
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

typedef struct
{
    uint8_t action;
    uint32_t speed;
    uint32_t acceleration;
} WheelMotorSpeed;

struct MCUResetErrorBit
{
    uint64_t new_err_code;
};

struct MCUBatteryStatus
{
/*
    uint8_t  battery_percent;  // 电池电量   (0 - 100)
    uint8_t  battery_temp;//电池温度
    uint16_t battery_current;  // 电池电流  单位:mA
    uint16_t battery_voltage;  // 电池电压  单位:0.01V
    uint8_t  charge_state;//电池充电状态
    uint16_t charge_current;  // 充电电流  单位:mA
    uint16_t charge_voltage;  // 充电电压  单位:0.01V
*/

    uint8_t  battery_percent;  // 电池电量   (0 - 100)
    uint8_t  charge_state;//电池充电状态
    uint16_t battery_voltage;  // 电池电压  单位:0.01V
    uint8_t  battery_temp;//电池温度
//    uint16_t battery_current;  // 电池电流  单位:mA
    uint16_t charge_voltage;  // 充电功率
    uint16_t charge_current;  // 充电电流  单位:mA
    uint8_t charge_type;  // 充电类型  0-无，1-有线，2-无线

};

struct MCUTouchControl
{
    uint8_t  touch_part;  // 触摸部位
    uint8_t  touch_mode;//触摸模式
    uint8_t  touch_force;//触摸力度
    uint8_t  touch_ch;//触摸通道

};

struct MCUVersionInfo
{
    uint8_t version_type;   // 版本类型标识，固定为 0x01
    char version[5];   
};

struct MCUOtaRsp
{
    uint8_t ota_flag;   // OTA标识，固定为 0x01
};

struct MCUCurrentInfo
{
    uint8_t tailMotor;   // 尾巴电机
    uint16_t tailMotorCur;   // 尾巴电机电流mA
 
    uint8_t heater;   // 加热片
    uint16_t heaterCur;   // 加热片电流值mA

    uint8_t rollServo;   // 头部侧倾舵机
    uint16_t rollServoCur;   // 头部侧倾舵机电流值mA

    uint8_t yawServo;   // 头部左右舵机
    uint16_t yawServoCur;   // 头部左右舵机电流值mA
			    
    uint8_t pitchServo;   // 头部上下舵机
    uint16_t pitchServoCur;   // 头部上下舵机电流值mA

};

struct MCUTempInfo
{
    uint8_t env;   // 环境温度
    uint16_t envTemp_value;   // 环境温度值
			      
    uint8_t heaterLeft;   // 肚子左侧加热片温度
    uint16_t heaterLeftTemp_value;   // 肚子左侧加热片温度值
				     
    uint8_t heaterRight;   // 肚子右侧加热片温度
    uint16_t heaterRightTemp_value;   // 肚子右侧加热片温度值
				      
    uint8_t rollServo;   // 头部侧倾舵机温度
    uint16_t rollServoTemp_value;   // 头部侧倾舵机温度值 

    uint8_t yawServo;   // 头部左右舵机温度
    uint16_t yawServoTemp_value;   // 头部左右舵机温度值

    uint8_t pitchServo;   // 头部上下舵机温度
    uint16_t pitchServoTemp_value;   // 头部上下舵机温度值

};

struct MCUEcInfo
{
    uint8_t reserved;   // 预留字段
    uint8_t Ex_type;   // 异常类型
    uint8_t Ex_status;   // 异常状态
    uint32_t Ex_Code;   // 异常码
};

struct FactoryResetInfo
{
    uint8_t cmd;   //1-恢复出场设置
};

struct MCUGyroPoseInfo
{
    uint8_t  static_pose;  // 静态姿势
    uint8_t  dynamic_pose;//动态姿势
};

struct MCUPackSlowStatus
{

    uint64_t stamp;                // 时间戳(精确到us)
    uint64_t err_code;             // 异常码
    uint16_t left_moter_current;   // 左轮电机电流  单位:mA
    uint16_t right_moter_current;  // 右轮电机电流  单位:mA
    uint8_t  run_state_machine;    // 运动状态机
    uint8_t  motor_mode_control;   // 电机控制模式
    uint16_t along_wall_distance;  // 沿墙距离
    uint16_t roll_brush_current;
    uint16_t edge_brush_current;      // 边刷电机电流   单位:mA
    uint16_t air_absorbing_current;   // 主吸/风机电流   单位:mA
    uint16_t air_absorbing_speed;     // 主吸/风机转速 单位:rpm
    uint16_t water_pump_current;      // 水泵电流 单位:mA
    uint8_t  cliff_detect_status{ 0 };     // 悬崖检测
                                      // Bit0 ~ Bit3每一位对应一个悬崖检测传感器，其中0 – 没有检测到悬崖，1 – 检测到悬崖
                                      // cliff1| cliff2| cliff3| cliff4
    uint8_t  collide_detect_status{ 0 };   // 碰撞检测
                                      // Bit0 ~ Bit4每一位对应一个碰撞检测传感器，其中0 – 没有碰撞，1 – 有碰撞
                                      // lbumper| rbumper| top-left | top-right | top-down
    uint8_t  dangling_detect_status{ 0 };  // 悬空检测
                                      // Bit0 ~ Bit1每一位对应一个悬空检测传感器，其中0 – 无悬空，1 -  悬空
                                      // ldanglingr| rdangling
    uint8_t  charge_status;           // 充电检测 0 – 未充电，1 – 充电中
    uint8_t  in_pipe_status;          // 在桩状态检测 0 – 不在充电桩上，1 – 在充电桩上，2 – 缓慢掉电，3 – 突然掉电

    uint16_t battery_current;  // 电池电流  单位:mA
    uint16_t battery_voltage;  // 电池电压  单位:mV
    uint8_t  battery_percent;  // 电池电量   (0 - 100)
    int8_t   mcu_temprature;   // mcu温度单位：℃

    uint8_t water_tank_status;  // 水箱水量状态检测 0 – 水箱不缺水，1 – 水箱缺水
    uint8_t dust_box_status;    // 尘盒状态检测信号 0 – 尘盒尘未满，1 – 尘盒已尘满
    uint8_t rug_detect_status;  // 地毯状态 0 – 非地毯，1 – 地毯
    uint8_t net_status_433;     // 433
    uint8_t lift_motor_status;  // 升降电机状态
    uint8_t sta_bind_status;    // 基站绑定状态
    uint8_t dust_box_pos;       // 尘盒在位
    uint8_t mop_in_pos;         // 拖布在位

    uint8_t ira_recharge_fl;
    uint8_t ira_recharge_fr;
    uint8_t ira_recharge_rl;
    uint8_t ira_recharge_rr;
    uint16_t dust_box_press;        //        # 尘盒位置气压
    uint16_t mlift_motor_current;   //   # 拖布升降电机电流（单位：mA）
    uint16_t mshark_motor_current;  //  # 拖布震动电机电流（单位：mA）
};

struct MCUOnOffSensorStatus
{
    uint64_t stamp;                // 时间戳(精确到us)
    uint8_t  cliff_detect_status{ 0xFF };     // 下视检测
                                      // Bit0 ~ Bit3每一位对应一个悬崖检测传感器，其中0 – 没有检测到悬崖，1 – 检测到悬崖
                                      // bit3(right)| bit2(front right)| bit1(front left)| bit0(left)
    uint8_t  collide_detect_status{ 0xFF };   // 碰撞检测
                                      // Bit3~Bit7(no use),Bit0 ~ Bit2分别对应左右及LDS碰撞，其中数值0 – 没有碰撞，数值1 – 有碰撞
                                      // bit2(LDS bump)|bit1(right bump)|bit0(left bump)
    uint8_t  dangling_detect_status{ 0xFF };  // 悬空检测
                                      // Bit0 ~ Bit1每一位对应一个跌落检测传感器，其中0 – 无悬空，1 -  悬空
                                      // bit1(right)| bit0(left)
    uint8_t charge_status{ 0xFF };           // 充电检测 0 – 未充电，1 – 充电中
    uint8_t rug_detect_status{ 0xFF };  // 地毯状态 0 – 非地毯，1 – 地毯
    uint8_t dust_box_pos{ 0xFF };       // 尘盒状态 
                                        //bit0:尘盒安装检测(1-安装 0-未安装)
                                        //bit1:尘盒光电检测(1-触发 0-未触发)
                                        //bit2:一次性尘袋尘盒盖检测1(1-安装 0-未安装)
                                        //bit3:一次性尘袋尘盒盖检测2(1-安装 0-未安装)
    uint8_t mop_status{ 0xFF };         //拖布状态检测
                                //Bit0~Bit1:安装检测(0-未安装 1-已安装) Bi2~Bit3:抬升到位检测(1-抬升到位 0-不到位)
                                //Bit4:外扩电机归位是否到位(1-到位 0-未到位)Bit5:电机外扩是否到位(1-到位 0-未到位)
                                //bit5(外扩)|bit4(归位)|bit3(左)|bit2(右)|bit1(左)|bit0(右)
    uint8_t station_exception_status{ 0xFF }; //基站异常状态
                                //bit4:烘干电机异常状态(1-异常 0-正常) bit3:清洁槽溢水状态(1-溢水 0-正常)
                                //bit2:污水箱安装(1-已安装 0-未安装) bit1:清水箱安装(1-已安装 0-未安装)
                                //bit0:集尘袋安装(1-已安装 0-未安装)
    uint8_t station_work_status{ 0xFF }; //基站工作状态
                                //bit6:灯效开关(1-开启 0-关闭) bit5:热水加热器状态(1-开启 0-关闭)
                                //bit4:烘干加热器(1-开启 0-关闭) bit3:集尘风机(1-开启 0-关闭)
                                //bit2:风干电机(1-开启 0-关闭) bit1:污水电机(1-开启 0-关闭)
                                //bit0:清水电机(1-开启 0-关闭)
    uint8_t station_comm_status{ 0xFF };//基站通信状态 0-未通信 1-正常通信 2-通信异常
    uint8_t dust_collect_count{ 0xFF };//集尘次数
};

struct MCUIRSignalStatus
{
    uint64_t stamp;        //时间戳(精确到us)
    uint8_t chargingtype;//充电类型 0:标准充电类型 1.3D充电座
    uint8_t charger_signal;//是否处于充电中 0:未充电 1:充电
    uint8_t FleftSignal;//前左接收头信号 0:no signal 1:B signal 2:A signal 3:C signal 
    uint8_t FrightSignal;//前右接收头信号
    uint8_t SleftSignal;//边左接接收头信号
    uint8_t SrightSignal;//边右接收头信号
    uint8_t BleftSignal;//后左接收头信号
    uint8_t BrightSignal;//后右接收头信号
    uint8_t FleftVirtualSignal;//前左虚拟墙信号
    uint8_t FrightVirtualSignal;//前右虚拟墙信号
    uint8_t SleftVirtualSignal;//边左虚拟墙信号
    uint8_t SrightVirtualSignal;//边右虚拟墙信号
    uint8_t BleftVirtualSignal;//后左虚拟墙接号
    uint8_t BrightVirtualSignal;//后右虚拟墙信号
    uint16_t l_side_ir_value;  //左沿墙信号IR信号adc采样值
    uint16_t r_side_ir_value;  //右沿墙信号IR信号adc采样值
    uint16_t ml_front_ir_value;   // 前视信号IR信号adc采样值 -- 中间左边
    uint16_t mr_front_ir_value;   // 前视信号IR信号adc采样值 -- 中间右边
    uint16_t l_front_ir_value;    // 前视信号IR信号adc采样值 -- 左
    uint16_t r_front_ir_value;    // 前视信号IR信号adc采样值 -- 右
};

struct MCUStationComm
{
    uint64_t stamp;//ms
    uint8_t len;//指令长度
    std::vector<uint8_t> cmd;//指令内容,暂定最长不超过8个字节
};

struct MCUPackFastStatus
{
    uint64_t stamp;
    int32_t left_encode_val;
    int32_t right_encode_val;
    int16_t left_wheel_speed;   // # 左轮速度
    int16_t right_wheel_speed;  // # 右轮速度
    int16_t sliding_coefficient;
};

struct MCUSetDriverMotorRequest
{
    uint64_t stamp                   = 0;  // # 时间戳(精确到us)
    uint8_t  type                    = 0;
    uint8_t  brake_status            = 0;
    int16_t  target_linear_velocity  = 0;  //mm/s
    int16_t  target_angular_velocity = 0;  //mrad/s
    uint8_t  run_mode                = 0;
    int16_t  target_linear_vacc      = 0;
    int16_t  target_angular_vacc     = 0;
    int16_t  target_distance         = 0;
    int16_t  target_angular          = 0;
    int16_t  slam_rotating_rad       = 0;
    uint8_t  left_motor_pwm          = 0;
    uint8_t  right_motor_pwm         = 0;
    int32_t  vz_speed                = 0;  //imu 角速度
};


struct MCUSetDriverMotorResponse
{
    bool state;  // true 设置成功, false 设置失败
};

struct MCUSetRobotStatusRequest
{
    uint64_t stamp;             //时间戳(精确到us)
    uint8_t  work_mode;         // 运行模式
    uint8_t  work_display;      // 启动/暂停led显示模式
    uint8_t  recharge_display;  // 回冲led显示模式
    uint8_t wifi_status;
};

struct MCUSetRobotStatusResponse
{
    bool state;
};

struct MCUSetCommonMotorStatusRequest
{
    uint64_t stamp;
    uint8_t  cleanner_motor;            // 吸尘电机转速控制
    uint8_t  water_tank_motor;          // 水箱电机转速控制
    uint16_t water_pump_work_time = 0;  // 水泵出水时间（单位ms）
    uint16_t water_pump_stop_time = 0;  // 水泵关闭时间（单位ms）
    int8_t   roll_brush_motor;          // 滚刷电机转速控制
    uint8_t  edge_brush_motor;          // 边刷电机转速控制
    uint8_t  mop_rotates_motor;         // 拖布旋转电机转速控制
    uint8_t  mop_lifting_motor;         // 拖布升降电机
    uint8_t rug_stress_mode;
    uint8_t bumper_stress_mode = 2;     // 碰撞应激模式
};

struct MCUSetCommonMotorStatusResponse
{
    bool state;
};

struct MCUKeySignal
{
    uint64_t stamp;                   // 时间戳(精确到ms)
    uint8_t  work_key_value;          // 启动/暂停按键值
    uint8_t  recharge_key_value;      // 回充按键值
    uint8_t  sta_work_key_value;      // 启动/暂停按键值
    uint8_t  sta_recharge_key_value;  // 回充按键值
};

struct MCUPhyKey
{
    uint8_t key_type;                   //按键类型
    uint8_t key_event;          //按键事件
};

struct IRRealtimeData
{
    bool     valid = false;
    uint16_t ir_ad = 0;
};

struct TofCalibrateData
{
    uint8_t offset_valid = 0;    // bit0
    uint8_t xcross_valid = 0;    // bit1
    int8_t  offset       = 0;
    float   xcross       = 0.0;
};

struct CommonModuleStatus
{
    int module;
    int status;
};

struct CommonErrorCode
{
    int error_code;
    std::string description;
};

struct MCUOTATaskRequest
{
    uint64_t stamp;
    uint8_t type;
    uint8_t flag;
    uint8_t len;
    char bin[128];
    uint32_t size;
};

struct MCUOTATaskResponse
{
    uint8_t type;    //type = 1, MCU ot升级任务；
    int32_t state;   // state  = 0 , 空闲；state = 1, 执行中， state = 2, 执行成功， state = 3, 执行失败，
    int32_t result;  // result  = 0 , 执行成功 其它值, 反馈失败异常码
};

struct MCUFactoryRunInfo
{
    uint32_t stamp;            // 时间戳
    int32_t  left_encode_val;  // 左轮编码器累计值
    int32_t right_encode_val;
    int16_t left_speed;
    int16_t right_speed;
    uint16_t fans_speed;   // 风机转速，单位：rpm
    uint8_t  fl_recharge;  // 前左回冲
    uint8_t fr_recharge;
    uint8_t rl_recharge;  // 后左
    uint8_t rr_recharge;
    uint16_t left_wheel_current;  //左轮马达电流
    uint16_t right_wheel_current;
    uint16_t wind_wheel_current;  // 风轮马达电流
    uint16_t mid_brush_current;   // 中刷电流
    uint16_t edge_brush_current;  // 边刷电流
    uint16_t lfad_sample_value;   // 左前AD采样
    uint16_t lrad_sample_value;   // 左右AD采样
    uint16_t rfad_sample_value;
    uint16_t rrad_sample_value;
    uint16_t elec_plate_voltage;  // 充电极片电压
    uint16_t battery_voltage;     // 电池电压
    uint8_t battery_temp;
    uint8_t misc_status;        // bit5:回充键	bit4:电源键	bit3:右抬起	bit2:左抬起	bit1:右碰撞	bit0:左碰撞
    int16_t air_press_data;     // 气压数据
    uint8_t rug_sensor_status;  // 地毯传感器
    uint8_t commucation_uart;   // 433 通讯串口
    uint8_t commucation_mop;    // 拖布模块
};

struct MCUMemoryAddrValue
{

    uint8_t length;
    uint8_t data[256];
};

struct MCURequestMcuInfo
{
    uint8_t  type;    // 0x06 -> 硬件信息 , 0x07 -> 调试信息
    uint32_t addr;    // 内存地址 type = 0x07 有效
    uint8_t  length;  // 获取的长度
};

struct MCUTempControl
{
    int8_t index;//索引号
    int8_t mode;//温度开关
    int16_t value;//温度数据
};

struct MCUPower
{
    uint8_t type;//预留功率控制
    uint32_t power;//功率值
};

struct MCUPowerRsp
{
    int8_t code;// 0-成功 非0-失败
    uint8_t power;//充电功率
};

struct MCUMotorControl
{
    uint8_t index;//索引号
    uint8_t action;//控制类型
    uint16_t content;//参数
    uint16_t time;//时间
    uint8_t cmdType;//指令类型
};

struct MCUMotorControlReply
{
    uint32_t task_id;
    bool status;
};

struct LedControl
{
    uint8_t index;//索引号
    uint8_t mode;//控制效果
    uint8_t content;//时间参数
};

struct EyeControl
{
    uint32_t msg_id;//索引号
    int8_t mode;
    int8_t status;
    int16_t freq;
    int16_t count;
};

struct EyeControlReply
{
    uint32_t msg_id;//索引号
    int8_t mode;
    int8_t status;
    int16_t freq;
    int16_t count;
};

struct LedControlReply
{
    uint32_t task_id;
    bool status;
};

struct MCUSetStationControl
{
    uint8_t index;//索引号
    uint8_t onOff;//1:开启 0:关闭
    uint8_t lasttime;//持续时间
};

struct MCUSetStationControlReply
{
    uint32_t task_id{0};
    bool status{false};
};

struct MCUSetStationFunction
{
    uint8_t  func;  // :0-保留，1-集尘，2-拖布清洗，3-拖布烘干，4-基站自清洁，5-扫地机补水，10-自动童锁，11-自动息屏，12-按键声音
    uint16_t data;  // 参数, 根据 function 确定
};

struct MCUSetStationFunctionReply
{
    bool status;
};

struct MCUStationFunctionStatus
{
    uint8_t  dust_collect;    // 集尘功能状态，0-空闲，1-工作中，2-启动失败，3-停止中
    uint8_t  mop_clean;       // 拖布清洗功能状态，0-空闲，1-工作中，2-启动失败，3-停止中
    uint8_t  mop_dry;         // 拖布烘干功能状态，0-空闲，1-工作中，2-启动失败，3-停止中
    uint8_t  sta_self_clean;  // 基站自清洁功能状态，0-空闲，1-工作中，2-启动失败，3-停止中
    uint8_t  add_water;       // 扫地机补水功能状态，0-空闲，1-工作中，2-启动失败，3-停止中
    uint16_t hardware_fault;  // 硬件故障标志位
    uint16_t function_fault;  // 功能异常标志位
    uint16_t sensor_fault;    // 传感器故障
    uint8_t
            status;            // bit0：自动童锁，0-关闭，1-打开 bit1：自动息屏，0-关闭，1-打开 bit2：按键声音，0-关闭，1-打开 bit4：基站在工厂模式，0-否，1-是
    uint8_t dust_collect_cnt;  // 集尘次数
};

enum class StationType // 基站类型
{
    Station_Normal = 0, // 只充电
    Station_Omni = 1,   // 全能
    Station_Acs = 2,    // 只洗抹布
    Station_Aes = 3,     // 只集尘
    Station_Omni_Combo = 4
};

struct MCUStationStatus
{
    StationType stationType;
    std::string stationVersion{"V0.0.0"};
    std::string stationSN{"E1234567891234567890"};
    std::map<uint8_t,uint8_t> stationStates;//包含所有状态
    // uint8_t detect_water_tank;  //bit0：净水箱在位检测，0-不在位，1-在位;
    //                             //bit1：净水箱缺水检测，0-有水，1-缺水
    //                             //bit2：污水箱在位检测，0-不在位，1-在位
    //                             //bit3：污水箱满水检测，0-未满水，1-满水
    // uint8_t detect_flex_pipe;   //bit0：伸缩管限位开关（前），0-不到位，1-到位;
    //                             //bit1：伸缩管限位开关（后），0-不到位，1-到位
    // uint8_t detect_dust_box;    //bit0：尘袋在位检测，0-不在位，1-在位
    // uint8_t detect_lid;         //bit1：开盖检测，0-合上，1-打开

    // uint8_t flume_brush_status;  //bit0：水槽在位检测，0-不在位，1-在位;
    //                              //bit1：水槽满水检测，0-未满水，1-满水;
    //                              //bit2：刷头限位开关（左），0-不到位，1-到位;
    //                              //bit3：刷头限位开关（右），0-不到位，1-到位

    // uint16_t charger_voltage;     // 充电电压，（0 ~ 65535）mV
    // uint16_t charger_current;     // 充电电流，（0 ~ 65535）mA
    // uint32_t air_press_detect;    // 气压检测，（0 ~ 16777215）Pa
    // uint8_t  temp_detect;         // 温度检测，（-128 ~ 127）度
    // uint16_t water_pump_current;  // 电磁水泵电流，（0 ~ 65535）mA
    // uint16_t air_pump_current;    // 气泵电流，（0 ~ 65535）mA
    // uint16_t elec_water_current;  // 电解水电流，（0 ~ 65535）mA
    // uint16_t step_motor_a;        // 步进电机电流A，（0 ~ 65535）mA
    // uint16_t step_motor_b;        // 步进电机电流B，（0 ~ 65535）mA
    // uint16_t blower_current;      // 鼓风机电流，（0 ~ 65535）mA
    // int8_t   ptc_temp;            // PTC温度检测，（-128 ~ 127）度
};

struct MCUStaFunctionStatusReply
{
    bool status;
};

struct MCUSetCBTDataRequest
{
    uint16_t cliff_sensor1;  // 悬崖传感器1
    uint16_t cliff_sensor2;  // 悬崖传感器2
    uint16_t cliff_sensor3;  // 悬崖传感器3
    uint16_t cliff_sensor4;  // 悬崖传感器4
};

struct MCUSetCBTDataResponse
{
    bool status;
};

struct MCUVerInfoRequest
{
    uint8_t type;
};

// 心跳控制数据结构
struct MCUHeartControl
{
    uint8_t  mode;                // 控制模式：1-固定心率模式，0-PWM 模式
    uint16_t heart;               // 心率模式：心率值 [30-150]；PWM 模式：心跳强度 [0-1600]
    uint16_t time;                // 心率模式：第一短时间 (μs)；PWM 模式：无实际意义
};

struct MCUHeartControlReply
{
    uint8_t  mode;                // 控制模式：1-固定心率模式，0-PWM 模式
    uint8_t  code;                // 错误码：0-成功，!0-失败
    int32_t  code_int32;          // 错误码 (转换为 int32 用于 ROS 消息)
};

}  // namespace stark_power_manager
// clang-format on
