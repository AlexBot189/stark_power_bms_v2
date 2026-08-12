# -*- coding: utf-8 -*-

'''
Author: colin yuanzhi.yang@ecovacs.com
Date: 2024-08-08 13:46:17
LastEditors: colin yuanzhi.yang@ecovacs.com
LastEditTime: 2024-08-08 13:57:07
FilePath: /eros_periph_manager_node/sbin/showLastPointClouds.py
Description: 雷达坐标系转换到机器坐标系后且剔除了柱子的点云数据可视化
'''

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv('ldlidar_after.csv')

r = df.iloc[:, 0].values * 1000.0
# theta = df.iloc[:, 1].values
theta = df.iloc[:, 1].values
x = r * np.cos(theta)
y = r * np.sin(theta)

plt.figure(figsize=(8, 6))
plt.scatter(x, y, color='blue', alpha=0.6)
plt.xlabel('X')
plt.ylabel('Y')
plt.title('Lidar Scatter Plot')
plt.grid(True)
plt.show()