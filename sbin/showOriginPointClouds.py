# -*- coding: utf-8 -*-

'''
Author: colin yuanzhi.yang@ecovacs.com
Date: 2024-08-08 13:45:58
LastEditors: colin yuanzhi.yang@ecovacs.com
LastEditTime: 2024-08-08 13:52:35
FilePath: /eros_periph_manager_node/sbin/showOriginPointClouds.py
Description: 雷达原始点云数据可视化
'''

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv('ldlidar.csv')

r = df.iloc[:, 0].values
# theta = df.iloc[:, 1].values
theta = df.iloc[:, 1].values * np.pi / 180
x = r * np.cos(theta)
y = r * np.sin(theta)

plt.figure(figsize=(8, 6))
plt.scatter(x, y, color='blue', alpha=0.6)
plt.xlabel('X')
plt.ylabel('Y')
plt.title('Lidar Scatter Plot')
plt.grid(True)
plt.show()