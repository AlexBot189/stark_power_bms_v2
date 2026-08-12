#include <vector>
#include <thread>
#include <functional>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

namespace stark_power_manager
{
class SerialReceiver {
public:
    using Callback = std::function<void(const uint8_t* data, size_t len)>;

    SerialReceiver(const char* port, Callback callback) 
        : m_port(port), m_callback(callback) {}
    
    bool start() {
        m_fd = open(m_port, O_RDWR | O_NOCTTY);
        if(m_fd < 0) {
            perror("open receiver port failed");
            return false;
        }
	printf("Open uart %s:115200 success!\n", m_port);
        
        if(configure_serial(m_fd) != 0) {
            close(m_fd);
            return false;
        }
        
        m_thread = std::thread(&SerialReceiver::run, this);
        return true;
    }
    
    void stop() {
        m_running = false;
        if(m_thread.joinable()) m_thread.join();
        if(m_fd >= 0) close(m_fd);
    }

private:
    void run() {
        uint8_t buf[512];
        while(m_running) {
            ssize_t n = read(m_fd, buf, sizeof(buf));
            if(n > 0) {
                m_callback(buf, n);
            } else if(n < 0 && errno != EAGAIN) {
                perror("read error");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    int configure_serial(int fd) {
        // 复用你提供的配置函数
        struct termios options;
        if(tcgetattr(fd, &options) != 0) return -1;
        
        cfsetispeed(&options, B115200);
        cfsetospeed(&options, B115200);
        
        options.c_cflag |= (CLOCAL | CREAD);
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CRTSCTS;
        
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL);
        options.c_oflag &= ~OPOST;
        
        options.c_cc[VMIN] = 1;
        options.c_cc[VTIME] = 0;
        
        if(tcsetattr(fd, TCSANOW, &options) != 0) return -1;
        tcflush(fd, TCIFLUSH);
        return 0;
    }

    const char* m_port;
    int m_fd = -1;
    Callback m_callback;
    std::thread m_thread;
    std::atomic<bool> m_running{true};
};


}  // namespace comdebug
