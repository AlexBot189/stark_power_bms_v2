#include <chrono>
#include <fcntl.h>
#include <termios.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <fstream>
#include "CRC8.hpp"
#include "UartDispatcher.hpp"
#include "utility/McuUartType.hpp"
#include "3rd_party/nlohmann/json.hpp"
#include "drivers/gpio_touch/TouchMonitorAdapter.hpp"
#include <boost/filesystem.hpp>

using nJson = nlohmann::json;
using namespace stark_power_manager;

#define PROCESS_RX_DATA()                                                                \
    if (MAX_RX_LEN - m_iRxStart <= MIN_PKG_LEN || MAX_RX_LEN == m_iRxTop)                \
    {                                                                                    \
        bcopy(m_RxBuffer.data() + m_iRxStart, m_RxBuffer.data(), m_iRxTop - m_iRxStart); \
        m_iRxTop = m_iRxStart ? m_iRxTop - m_iRxStart : 0;                               \
        m_iRxStart = 0;                                                                  \
    }

UartDispatcher::UartDispatcher(const UartOption& uartOption)
    : UartDispatcher(uartOption, nullptr)
{
}

UartDispatcher::UartDispatcher(const UartOption& uartOption, std::shared_ptr<ros::NodeHandle> nh)
    : m_rosNh(nh)
    , m_RxBuffer(MAX_RX_LEN)
    , m_uartOption(uartOption)
{
    InitCrc8();
    InitDispatcher();
}

UartDispatcher::~UartDispatcher()
{
    DestroyDispatcher();
}

int
UartDispatcher::GetActualBaudRate(speed_t baud) {
    switch(baud) {
        case B0:      return 0;
        case B50:     return 50;
        case B75:     return 75;
        case B110:    return 110;
        case B134:    return 134;
        case B150:    return 150;
        case B200:    return 200;
        case B300:    return 300;
        case B600:    return 600;
        case B1200:   return 1200;
        case B1800:   return 1800;
        case B2400:   return 2400;
        case B4800:   return 4800;
        case B9600:   return 9600;
        case B19200:  return 19200;
        case B38400:  return 38400;
        case B57600:  return 57600;
        case B115200: return 115200;
        // 如果还有更多波特率需要支持，可以继续添加
        default:      return -1; // 未知的波特率
    }
}

bool
UartDispatcher::InitDispatcher()
{
    if (InitUart() == false)
    {
        ECO_ERROR("Uart Init failed");
        DestroyDispatcher();

        return false;
    }

    ECO_INFO("Open %s:%d success!", m_uartOption.strTty.c_str(), GetActualBaudRate(m_uartOption.iBaudRate));

    m_bIsRun = true;

    m_sendThread = std::thread(&UartDispatcher::SendThreadFunc, this);
    m_recvThread = std::thread(&UartDispatcher::RecvThreadFunc, this);
//    m_stationCommThread = std::thread(&UartDispatcher::StationCommThreadFunc,this);
    // m_heartThread = std::thread(&UartDispatcher::HeartThreadFunc, this);
    return true;
}

bool
UartDispatcher::DestroyDispatcher()
{
    if (m_heartThread.joinable())
    {
        m_heartThread.join();
    }

    if (m_recvThread.joinable())
    {
        m_recvThread.join();
    }

    if (m_sendThread.joinable())
    {
        m_sendThread.join();
    }

    m_bIsRun.store(false);
    CloseUart();
    return true;
}

void
UartDispatcher::Send(const std::string& data)
{
    m_sendQueue.enqueue(data);
}

bool
UartDispatcher::SendData(const std::string& data)
{
    if (m_fd == -1)
    {
        ECO_ERROR("Serial port fd = -1!");
        return false;
    }

    int len = write(m_fd, data.c_str(), data.size());
    if (len < 0)
    {
        ECO_ERROR_STREAM("Error from write: " << strerror(errno));
        return false;
    }

    return true;
}

// TODO(colin): 暂时没用到，重载
std::string
UartDispatcher::OnRecvData()
{
    return "";
}

void
UartDispatcher::RegisterObserver(ListenerType type, std::shared_ptr<IListener> listener)
{
    auto it = m_Listeners.find(type);

    if (it == m_Listeners.end())  // key 不存在
    {
        m_Listeners.insert(std::make_pair(type, listener));
    }
    else  // key 已经存在
    {
        ECO_WARN("Observer already exists!");
    }
}

void
UartDispatcher::RemoveObserver(ListenerType type, std::shared_ptr<IListener> listener)
{
    auto it = m_Listeners.find(type);
    if (it != m_Listeners.end())
    {
        m_Listeners.erase(it);
    }
}

void
UartDispatcher::NotifyObserver(const boost::any& data)
{
    for (auto it = m_Listeners.begin(); it != m_Listeners.end(); ++it)
    {
        it->second->Update(data);
    }
}

bool
UartDispatcher::InitUart()
{
    struct termios tty;

    /**< 检查串口访问权限 */
    if (access(m_uartOption.strTty.c_str(), R_OK | W_OK) != 0)
    {
        ECO_ERROR("No access permission to the UART file:%s\n", m_uartOption.strTty.c_str());
        return false;
    }

    // 初始化串口
    m_fd = open(m_uartOption.strTty.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0)
    {
        ECO_WARN("Open uart error. %s:%d", m_uartOption.strTty.c_str(), GetActualBaudRate(m_uartOption.iBaudRate));
        return false;
    }
    // 置设备 8 -> N -> 1
    if (tcgetattr(m_fd, &tty) < 0)
    {
        ECO_ERROR("tcgetattr error");
        return false;
    }

    fcntl(m_fd, F_SETFL, 0);
    tty.c_cflag |= (tcflag_t)(CLOCAL | CREAD | CS8);                                      // 数据位为8
    tty.c_cflag &= (tcflag_t) ~(CSTOPB | PARENB);                                         // 关闭奇偶校验，停止位为1
    tty.c_lflag &= (tcflag_t) ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);  //|ECHOPRT 禁用规范模式和回显
    tty.c_oflag &= (tcflag_t) ~(OPOST);                                                   // 禁用特殊的输出处理
    tty.c_iflag &= (tcflag_t) ~(IXON | IXOFF | INLCR | IGNCR | ICRNL | IGNBRK);  // 禁用软件流控制和输入处理

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 15;
    tty.c_cflag &= ~CRTSCTS;

    cfsetispeed(&tty, m_uartOption.iBaudRate);
    cfsetospeed(&tty, m_uartOption.iBaudRate);

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0)
    {
        ECO_ERROR("Failed to set serial port attributes");
        return false;
    }

    // 清空缓冲
    // tcflush(m_fd, TCIFLUSH);
    // tcflush(m_fd, TCOFLUSH);
    tcflush(m_fd, TCIOFLUSH);
    ECO_INFO("uart init success ");
    return true;
}

