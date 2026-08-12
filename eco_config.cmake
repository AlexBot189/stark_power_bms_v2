##########################
#
#  	eco_config.cmake
#	version: 0.1
#  	Created on: 2020.4
# 	Author: yanxin.xing
#	Description: project cmake local config for developer
#	User: project developer
#
##########################

#eros_msg头文件路径，ECO_WORKSPACE_DIR在make.sh中设置
set(EROSMSG_INCLUDE_PATH ${ECO_WORKSPACE_DIR}/release/include)

# 固件依赖包路径
set(PROJECT_TYPE_NAME "rv1126b")

add_definitions(-DMODULE_NAME="stark_power_node")
#set(DEPENDENCY_VERSION "1.6.1")

#ros消息是否加密，TRUE or FALSE
set(NEED_ENCRYPT FALSE)

set(NO_STRICT TRUE)

#内存检测(仅对x86版)，address-内存越界及溢出，thread-线程安全，no-不检测
set(MEMORY_CHECK_TYPE "no")

#设置debug版是否加-g选项，不加则仅开启日志，仅对arm平台有效，x86默认debug加-g
set(NEED_SYMBOLS FALSE)	#TRUE or FALSE

#编译目标类型，bin-可执行文件，shared-动态链接库，static-静态库，eros_msg-ROS消息
set(COMPILE_TARGET_TYPE "bin")

#依赖的固件库列表，使用;隔开，如taskmanager, cf, eros, lzma, Ecovacs, Xspace, nodelet等
## !!该部分名称由于前后兼容性问题可能会有变化，实际名称请参考上述依赖路径下的名字
# set(DEPENDENCY_HARDWARE_LIST "cf;Ecovacs;lzma")

set(DEPENDENCY_THIRD_PARTY_LIST "log_helper;OpenCV")

#依赖的系统库列表，使用;隔开，如pthread，dl
# set(DEPENDENCY_SYSTEM_LIST "/home/exbot/build-dep/k850/0.1.8/k850/usr/lib/log_helper/liblog_helper.so")

#依赖的ros列表，使用;隔开，如roscpp;std_msgs
set(DEPENDENCY_ROS_LIST "roscpp;std_msgs")

# 需要打入包中的public头文件路径，使用项目根目录下相对路径，用;隔开
## 默认include和src/include中头文件为公开头文件，src/*.hpp中src/*.h为私有头文件
set(PUBLIC_HEADER_FOLDER "include;src/include")

#这里设置你自定义的头文件路径，优先级最高(除项目内相对路径中的头文件)
set(CUSTOM_INLCUDE_PATH "${ECO_WORKSPACE_DIR}/release/include/")
#set(CUSTOM_INLCUDE_PATH "")
#这里设置你自定义的lib库路径，优先级最高
set(CUSTOM_LIBRARY_PATH "")
#set(CUSTOM_LIBRARY_PATH "./src")
#设置编译时需要忽视其中源码的文件夹，使用项目根目录下相对路径，用;隔开
set(IGNORE_SOURCES_FOLDER "src/ros_adapter;src/protocol;src/drivers;src/interface;src/lidar_adpter")

#设置编译时需要忽视的源文件，仅需文件名，用;隔开l
set(IGNORE_SOURCES_FILES "")

#设置日志等级
set(ECO_CMAKE_LOG_LEVEL 1)

set(CMAKE_PREFIX_PATH "/home/exbot/build-dep/rv1126b/0.1.8/rv1126b/usr;/home/exbot/build-dep/prebuiltlibraries/linux/18.04/x86_64;${CMAKE_PREFIX_PATH}")
set(OpenCV_DIR /home/exbot/build-dep/prebuiltlibraries/linux/18.04/arm64/rv1126b/lib/cmake/opencv4)

# platform config
set(LOCAL_SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LOCAL_INCLUDE_PATH "${LOCAL_SRC_PATH};${LOCAL_SRC_PATH}/drivers/lidar_driver/ld_l14p/include;${ECO_WORKSPACE_DIR}/eros_common/include;/home/exbot/build-dep/rv1126b/0.1.8/rv1126b/usr/include/;${LOCAL_SRC_PATH}/drivers/lidar_driver/ld_l14p/include;${LOCAL_SRC_PATH}/drivers/hc_lidar/base;${LOCAL_SRC_PATH}/drivers/hc_lidar/;${LOCAL_SRC_PATH}/drivers/hc_linelaser/;${LOCAL_SRC_PATH}/drivers/ld_lidar/;${LOCAL_SRC_PATH}/drivers/ld_lidar/ldlidar_driver/include")
if(${BUILD_PLATFORM} STREQUAL "rv1126b")
	# set(NO_STRICT TRUE)
	#这里加入你的k850平台配置
	set(CUSTOM_LIBRARY_PATH "${CMAKE_CURRENT_LIST_DIR}/src/3rd_party/gpiod/lib;")
	set(CUSTOM_INLCUDE_PATH "/home/exbot/build-dep/rv1126b/0.1.8/rv1126b/usr/include;${LOCAL_INCLUDE_PATH};${CMAKE_CURRENT_LIST_DIR}/src/3rd_party/gpiod/include;")
elseif(${BUILD_PLATFORM} STREQUAL "x86")
	#这里加入你的x86平台配置
	set(CUSTOM_LIBRARY_PATH "")
	set(CUSTOM_INLCUDE_PATH "/opt/ros/melodic/include;/home/exbot/build-dep/k850/0.1.8/x86/usr/include;${LOCAL_INCLUDE_PATH}")
endif()

# debug or release配置
if(${BUILD_TYPE} STREQUAL "debug")
	#这里加入你的release版配置
else()
	#这里加入你的debug版配置
endif()
