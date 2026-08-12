#include <boost/filesystem.hpp>
#include <fstream>
#include <iterator>
#include <log_helper/LogHelper.h>

#include "FileUnit.hpp"

namespace stark_power_manager
{
    FileUnit::FileUnit()
    {
    }

    FileUnit::~FileUnit()
    {
        ECO_WARN("~FileUnit");
    }

    std::string FileUnit::GetFileContent(const std::string &file_name)
    {
        if (!IsFileExist(file_name))
        {
            ECO_WARN("File %s not exist", file_name.c_str());
        }
        else
        {
            std::ifstream is(file_name);
            return {std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>()};
        }
        return std::string();
    }

    bool FileUnit::WriteContentToFile(const std::string &file_name, const std::string &content)
    {
        std::ofstream file(file_name, std::ios::binary | std::ios::out | std::ios::trunc);
        file << content;
        file.close();
        sync();
        return true;
    }

    std::vector<uint8_t> FileUnit::GetFileContentAsVector(const std::string &file_name)
    {
        if (!IsFileExist(file_name))
        {
            ECO_WARN("File %s not exist", file_name.c_str());
        }
        else
        {
            uint32_t file_size = GetFileSize(file_name);
            std::vector<uint8_t> buffer;
            buffer.resize(file_size);
            std::ifstream inFile(file_name, std::ios::binary);
            inFile.read((char *)buffer.data(), buffer.size());
            inFile.close();
            ECO_DEBUG("read div binary_size  = %d, file_size = %d", buffer.size(), file_size);
            return std::move(buffer);
        }
        return std::vector<uint8_t>();
    }

    bool FileUnit::WriteVectorContentToFile(const std::string &file_name, const std::vector<uint8_t> &content)
    {

        std::ofstream file(file_name, std::ios::binary | std::ios::out | std::ios::trunc);
        file.write((char *)content.data(), content.size());
        file.close();
        sync();
        int file_size = GetFileSize(file_name);
        ECO_DEBUG("write div binary_size  = %d, file_size = %d", content.size(), file_size);
        return true;
    }

    bool FileUnit::CreateNewDirectory(const std::string &path_name)
    {
        if (IsDirExist(path_name))
        {
            return true;
        }
        return boost::filesystem::create_directories(path_name);
    }

    bool FileUnit::CreateNewFile(const std::string &file_name)
    {
        // if (IsFileExist(file_name))
        // {
        //     return true;
        // }
        // std::ofstream fout(file_name.c_str());
        // fout.close();
        // return true;

         boost::filesystem::path filePath(file_name);
        // 创建父目录（如果不存在）
        if (!boost::filesystem::exists(filePath.parent_path()))
        {
            boost::filesystem::create_directories(filePath.parent_path());
        }

        // 创建文件
        std::ofstream ofs(filePath.native());
        if (!ofs.is_open())
        {
            std::cerr << "无法创建文件！" << std::endl;
            return -1;
        }

        // 文件操作...

        // 关闭文件并退出程序
        ofs.close();
        return true;
    }

    bool FileUnit::IsDirExist(const std::string &path_name)
    {
        if (boost::filesystem::exists(path_name) && boost::filesystem::is_directory(path_name))
        {
            return true;
        }
        return false;
    }

    bool FileUnit::IsFileExist(const std::string &file_name)
    {
        if (boost::filesystem::exists(file_name) && boost::filesystem::is_regular_file(file_name))
        {
            return true;
        }
        return false;
    }

    uint32_t FileUnit::GetFileSize(const std::string &file_name)
    {
        uint32_t file_size = boost::filesystem::file_size(file_name);
        return file_size;
    }

    bool FileUnit::IsAllFileNotEmpty(const std::string &file_name, const std::vector<std::string> &suffix)
    {
        bool ret = true;
        for (auto var : suffix)
        {
            std::string file_full_path = file_name + var;
            if (boost::filesystem::file_size(file_full_path) <= 0)
            {
                ECO_WARN("file \"%d\" is empty !!!", file_full_path.c_str());
                ret = false;
                break;
            }
        }
        return ret;
    }
} // namespace stark_power_manager