int
UartDispatcher::CloseUart()
{
    tcflush(m_fd, TCIOFLUSH);

    // 0：表示关闭操作成功；
    // 1：表示传入的参数无效；
    // 2：表示设备不可用或己经被打开；
    // 3：表示设备未打开，无法进行关闭操作；
    // 4：表示关闭操作失败，可能是设备已经被移除；
    // 5：表示操作被中断
    m_fd = m_fd > 0 ? close(m_fd) : 0;

    if (m_fd > 0)
    {
        ECO_ERROR("Close Uart failed, error code: %d", m_fd);
    }
    return m_fd;
}

void
UartDispatcher::HeartThreadFunc()
{
    prctl(PR_SET_NAME, "mcu_heart");
    ECO_INFO("Thread started");

    // TODO(colin): 实现心跳
    // while (m_bIsRun)
    // {
    //     sleep(1);
    // }

    ECO_INFO("Thread exit");
}

void
UartDispatcher::RecvThreadFunc()
{
    prctl(PR_SET_NAME, "uart_recv");
    ECO_INFO("Thread started");

    UartFrame frame;
    UartFramePkg pkg;

    bool isOk = false;
    uint8_t pos = 0;

    while (m_bIsRun)
    {
        // ECO_WARN("recv msg...");

        int nBytes = 0;
        ioctl(m_fd, FIONREAD, &nBytes);
        if (nBytes > 0)
        {
            // 有数据，清空串口缓冲区
            // tcflush(m_fd, TCIOFLUSH);
        }

        if (DataWait() <= 0)
        {
            PROCESS_RX_DATA();
            continue;
        }

        // TODO(colin): ota

        for (; m_iRxStart + MIN_PKG_LEN <= m_iRxTop; m_iRxStart += pos)
        {
            //< 查找帧头 {0x60, 0x53, 0x41}: <`><SA>
            if (!frame.UartFrameFindHead(m_RxBuffer.data() + m_iRxStart, m_iRxTop - m_iRxStart, pos))
            {
                if (pos == 0)
                {
                    // 数据量不足一帧
                    PROCESS_RX_DATA();
                    // break;
                }
                else
                {
                    // 没有找到报头
                    ECO_WARN("Message not found header 0x60,0x53,0x41 pos: %d", pos);
                    continue;
                }
            }

            m_iRxStart += pos;
            //  找到报头及其相对start的偏移量 use
            pos = frame.UartFrameUnpack(m_RxBuffer.data() + m_iRxStart, m_iRxTop - m_iRxStart, pkg, isOk);
            if (pos == 0)
            {
                // 计算的出的帧长大于当前数据量，数据量不足
                PROCESS_RX_DATA();
                break;
            }
            if (isOk == true)
            {
                OnRecvData(pkg);
            }
        }
        // 检查缓冲数据是否使用完毕，完毕则全部归零
        if (m_iRxStart >= m_iRxTop)
        {
            m_iRxStart = 0;
            m_iRxTop = 0;
            continue;
        }
    }
    ECO_INFO("Thread exit");
}

int
UartDispatcher::DataWait()
{
    int ret = 0;
    fd_set read_fds;
    struct timeval tv = { 0, 15000 };

    FD_ZERO(&read_fds);
    FD_SET(m_fd, &read_fds);

    ret = select(m_fd + 1, &read_fds, (fd_set*)0, (fd_set*)0, ((m_eStatus == McuState::MCU_OK) ? &tv : NULL));
    if (ret < 0)
    {
        ECO_WARN("Failed to select data from serial port");
    }
    else if (ret == 0)
    {
        if (!m_iCollide)
        {
            ECO_WARN("Wait mcu data time out, status = %d", m_eStatus);
        }
        m_iCollide++;
        return -1;
    }

    if (m_iCollide)
    {
        ECO_WARN("Status = %d, data resume time : %d * 15ms", m_eStatus, m_iCollide.load());
        m_iCollide = 0;
    }

    if (FD_ISSET(m_fd, &read_fds) <= 0)
    {
        ECO_ERROR("FD_ISSET error");
        return -1;
    }

    int len = read(m_fd, m_RxBuffer.data() + m_iRxTop, MAX_RX_LEN - m_iRxTop);
    if (len <= 0)
    {
        ECO_WARN("Read data error , top: %d, start: %d ", m_iRxTop, m_iRxStart);
        return len;
    }

    m_iRxTop += len;
    return len;
}

void
UartDispatcher::OnRecvData(const UartFramePkg& pkg)
{
    // ECO_INFO("cmd&sub_cmd = '%c%c'", pkg.cmd, pkg.sub_cmd);

    /**< imu */
    if (pkg.cmd == 'G' && pkg.sub_cmd == 'D')
    {
        DecodeImuPackMsg(pkg);
    }
    /**< rtc时间同步 */
    else if (pkg.cmd == 'R' && pkg.sub_cmd == 'C')
    {
        DecodeRtcPackMsg(pkg);
    }
    /**< 开关量信息*/
    else if (pkg.cmd == 'B' && pkg.sub_cmd == 'C')
    {
        DecodeOnOffMsg(pkg);
    }
    /**<MCU按键信息*/
    else if (pkg.cmd == 'K' && pkg.sub_cmd == 'A')
    {
        DecodePhyKeyMsg(pkg);
    }
    /**<电池信息*/
    else if(pkg.cmd == 'C' && pkg.sub_cmd == 'C')
    {
        DecodeBattInfoMsg(pkg);
    }
    /*response:motor control*/
    else if(pkg.cmd == 'M' && pkg.sub_cmd == 'A')
    {
        DecodeMotorCtrlRspMsg(pkg);
    }
    /* 心跳控制回复 */
    else if(pkg.cmd == 'M' && pkg.sub_cmd == 'C')
    {
        DecodeHeartControlReplyMsg(pkg);
    }
    else if(pkg.cmd == 'S' && pkg.sub_cmd == 'M')
    {
        DecodeServoCtrlRspMsg(pkg);
    }
    /*触摸信息*/
    else if(pkg.cmd == 'S' && pkg.sub_cmd == 'T')
    {
        DecodeTouchMsg(pkg);
    }/**<MCU版本号>*/
    else if(pkg.cmd == 'V' && pkg.sub_cmd == 'B')
    {
        DecodeMCUVerionMsg(pkg);
    }
    else if(pkg.cmd == 'V' && pkg.sub_cmd == 'P')
    {
        DecodeMCUOtaMsg(pkg);
    }
    else if(pkg.cmd == 'S' && pkg.sub_cmd == 'C') //组建电流
    {
        DecodeMCUCurrentMsg(pkg);
    }
    else if(pkg.cmd == 'S' && pkg.sub_cmd == 'D') //组建温度
    {
        DecodeMCUTempMsg(pkg);
    }
    else if(pkg.cmd == 'S' && pkg.sub_cmd == 'E') //异常码
    {
        DecodeMCUEcMsg(pkg);
    }
    else if(pkg.cmd == 'G' && pkg.sub_cmd == 'P') //陀螺仪姿态识别上报
    {
        DecodeMCUGyroPoseMsg(pkg);
    }
    else if (pkg.cmd == 'F' && pkg.sub_cmd == 'R')
    {
        DecodeFactoryResetMsg(pkg);
    }
    else if (pkg.cmd == 'P' && pkg.sub_cmd == 'C') //power ctl rsp
    {
        DecodePowerCtlMsg(pkg); //功率控制mcu消息返回
    }
    else if (pkg.cmd == 'F' && pkg.sub_cmd == 'S')
    {
        DecodeAgingTestMsg(pkg); //老化测试
    }
}

