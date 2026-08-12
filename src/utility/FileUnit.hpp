#ifndef FILE_UNIT_H_20220214
#define FILE_UNIT_H_20220214

#include <iostream>
#include <string>
#include <vector>
#include <log_helper/LogHelper.h>

namespace stark_power_manager
{
class FileUnit
{
public:
    static FileUnit&
    Instance()
    {
        static FileUnit instance;
        return instance;
    }
    FileUnit();
    virtual ~FileUnit();

public:
    bool
    CreateNewDirectory(const std::string& path_name);
    bool
    CreateNewFile(const std::string& file_name);
    bool
    IsDirExist(const std::string& path_name);
    bool
    IsFileExist(const std::string& file_name);
    bool
    IsAllFileNotEmpty(const std::string& file_name, const std::vector<std::string>& suffix);
    uint32_t
    GetFileSize(const std::string& file_name);
    std::string
    GetFileContent(const std::string& file_name);
    std::vector<uint8_t>
    GetFileContentAsVector(const std::string& file_name);
    bool
    WriteContentToFile(const std::string& file_name, const std::string& content);
    bool
    WriteVectorContentToFile(const std::string& file_name, const std::vector<uint8_t>& content);

    /**< 使用系统函数实现 */
    template<typename T>
    inline static bool
    writeToFileBySystem(const std::string& filename, const T& data)
    {
        ECO_INFO("[file]Write file: %s", filename.c_str());
        try
        {
            const int fd =
                open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
            if (fd == -1)
            {
                ECO_ERROR("The file cannot be opened.");
                return false;
            }

            ssize_t bytesWritten = write(fd, data.data(), data.size());
            if (bytesWritten == -1)
            {
                close(fd);
                ECO_ERROR("An exception occurred when writing to the file.");
                return false;
            }

            int ret = fsync(fd);
            if (ret == -1)
            {
                close(fd);
                ECO_ERROR("An exception occurred when executing fsync.");
                return false;
            }

            close(fd);
            return true;
        }
        catch (const std::exception& ex)
        {
            ECO_ERROR_STREAM("An exception occurred when writing to the file: " << ex.what());
            return false;
        }
    }
};
}  // namespace stark_power_manager

#endif