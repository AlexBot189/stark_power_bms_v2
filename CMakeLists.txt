##########################
#
#  	CMakeLists.txt
#	version: 0.1
#  	Created on: 2020.4
# 	Author: yanxin.xing
#	Description: project CMakeLists
#	User: project developer
#
##########################

cmake_minimum_required(VERSION 2.8.12)
project(stark_power_manager_node)

option (USE_CROSS_COMPILE "Use Cross-compile" ON)
if(USE_CROSS_COMPILE)
	set(TARGET_ARCHITECTURE "arm64")
else()
    set(TARGET_ARCHITECTURE "amd64")
endif()

include(eco_config.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} /home/exbot/build-dep/rv1126b/build/config)
include(eco_config_global)

set(COMMON_INCLUDE_PATH
    "/home/exbot/ros_build_env_melodic/rk3576/ros/staging/root/opt/ros/melodic/include"     # 自定义头文件路径
    "/home/exbot/ros_build_env_melodic/rk3576/boost/install/root/usr/include"     # 自定义头文件路径
)

message("根目录的 CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

#这里顺序最好不要改，如有需要可以在${CUSTOM_INLCUDE_PATH}中添加
include_directories(
	include
	src/include
	${CMAKE_CURRENT_SOURCE_DIR}/src/3rd_party/opencv_3.2/include/
	${CUSTOM_INLCUDE_PATH}	#自定义
	${EROSMSG_INCLUDE_PATH}	#eros_msg
	${PACKAGE_INCLUDE_PATH}	#固件中其他包
	${COMMON_INCLUDE_PATH}	#非固件中其他包，如加密库，ros
)


# 链接路径
#这里顺序最好不要改，如有需要可以在${CUSTOM_LIBRARY_PATH}中添加
link_directories(
	${CUSTOM_LIBRARY_PATH}	#自定义
	${PACKAGE_LIBRARY_PATH}	#固件中其他包
	${COMMON_LIBRARY_PATH}	#非固件中其他包，如加密库，ros
)

# 默认编译规则
#     编译名为${PROJECT_NAME}，COMPILE_TARGET_TYPE指定类型，使用scr中的源文件，
#     依赖库由DEPENDENCY_HARDWARE_LIST，DEPENDENCY_SYSTEM_LIST，DEPENDENCY_ROS_LIST三个变量指定，见eco_config.cmake
include(eco_build)

# 使用自定义编译规则
#add_custom_build(para1 para2 para3 para4)
	# @para1: target 类型 [shared/bin/static]
	# @para2: target 名称
	# @para3: target 源文件列表，需要绝对路径
	# @para4: target 链接的库列表
# 举几个栗子, 其中${SRC_DIR}为源码所在绝对路径
# add_custom_build(shared myshare "${SRC_DIR}/fool.c;${SRC_DIR}/fool2.cpp" "Deebot;Xspace")
# add_custom_build(bin mybin "${SRC_DIR}/fool.c;${SRC_DIR}/fool2.cpp" "Deebot;Xspace")
# add_custom_build(static mystatic "${SRC_DIR}/fool.c;${SRC_DIR}/fool2.cpp" "Deebot;Xspace")

include(eco_install)

target_link_libraries(${PROJECT_NAME}
        gpiod
)

# add_subdirectory(src/drivers/ld)