void
UartDispatcher::DecodeImuPackMsg(const UartFramePkg& pkg)
{
    static uint8_t count = 0;
    static bool initOnce = false;
    static ImuData last_imu_data;
    static float last_yaw_sec = 0;
    static std::chrono::time_point<std::chrono::steady_clock> lastTime = std::chrono::steady_clock::now();

    //< do decode
    uint8_t* p = (uint8_t*)pkg.payload.data();
    int16_t yaw_angle = static_cast<int16_t>(p[1] | p[2] << 8);//Z轴
    uint16_t roll_angle = static_cast<uint16_t>(p[3] | p[4] << 8);//X轴
    uint16_t pitch_angle = static_cast<uint16_t>(p[5] | p[6] << 8);//Y轴
    uint16_t pitch_angle_sec = static_cast<uint16_t>(p[7] | p[8] << 8);
    int16_t gx = static_cast<int16_t>(p[9] | p[10] << 8);
    int16_t gy = static_cast<int16_t>(p[11] | p[12] << 8);
    int16_t gz = static_cast<int16_t>(p[13] | p[14] << 8);


    if (!initOnce)
    {
        count++;
        if (count > 20)
        {
            initOnce = true;
            count = 0;
        }
        lastTime = std::chrono::steady_clock::now();
        last_imu_data.yaw = (pitch_angle_sec / 100.0);
        last_imu_data.pitch = (pitch_angle / 100.0);
        last_imu_data.roll = (roll_angle / 100.0);
        p = nullptr;
        return;
    }
    ImuData imuData;
    imuData.yaw = (pitch_angle_sec / 100.0);
    imuData.pitch = (pitch_angle / 100.0);
    imuData.roll = (roll_angle / 100.0);
    float yaw_sec = (pitch_angle_sec / 100.0);  //< 不可靠

    //< 计算角速度
    auto delta_time_s =
        (double)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - lastTime).count()
                 / 1e6);
    imuData.ax = gx;
    imuData.ay = gy;
    imuData.az = gz;
    imuData.vx = (imuData.roll - last_imu_data.roll) / delta_time_s;
    imuData.vy = (imuData.pitch - last_imu_data.pitch) / delta_time_s;
    imuData.vz = (imuData.yaw - last_imu_data.yaw) / delta_time_s;

 //   ECO_INFO_NEW("BodyIMuData gx:{},gy:{},gz:{}",imuData.ax, imuData.ay, imuData.az);
    ECO_TRACE_NEW("BodyIMuData gx:{},gy:{},gz:{}",imuData.ax, imuData.ay, imuData.az);
    // ECO_LOG_THROTTLE(5000, "5s delta_time = {}, imu_yaw: {}, pitch: {}, roll: {}, yaw_sec: {}, vx: {}, vy: {} , vz: {}",
    //                  delta_time_s, imuData.yaw, imuData.pitch, imuData.roll, yaw_sec, imuData.vx, imuData.vy, imuData.vz);
    ECO_TRACE_NEW("5s delta_time = {}, imu_yaw: {}, pitch: {}, roll: {}, yaw_sec: {}, vx: {}, vy: {} , vz: {}",
                     delta_time_s, imuData.yaw, imuData.pitch, imuData.roll, yaw_sec, imuData.vx, imuData.vy, imuData.vz);
    if (fabs(imuData.yaw - last_imu_data.yaw) > 0.1)
    {
        ECO_DEBUG("last_imu: delta_time = %f, imu_yaw: %f, pitch: %f, roll: %f, yaw_sec: %f, vx: %f, vy: %f , vz: %f",
                 delta_time_s, last_imu_data.yaw, last_imu_data.pitch, last_imu_data.roll, last_yaw_sec, last_imu_data.vx,
                 last_imu_data.vy, last_imu_data.vz);
        ECO_DEBUG("current_imu:delta_time = %f, imu_yaw: %f, pitch: %f, roll: %f, yaw_sec: %f, vx: %f, vy: %f , vz: %f",
                 delta_time_s, imuData.yaw, imuData.pitch, imuData.roll, yaw_sec, imuData.vx, imuData.vy, imuData.vz);
    }

    lastTime = std::chrono::steady_clock::now();
    last_imu_data = imuData;
    last_yaw_sec = yaw_sec;
    NotifyObserver(imuData);
    m_imuData = imuData;
    p = nullptr;
}

