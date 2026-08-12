##########################
#
#  	make.sh
#	version: 0.1
#  	Created on: 2020.4
# 	Author: yanxin.xing
#	Description: project make script for developer
#	User: project developer
#
##########################

#!/bin/bash
CUR_DIR=$(pwd)

# 项目名称，生成的库或者bin文件都依此产生，默认取当前文件夹名字作为项目名称
export ECO_PROJECT_NAME=${CUR_DIR##*/}

# workspace路径，当前版本主要用于eros_msg头文件引用
export ECO_WORKSPACE_DIR=~/workspace/project/rv1126b/embuild

# 是否显示详细的编译信息
export DETAILED_BUILDING_MESSAGE=true

# 设置打包名中project_name字段值，默认为当前项目名.
# 强烈使用默认项目名，如需更改则不允许使用-，可以使用_代替，
export ECO_PKG_PROJECT_NAME=${ECO_PROJECT_NAME}

# 编译
/home/exbot/build-dep/rv1126b/build/sub_make.sh $@


