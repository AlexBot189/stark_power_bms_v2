#!/bin/bash
##########################
#
#  	_make_scp.sh
#	version: 0.1
#  	Created on: 2024.09
# 	Author: colin.yang
#	Description: scp package 2 ftp server
#	User: script maintainer
#
##########################

readonly FTP_HOST="10.114.6.150"
readonly ECO_MAKE_CONFIG_DIR=/home/exbot/build-dep/rk3576/build
readonly SOURCE_DIR="$(pwd)/package"
readonly VERSION=$(head -n 1 "$ECO_MAKE_CONFIG_DIR/VERSION")
readonly TARGET_DIR="phoenix_rk3576/pkg/phoenix_app/$VERSION"

readonly USER="ecovacs"
readonly PASS="ecovacs"

source /home/exbot/build-dep/tools/eco_log.sh
echo "Ftp from $SOURCE_DIR to $TARGET_DIR..."

if [ ! -d "$SOURCE_DIR" ]; then
    eco_error "Failed, $SOURCE_DIR: No such file or directory"
    exit -1
fi

if [ ! "$(ls -A $SOURCE_DIR)" ]; then
    eco_error "Failed, '$SOURCE_DIR' is empty!"
    exit -1
fi

SOURCE_FILE=$(ls -t $SOURCE_DIR/*debug*.tar.bz2 | head -1)
# eco_infor "source file: $SOURCE_FILE"

lftp -u "$USER","$PASS" $FTP_HOST <<EOF
cd $TARGET_DIR
put $SOURCE_FILE
bye
EOF

if [ $? -eq 0 ]; then
    echo "上传成功！"
else
    echo "上传失败！"
fi