#define MDS_ONOFF_INDEX_COLLIDE 0           //碰撞
#define MDS_ONOFF_INDEX_CLIFF 1             //下视
#define MDS_ONOFF_INDEX_FALL 2              //跌落
#define MDS_ONOFF_INDEX_DUST_BOX 3          //尘盒检测
#define MDS_ONOFF_INDEX_RUG 4               //地毯检测
#define MDS_ONOFF_INDEX_CHARGE 8            //充电状态
#define MDS_ONOFF_INDEX_MOP 12              //拖布状态
#define MDS_ONOFF_INDEX_STATION_EXP 13      //基站异常状态
#define MDS_ONOFF_INDEX_STATION_WORK 14     //基站工作状态
#define MDS_ONOFF_INDEX_STATION_COMM 15     //基站通信状态
#define MDS_ONOFF_INDEX_DUST_COLL 16        //集尘次数
void
UartDispatcher::DecodeOnOffMsg(const UartFramePkg& pkg)
{
    u_int8_t idx, state;
    u_int8_t* p = (uint8_t*)pkg.payload.data();

    for (int i = 0; i < pkg.payload.size(); i += 2)
    {
        idx = *(p + i);
        state = *(p + i + 1);

        switch (idx)
        {
        case MDS_ONOFF_INDEX_COLLIDE:
        {
            // BumpMsg bumpMsg = {
            //     .leftBump = 0, .frontLeftBump = 0, .frontRightBump = 0, .rightBump = 0, .ldsLeftBump = 0, .ldsRightBump = 0
            // };

            //bit2(lds bump)|bit1(right bump)|bit0(left bump)
            m_mCuOnOffStatus.collide_detect_status = (state & 0x07);

            // NotifyObserver(mCuOnOffStatus);
            break;
        }
        case MDS_ONOFF_INDEX_CLIFF:
        {
            // bit3(right)| bit2(front right)| bit1(front left)| bit0(left)
            m_mCuOnOffStatus.cliff_detect_status = (state & 0x0F);
            break;
        }
        case MDS_ONOFF_INDEX_FALL:
        {
            //bit1(right)| bit0(left)
            m_mCuOnOffStatus.dangling_detect_status = (state & 0x03);
            break;
        }
        case MDS_ONOFF_INDEX_DUST_BOX:
        {
            //bit0:尘盒安装检测(1-安装 0-未安装)
            //bit1:尘盒光电检测(1-触发 0-未触发)
            //bit2:一次性尘袋尘盒盖检测1(1-安装 0-未安装)
            //bit3:一次性尘袋尘盒盖检测2(1-安装 0-未安装)
            m_mCuOnOffStatus.dust_box_pos = (state & 0x0F);
            break;
        }
        case MDS_ONOFF_INDEX_RUG:
        {
            //0 – 非地毯，1 – 地毯
            m_mCuOnOffStatus.rug_detect_status = (state & 0x01);
            break;
        }
        case MDS_ONOFF_INDEX_CHARGE:
        {
            // 充电检测 0 – 未充电，1 – 充电中
            m_mCuOnOffStatus.charge_status = (state & 0x01);
            break;
        }
        case MDS_ONOFF_INDEX_MOP:
        {
            //Bit0~Bit1:安装检测(0-未安装 1-已安装) Bi2~Bit3:抬升到位检测(1-抬升到位 0-不到位)
            //Bit4:外扩电机归位是否到位(1-到位 0-未到位)Bit5:电机外扩是否到位(1-到位 0-未到位)
            //bit5(外扩)|bit4(归位)|bit3(左)|bit2(右)|bit1(左)|bit0(右)
            m_mCuOnOffStatus.mop_status = (state & 0x3F);
            break;
        }
        case MDS_ONOFF_INDEX_STATION_EXP:
        {
            /**
             * bit4:烘干电机异常状态(1-异常 0-正常) bit3:清洁槽溢水状态(1-溢水 0-正常)
             * bit2:污水箱安装(1-已安装 0-未安装) bit1:清水箱安装(1-已安装 0-未安装)
             * bit0:集尘袋安装(1-已安装 0-未安装)
            */
            m_mCuOnOffStatus.station_exception_status = (0x1F & state);
            break;
        }
        case MDS_ONOFF_INDEX_STATION_WORK:
        {
            /**
             * bit6:灯效开关(1-开启 0-关闭) bit5:热水加热器状态(1-开启 0-关闭)
             * bit4:烘干加热器(1-开启 0-关闭) bit3:集尘风机(1-开启 0-关闭)
             * bit2:风干电机(1-开启 0-关闭) bit1:污水电机(1-开启 0-关闭)
             * bit0:清水电机(1-开启 0-关闭)
            */
            m_mCuOnOffStatus.station_work_status = (0x7F & state);
            break;
        }
        case MDS_ONOFF_INDEX_STATION_COMM:
        {
            //基站通信状态 0x00-未通信 0x01-正常通信 0x02-通信异常
            m_mCuOnOffStatus.station_comm_status = (0x03 & state);//取2bit
            break;
        }
        case MDS_ONOFF_INDEX_DUST_COLL:
        {
            //集尘次数
            m_mCuOnOffStatus.dust_collect_count = state;
            break;
        }
        default:
            break;
        }
    }

    NotifyObserver(m_mCuOnOffStatus);
}

void
UartDispatcher::DecodePhyKeyMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUPhyKey key;

    key.key_type = pData[0];
    key.key_event = pData[1];

    ECO_INFO("key type:%d,key event:%d\r\n", key.key_type, key.key_event);

    NotifyObserver(key);

#if 1 //按键进配网模式
    if(key.key_type == 0 && key.key_event == 1)//5s内短按3次
    {
	system("/etc/wifi/start_ap.sh");
    }
#endif
    m_phyKey = key;
}

void
UartDispatcher::DecodeBattInfoMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUBatteryStatus mCuBatteryStatus;

    mCuBatteryStatus.battery_percent = pData[0];
    mCuBatteryStatus.charge_state = pData[1];
    //pData[2]:低8bit，pData[3]:高8bit
    mCuBatteryStatus.battery_voltage = static_cast<uint16_t>(pData[3] << 8 | pData[2]);
    mCuBatteryStatus.battery_temp = pData[4];
    mCuBatteryStatus.charge_voltage = static_cast<uint16_t>(pData[6] << 8 | pData[5]);
    mCuBatteryStatus.charge_current = static_cast<uint16_t>(pData[8] << 8 | pData[7]);
    mCuBatteryStatus.charge_type = pData[9];

    NotifyObserver(mCuBatteryStatus);

//    ECO_TRACE_NEW("Report Battery Info every 10s, battery_percent:{}%, charge_status:{}, battery_voltage:{}mV, battery_temp:{}, charge_voltage{}mV\r\n", mCuBatteryStatus.battery_percent,
    ECO_DEBUG("Report Battery Info every 10s, battery_percent:%d, charge_status:%d, battery_voltage:%dmV, battery_temp:%d, charge_voltage:%dw, charge_current%dmA, charge_type:%d\r\n", mCuBatteryStatus.battery_percent,
             mCuBatteryStatus.charge_state, (mCuBatteryStatus.battery_voltage*10), mCuBatteryStatus.battery_temp, mCuBatteryStatus.charge_voltage, mCuBatteryStatus.charge_current, mCuBatteryStatus.charge_type);
}

void
UartDispatcher::DecodeMotorCtrlRspMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUMotorControlReply rpy;

    rpy.task_id = pkg.id;
    rpy.status = true;

    ECO_DEBUG("MotorCtrl response id:%d,status:%d\r\n", rpy.task_id, rpy.status);

    NotifyObserver(rpy);
}

void
UartDispatcher::DecodeHeartControlReplyMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUHeartControlReply reply;
    
    reply.mode = pData[0];
    reply.code = pData[1];
    reply.code_int32 = static_cast<int32_t>(reply.code);

    ECO_INFO("Heart Control Reply: mode=%d, code=%d (0=success, !0=fail)\r\n", 
             reply.mode, reply.code);

    NotifyObserver(reply);
}

void
UartDispatcher::DecodeTouchMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUTouchControl mCuTouchControl;

    mCuTouchControl.touch_part  = pData[0]; 
    mCuTouchControl.touch_mode  = pData[1]; 
    mCuTouchControl.touch_force = pData[2]; 
    mCuTouchControl.touch_ch = pData[3]; 

    ECO_INFO("mCuTouchControl touch_part:%02X,touch_mode:%02X,touch_force:%02X, touch_ch:%02X\r\n", mCuTouchControl.touch_part, mCuTouchControl.touch_mode, mCuTouchControl.touch_force, mCuTouchControl.touch_ch);

    NotifyObserver(mCuTouchControl);
}

void
UartDispatcher::DecodeServoCtrlRspMsg(const UartFramePkg& pkg)
{
  //< do decode
    auto pData = pkg.payload.data();
//MCU大端：
#if 1
    uint16_t Servo_0 = static_cast<uint16_t>(pData[1] | pData[2] << 8);
    uint16_t Servo_1 = static_cast<uint16_t>(pData[3] | pData[4] << 8);
    uint16_t Servo_2 = static_cast<uint16_t>(pData[5] | pData[6] << 8);
    uint16_t Servo_3 = static_cast<uint16_t>(pData[7] | pData[8] << 8);
#endif

    ECO_TRACE_NEW("ServoCtrl response Servo_0:{},Servo_1:{},Servo_2:{},Servo_3:{}\r\n", Servo_0, Servo_1, Servo_2, Servo_3);
   
    std::vector<ServoData> data = {
	{0x0, Servo_0},
	{0x1, Servo_1},
	{0x2, Servo_2},
	{0x3, Servo_3}
    };

    NotifyObserver(data);
}

void
UartDispatcher::DecodeMCUVerionMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUVersionInfo versionInfo;

    versionInfo.version_type = pData[0];
    memcpy(versionInfo.version, &pData[1], 5);

    ECO_INFO("MCU Version Type: 0x%02X, Version: %.5s\r\n", versionInfo.version_type, versionInfo.version);

    // 写入 /tmp/boardversion
    std::ofstream ofs("/tmp/boardversion");
    if (ofs.is_open()) {
        ofs.write(reinterpret_cast<const char*>(versionInfo.version), 5);
	ofs << "\n";
        ofs.close();
    } else {
        ECO_ERROR("Failed to open /tmp/boardversion for writing\n");
    }

//    system("sed -i 's/\"mcu_ver\": *\"[^\"]*\"/\"mcu_ver\": \"'$(cat /tmp/boardversion)'\"/' /etc/fw.manifest");

    NotifyObserver(versionInfo);
}

void
UartDispatcher::DecodeMCUOtaMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUOtaRsp OtaMsg;

    OtaMsg.ota_flag = pData[0];

    ECO_INFO_NEW("recv MCU OTA Flag: {}", OtaMsg.ota_flag);

    NotifyObserver(OtaMsg);
}

void
UartDispatcher::DecodeMCUCurrentMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUCurrentInfo CurrentInfo;

    CurrentInfo.tailMotor = pData[0];
    CurrentInfo.tailMotorCur = static_cast<uint16_t>(pData[2] << 8 | pData[1]);

    CurrentInfo.heater = pData[3];
    CurrentInfo.heaterCur = static_cast<uint16_t>(pData[5] << 8 | pData[4]);

    CurrentInfo.rollServo = pData[6];
    CurrentInfo.rollServoCur = static_cast<uint16_t>(pData[8] << 8 | pData[7]);

    CurrentInfo.yawServo = pData[9];
    CurrentInfo.yawServoCur = static_cast<uint16_t>(pData[11] << 8 | pData[10]);

    CurrentInfo.pitchServo = pData[12];
    CurrentInfo.pitchServoCur = static_cast<uint16_t>(pData[14] << 8 | pData[13]);

    ECO_DEBUG_NEW("MCU CurrentInfo tailMotor:{} tailMotorCur:{}, heater:{} heaterCur:{}, rollServo:{} rollServoCur:{}, yawServo:{} yawServoCur:{}, pitchServo:{} pitchServoCur:{} ", CurrentInfo.tailMotor, CurrentInfo.tailMotorCur, CurrentInfo.heater, CurrentInfo.heaterCur, CurrentInfo.rollServo, CurrentInfo.rollServoCur, CurrentInfo.yawServo, CurrentInfo.yawServoCur, CurrentInfo.pitchServo, CurrentInfo.pitchServoCur);

    NotifyObserver(CurrentInfo);
}

void
UartDispatcher::DecodeMCUTempMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUTempInfo TempInfo;
 
    TempInfo.env = pData[0];
    TempInfo.envTemp_value = static_cast<uint16_t>(pData[2] << 8 | pData[1]);

    TempInfo.heaterLeft = pData[3];
    TempInfo.heaterLeftTemp_value = static_cast<uint16_t>(pData[5] << 8 | pData[4]);

    TempInfo.heaterRight = pData[6];
    TempInfo.heaterRightTemp_value = static_cast<uint16_t>(pData[8] << 8 | pData[7]);

    TempInfo.rollServo = pData[9];
    TempInfo.rollServoTemp_value = static_cast<uint16_t>(pData[11] << 8 | pData[10]);

    TempInfo.yawServo = pData[12];
    TempInfo.yawServoTemp_value = static_cast<uint16_t>(pData[14] << 8 | pData[13]);

    TempInfo.pitchServo = pData[15];
    TempInfo.pitchServoTemp_value = static_cast<uint16_t>(pData[17] << 8 | pData[16]);

    ECO_DEBUG_NEW("MCU TempInfo env:{} envTemp_value:{}, heaterLeft:{} heaterLeftTemp_value:{}, heaterRight:{} heaterRightTemp_value:{}, rollServo:{} rollServoTemp_value:{}, yawServo:{} yawServoTemp_value:{}, pitchServo:{} pitchServoTemp_value:{} ", TempInfo.env, TempInfo.envTemp_value, TempInfo.heaterLeft, TempInfo.heaterLeftTemp_value, TempInfo.heaterRight, TempInfo.heaterRightTemp_value, TempInfo.rollServo, TempInfo.rollServoTemp_value, TempInfo.yawServo, TempInfo.yawServoTemp_value, TempInfo.pitchServo, TempInfo.pitchServoTemp_value);

    NotifyObserver(TempInfo);
}

void
UartDispatcher::DecodeMCUEcMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUEcInfo EcInfo;

    EcInfo.reserved = pData[0]; //预留字段
    EcInfo.Ex_type = pData[1]; //异常类型0-可恢复，1-不可恢复(用户检查),2-不可恢复(售后)
    EcInfo.Ex_type += 1; //对应ros消息定义1,2,3

    EcInfo.Ex_status = pData[2]; //0-异常解除，1-异常出现
    EcInfo.Ex_Code = static_cast<uint16_t>(pData[4] << 8 | pData[3]);//异常码

    ECO_INFO_NEW("MCU EcInfo reserved:{}, Ex_type:{}, Ex_status:{}, Ex_Code:{}", EcInfo.reserved, EcInfo.Ex_type, EcInfo.Ex_status, EcInfo.Ex_Code);

    NotifyObserver(EcInfo);
}

void
UartDispatcher::DecodeFactoryResetMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    FactoryResetInfo cmdInfo;

    cmdInfo.cmd = pData[0]; //1-恢复出厂设置

    ECO_INFO_NEW("MCU set FactoryReset CMD :{} ", cmdInfo.cmd);

    system("/usr/bin/fct_reset.sh &");

    NotifyObserver(cmdInfo);
}

void
UartDispatcher::DecodePowerCtlMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUPowerRsp pInfo;

    pInfo.code = pData[0];
    pInfo.power = pData[1];

    ECO_INFO_NEW("MCU power rsp, code={}, power={}", pInfo.code, pInfo.power);

    NotifyObserver(pInfo);
}

void
UartDispatcher::DecodeAgingTestMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    int fileCmdResult = -1;
    int procCmdResult = -1;

    UartFramePkg rspPkg;
    rspPkg.type = CMD_TYPE_REQUEST;
    rspPkg.id = 0;
    rspPkg.cmd = 0;
    rspPkg.sub_cmd = 0;
    rspPkg.payload.clear();

    //00-退出老化,01-进入老化,02-查询老化结果
    switch(pData[0])
    {
        case 0:
        {
            //强制关闭aging_test进程
            procCmdResult = std::system("killall aging_test");//使用默认SIGTERM信号,保证aging_test里面能执行退出操作

            //回复mcu
            rspPkg.cmd = 'F';
            rspPkg.sub_cmd = 'S';
            rspPkg.payload.push_back(0);//不论成功失败都直接返回0

            ECO_INFO_NEW("exit aging test proc:{}",procCmdResult);
        }
        break;
        case 1://创建/tmp/aging_test,启动aging_test进程
        {
            boost::filesystem::path filePath("/tmp/aging_test");

            /*文件存在表示已经在测试,不重复启动*/
            if (boost::filesystem::exists(filePath))
            {
                rspPkg.cmd = 'A';
                rspPkg.sub_cmd = 'T';
                rspPkg.payload.push_back(1);
            }
            else
            {
                fileCmdResult = std::system("touch /tmp/aging_test");
                procCmdResult = -1;

                uint8_t agingTestTime = 60;//老化测试时间(单位min)

                if(pkg.payload.size() > 1)
                {
                    agingTestTime = pData[1];
                }

                std::string filepath1 = "/data/autostart/upper/usr/bin/aging_test";//路径1
                std::string filepath2 = "/usr/bin/aging_test";//路径2

                if(access(filepath1.c_str(),X_OK) == 0)//优先使用路径1
                {
                    std::string exeCmd = filepath1 + " " + std::to_string(agingTestTime) + " > /dev/null 2>&1 &";
                    procCmdResult = std::system(exeCmd.c_str());
                }
                else if(access(filepath2.c_str(),X_OK) == 0)
                {
                    std::string exeCmd = filepath2 + " " + std::to_string(agingTestTime) + " > /dev/null 2>&1 &";
                    procCmdResult = std::system(exeCmd.c_str());
                }
                else
                {
                    ECO_INFO_NEW("aging_test do not exist!!");
                }

                ECO_INFO_NEW("enter aging test time:{},filecmd:{},proccmd:{}",agingTestTime,fileCmdResult,procCmdResult);

                //回复mcu
                rspPkg.cmd = 'A';
                rspPkg.sub_cmd = 'T';

                if(fileCmdResult >= 0 && procCmdResult >= 0)
                {
                    //指令执行都成功返回1
                    rspPkg.payload.push_back(1);
                }
                else
                {
                    //指令执行失败返回0
                    rspPkg.payload.push_back(0);
                }
            }
        }
        break;
        case 2://解析/data/loop_test_result,将code发给mcu
        {
            //回复mcu
            rspPkg.cmd = 'B';
            rspPkg.sub_cmd = 'T';

            std::vector<uint8_t> payload;
            payload.clear();

            boost::filesystem::path filePath("/data/aging_test_result");

            /*确认目录文件是否存在*/
            if (!boost::filesystem::exists(filePath))
            {
                payload.push_back(0xFF);
                payload.push_back(0xFF);
            }
            else
            {
                std::ifstream ifs("/data/aging_test_result");
                std::string strJson;

                /*读取文件内容*/
                if (!ifs.is_open())
                {
                    payload.push_back(0xFF);
                    payload.push_back(0xFF);
                }
                else
                {
                    strJson = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                    ifs.close();

                    /*解析json*/
                    try
                    {
                        auto jsonObj = nlohmann::json::parse(strJson);

                        if(jsonObj.contains("code"))
                        {
                            uint16_t code = static_cast<uint16_t>(jsonObj["code"].get<int>());

                            payload.push_back(static_cast<uint8_t>(code & 0xFF));       // 低 8 位
                            payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));// 高 8 位
                        }
                        else
                        {
                            payload.push_back(0xFF);
                            payload.push_back(0xFF);
                            ECO_WARN_NEW("json no code!!");
                        }
                    }
                    catch (std::exception &ex)
                    {
                        payload.push_back(0xFF);
                        payload.push_back(0xFF);
                        ECO_WARN_NEW("parse fail:{}!!",ex.what());
                    }
                }
            }

            rspPkg.payload = payload;

            ECO_INFO_NEW("check aging test result:{}",(payload[0] | payload[1] << 8));
        }
        break;
        default:
        break;
    }

    if(rspPkg.cmd == 'A' || rspPkg.cmd == 'B' || rspPkg.cmd == 'F')
    {
        UartFrame frame;
        auto tx = frame.UartFrameBuild(rspPkg);

        Send(std::string(tx.begin(), tx.end()));
    }
}

void
UartDispatcher::DecodeMCUGyroPoseMsg(const UartFramePkg& pkg)
{
    auto pData = pkg.payload.data();

    MCUGyroPoseInfo PoseInfo;

    PoseInfo.static_pose = pData[0];
    PoseInfo.dynamic_pose = pData[1];

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()
           ).count();

    if (auto touch = m_touchAdapter.lock()) {
	    touch->UpdateDynamicPose(PoseInfo.dynamic_pose, now);
    }

    ECO_INFO_NEW("MCU GyroPose static_pose:{}, dynamic_pose:{}", PoseInfo.static_pose, PoseInfo.dynamic_pose); //PoseInfo.dynamic_pose 1 / 5表示有拍打

    NotifyObserver(PoseInfo);
}

void
UartDispatcher::DecodeRtcPackMsg(const UartFramePkg& pkg)
{
    if (pkg.payload.size() < 7)
    {
        ECO_ERROR("DecodeRtcPackMsg: payload length < 7");
        return;
    }

    const uint8_t* pData = pkg.payload.data();

    // [0-1]: year (little endian), [2]: month, [3]: day, [4]: hour, [5]: min, [6]: sec
    uint16_t year = static_cast<uint16_t>(pData[0] | (pData[1] << 8));
    uint8_t month = pData[2];
    uint8_t day = pData[3];
    uint8_t hour = pData[4];
    uint8_t minute = pData[5];
    uint8_t second = pData[6];

    ECO_INFO_NEW("Got RTC from MCU: {}-{}-{} {}:{}:{}",
                 year, month, day, hour, minute, second);
    
    struct tm gotTime {};
    gotTime.tm_year = year - 1900;
    gotTime.tm_mon  = month;
    gotTime.tm_mday = day;
    gotTime.tm_hour = hour;
    gotTime.tm_min  = minute;
    gotTime.tm_sec  = second;
    gotTime.tm_isdst = -1;

    time_t utcTime = mktime(&gotTime);
    if (utcTime == -1)
    {
        ECO_ERROR("mktime() failed to convert MCU RTC to time_t");
        return;
    }

    ECO_INFO_NEW("MCU RTC converted to UTC timestamp: {}", utcTime);

    // --- 设置系统时间 ---
    struct timeval tv {};
    tv.tv_sec = utcTime;
    tv.tv_usec = 0;

    if (settimeofday(&tv, nullptr) == 0)
    {
        ECO_INFO_NEW("Soc System time successfully updated from MCU RTC.");
    }
    else
    {
        ECO_ERROR("Failed to set system time via settimeofday()");
    }

    NotifyObserver(utcTime);
}

void
UartDispatcher::SendThreadFunc()
{
    prctl(PR_SET_NAME, "uart_send");
    ECO_INFO("Thread started");
    while (m_bIsRun)
    {
        std::string data;
        if (m_sendQueue.try_dequeue(data))
        {
            SendData(data);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ECO_INFO("Thread exit");
}

bool
UartDispatcher::SetMCUMotorControl(const std::vector<MCUMotorControl>& request,const uint32_t& task_id)
{
    UartFramePkg pkg;

    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = task_id;
    pkg.cmd = 'M';
    pkg.sub_cmd = 'A';

    std::vector<uint8_t> payload;

    payload.clear();

    for(auto it = request.begin();it != request.end();it++)
    {
        payload.push_back(it->index);
        payload.push_back(it->action);
        payload.push_back(static_cast<uint8_t>(it->content & 0xFF));//content低8bit
        payload.push_back(static_cast<uint8_t>(it->content >> 8));//content高8bit

	payload.push_back(static_cast<uint8_t>(it->time & 0xFF));//time低8bit
        payload.push_back(static_cast<uint8_t>(it->time >> 8));//time高8bit
        payload.push_back(it->cmdType);


        ECO_INFO_NEW("MotorCtrl index:{},action:{},content:{},time:{},cmdType:{}",it->index,it->action,it->content,it->time,it->cmdType);
    }

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));
    return true;
}

bool
UartDispatcher::SetMCUHeartControl(const MCUHeartControl& request, const uint32_t& task_id)
{
    UartFramePkg pkg;

    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = task_id;
    pkg.cmd = 'M';
    pkg.sub_cmd = 'C';

    std::vector<uint8_t> payload;
    payload.clear();

    payload.push_back(request.mode);
    payload.push_back(static_cast<uint8_t>(request.heart & 0xFF));           // 低 8 位
    payload.push_back(static_cast<uint8_t>(request.heart >> 8));             // 高 8 位
    payload.push_back(static_cast<uint8_t>(request.time & 0xFF));            // 低 8 位
    payload.push_back(static_cast<uint8_t>(request.time >> 8));              // 高 8 位

    ECO_INFO("Heart Control: mode=%d, heart=%d, time=%d", 
             request.mode, request.heart, request.time);

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));
    return true;
}

bool
UartDispatcher::SetMCUTempControl(const std::vector<MCUTempControl>& request,const uint32_t& task_id)
{
    UartFramePkg pkg;

    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = task_id;
    pkg.cmd = 'T';
    pkg.sub_cmd = 'K';

    std::vector<uint8_t> payload;

    payload.clear();

    for(auto it = request.begin();it != request.end();it++)
    {
        payload.push_back(it->index);
        payload.push_back(it->mode);
        payload.push_back(static_cast<uint8_t>(it->value & 0xFF));//temp低8bit
        payload.push_back(static_cast<uint8_t>(it->value >> 8));//temp高8bit

        ECO_INFO_NEW("TempCtrl index:{},mode:{},temp:{}",it->index,it->mode,it->value);
    }

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));
    return true;
}

bool
UartDispatcher::SetMCUPowerOffCtl(uint16_t delay_ms)
{
    UartFramePkg pkg;

    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = 0;
    pkg.cmd = 'P';         
    pkg.sub_cmd = 'O';   

    std::vector<uint8_t> payload;
    payload.clear();

    // 按低位在前，高位在后方式打包 delay_ms（小端序）
    payload.push_back(static_cast<uint8_t>(delay_ms & 0xFF));       // 低 8 位
    payload.push_back(static_cast<uint8_t>((delay_ms >> 8) & 0xFF));// 高 8 位

    ECO_INFO_NEW("SetMCUPowerOffCtl: delay_ms:{}", delay_ms);

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));
    return true;
}

bool
UartDispatcher::SetErpCtl(uint8_t mode)
{
    UartFramePkg pkg;

    pkg.type = CMD_TYPE_REQUEST;
    pkg.id   = 1;
    pkg.cmd  = 'C';
    pkg.sub_cmd = 'K';

    std::vector<uint8_t> payload;
    payload.clear();

    // ERP 开关控制：0=退出ERP，1=进入ERP 2-erp(0.5w) 3-erp(2w) 4-CEC, 5-DOE
    payload.push_back(mode);

    ECO_INFO_NEW("SetERPCtl: mode={}", mode);

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));
    
    if (mode == 3) {
        system("/usr/bin/erp.sh");
    } else if (mode == 2 || mode == 4 || mode == 5) {
        system("/usr/bin/poweroff.sh");
    }

    return true;
}

bool UartDispatcher::GetMCUVersionRequest(const uint32_t& task_id)
{
    UartFramePkg pkg;

    pkg.type    = CMD_TYPE_REQUEST;  // 请求帧类型
    pkg.id      = task_id;
    pkg.cmd     = 'V';               
    pkg.sub_cmd = 'C';               
    pkg.payload.clear();

    // 构造 UART 帧
    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    // 发送串口帧
    Send(std::string(tx.begin(), tx.end()));

    ECO_INFO_NEW("Sent MCU Version Request: cmd='V', sub_cmd='C', task_id:{}", task_id);

    return true;
}

bool UartDispatcher::SetMcuOTARequest(const uint32_t& task_id)
{
    UartFramePkg pkg;

    pkg.type    = CMD_TYPE_REQUEST;  // 请求帧类型
    pkg.id      = task_id;
    pkg.cmd     = 'V';               
    pkg.sub_cmd = 'P';               
    pkg.payload.clear();

    // 构造 UART 帧
    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    // 发送串口帧
    Send(std::string(tx.begin(), tx.end()));

    ECO_INFO_NEW("Sent MCU OTA Request: cmd='V', sub_cmd='P', task_id:{}", task_id);

    return true;
}

void UartDispatcher::SendTouchEventToMCU(uint8_t part, uint8_t force)
{
    UartFramePkg pkg;
    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = 0;
    pkg.cmd = 'F';//cmd:Factory
    pkg.sub_cmd = 'T';//sub_cmd:Touch

    std::vector<uint8_t> payload;
    payload.push_back(part);  // 触摸部位
    payload.push_back(force); // 状态 1=按下 0=松开

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));

    ECO_INFO_NEW("SendTouchEventToMCU part:{} state:{}", part, force);
}

void UartDispatcher::SendRTCRequestToMCU()
{
    UartFramePkg pkg;
    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = 0;
    pkg.cmd = 'R';
    pkg.sub_cmd = 'A';

    pkg.payload.clear();

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));

    ECO_INFO_NEW("SendRTCRequestToMCU: cmd='R' sub_cmd='A' req mcu time ...");
}

void UartDispatcher::SendSysTimeToMCU(time_t timestamp /* = 0 */)
{
    // 如果没有传入时间，则使用系统当前时间
    time_t now = (timestamp == 0) ? time(nullptr) : timestamp;
    struct tm *local = localtime(&now);

    uint16_t year = static_cast<uint16_t>(local->tm_year + 1900);
    uint8_t month = static_cast<uint8_t>(local->tm_mon); // MCU 那边 +1
    uint8_t day = static_cast<uint8_t>(local->tm_mday);
    uint8_t hour = static_cast<uint8_t>(local->tm_hour);
    uint8_t minute = static_cast<uint8_t>(local->tm_min);
    uint8_t second = static_cast<uint8_t>(local->tm_sec);

    UartFramePkg pkg;
    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = 0;
    pkg.cmd = 'R';
    pkg.sub_cmd = 'B';

    std::vector<uint8_t> payload;
    // 小端发送: 低字节在前
    payload.push_back(static_cast<uint8_t>(year & 0xFF));        // 低 8 位
    payload.push_back(static_cast<uint8_t>((year >> 8) & 0xFF)); // 高 8 位
    payload.push_back(month);
    payload.push_back(day);
    payload.push_back(hour);
    payload.push_back(minute);
    payload.push_back(second);

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);
    Send(std::string(tx.begin(), tx.end()));

    ECO_INFO_NEW("SendSysTimeToMCU: {}-{}-{} {}:{}:{} (timestamp={})",
                 year, month + 1, day, hour, minute, second, now);
}

void UartDispatcher::SetTouchAdapter(std::shared_ptr<TouchMonitorAdapter> touch_adapter)
{
    m_touchAdapter = touch_adapter;
}

bool
UartDispatcher::SetEyeControl(const std::vector<EyeControl>& request,const uint32_t& task_id)
{
    UartFramePkg pkg;

    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = task_id;
    pkg.cmd = 'M';
    pkg.sub_cmd = 'D';

    std::vector<uint8_t> payload;

    payload.clear();

    for (auto it = request.begin(); it != request.end(); it++)
    {
           payload.push_back(it->mode);
           payload.push_back(it->status);

           int16_t freq = it->freq;
           payload.push_back(static_cast<uint8_t>(freq & 0xFF));           // 低字节
           payload.push_back(static_cast<uint8_t>((freq >> 8) & 0xFF));      // 高字节

           int16_t count = it->count;
           payload.push_back(static_cast<uint8_t>(count & 0xFF));           // 低字节
           payload.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));      // 高字节

           ECO_INFO_NEW("EyeCtrl 2 mode:{},status:{},freq:{},count{}", it->mode, it->status, it->freq, it->count);
    }


    pkg.payload = payload;
    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));
    return true;
}

bool
UartDispatcher::SetCommonMotorControl(const MCUSetCommonMotorStatusRequest& request)
{
    return true;
}

bool
UartDispatcher::SetIRCalibrationValue()
{
    // if(ir_calibration_values.size() < 4)
    // {
    //     ECO_INFO("ir_calibration_values.size must be at least 4! got {}", ir_calibration_values.size());
    //     return false;
    // }
    // for(int device_id = 0; device_id < ir_calibration_values.size(); device_id++)
    // {
    //     ECO_INFO("set ir_id: {}, calibration_value: {}", device_id, ir_calibration_values[device_id]);
    //     ConfigSensorCalibrationData(CMD_CONFIG,device_id,ir_calibration_values[device_id]);
    // }
    return true;
}

bool
UartDispatcher::SetMCUPowerReq(const std::vector<MCUPower>& request,const uint32_t& task_id)
{
    UartFramePkg pkg;

    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = task_id;
    pkg.cmd = 'P';
    pkg.sub_cmd = 'C';

    std::vector<uint8_t> payload;

    payload.clear();

    for(auto it = request.begin();it != request.end();it++)
    {
     //   payload.push_back(it->type);
	payload.push_back(static_cast<uint8_t>(it->power & 0xFF));         // 第 1 字节：最低 8 位
    	payload.push_back(static_cast<uint8_t>((it->power >> 8) & 0xFF));  // 第 2 字节
    	payload.push_back(static_cast<uint8_t>((it->power >> 16) & 0xFF)); // 第 3 字节
    	payload.push_back(static_cast<uint8_t>((it->power >> 24) & 0xFF)); // 第 4 字节：最高 8 位
    	
	ECO_INFO_NEW("SetMcuPower type:{},power:{}",it->type,it->power);
    }

    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    Send(std::string(tx.begin(), tx.end()));
    return true;
}

bool
UartDispatcher::SetGyroResetCmd(const uint8_t& option)
{
    UartFramePkg pkg;
    pkg.type = CMD_TYPE_REQUEST;
    pkg.id = rand();
    pkg.cmd = 'G';
    pkg.sub_cmd = 'C';

    std::vector<uint8_t> payload = {
        (uint8_t)(0),  // idx 0x0:校准，0x02:启动单轴陀螺仪
        (uint8_t)option // 0x00：角度校准，0x01：加速度校准，0x02：全部校准
    };
    pkg.payload = payload;

    UartFrame frame;
    auto tx = frame.UartFrameBuild(pkg);

    ECO_INFO("Send to mcu reset gyro, option: %d", option);
    Send(std::string(tx.begin(), tx.end()));

    return true;
}
