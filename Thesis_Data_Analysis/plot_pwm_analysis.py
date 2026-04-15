import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# 确保输出目录存在
os.makedirs('output', exist_ok=True)

# 设置全局字体，确保论文图表美观 (使用系统自带的黑体或宋体支持中文)
plt.rcParams['font.sans-serif'] = ['SimHei', 'Songti SC', 'Arial Unicode MS'] # 兼容Win/Mac
plt.rcParams['axes.unicode_minus'] = False # 正常显示负号
plt.rcParams['font.size'] = 12
plt.rcParams['figure.dpi'] = 300 # 高分辨率

def plot_linearity():
    """绘制 PWM 调光线性度测试图 (冷光 100%)"""
    df = pd.read_csv('data/pwm_linearity_100.csv')
    
    plt.figure(figsize=(8, 6))
    
    # 生成理论连续曲线 (0-1000)
    x_theory = np.linspace(0, 1000, 100)
    y_theory = x_theory / 10.0
    
    # 绘制理论线
    plt.plot(x_theory, y_theory, label='理论占空比曲线', color='gray', linestyle='--', linewidth=2)
    
    # 绘制实测散点
    plt.scatter(df['Setting_Value'], df['Measured_Duty'], 
                color='red', marker='o', s=80, label='实测占空比 (DM40A)', zorder=5)
    
    plt.title('PWM 调光线性度测试 (色温 100% 冷光)')
    plt.xlabel('软件设定光强值 (0-1000)')
    plt.ylabel('PWM 占空比 (%)')
    plt.xlim(0, 1050)
    plt.ylim(0, 105)
    plt.grid(True, linestyle=':', alpha=0.7)
    plt.legend(loc='upper left')
    
    # 保存为 PDF 和 PNG (PDF 插入 LaTeX 最清晰)
    plt.savefig('output/pwm_linearity.pdf', bbox_inches='tight')
    plt.savefig('output/pwm_linearity.png', bbox_inches='tight')
    plt.close()
    print("已生成: output/pwm_linearity.pdf")

def plot_colortemp_comparison():
    """绘制不同色温下的占空比分配图 (验证恒功率算法)"""
    df_50 = pd.read_csv('data/pwm_colortemp_50.csv')
    df_70 = pd.read_csv('data/pwm_colortemp_70.csv')
    
    plt.figure(figsize=(8, 6))
    
    # 理论总功率线 (斜率为 0.1)
    x_theory = np.linspace(0, 1000, 100)
    plt.plot(x_theory, x_theory/10.0, label='理论总光强 (100%)', color='black', linestyle='-', linewidth=1.5)
    
    # 色温 70% 理论与实测
    plt.plot(x_theory, x_theory * 0.07, color='blue', linestyle='--', alpha=0.5)
    plt.scatter(df_70['Setting_Value'], df_70['Measured_Duty'], 
                color='blue', marker='s', s=60, label='冷光实测 (色温 70%)', zorder=5)
    
    # 色温 50% 理论与实测
    plt.plot(x_theory, x_theory * 0.05, color='green', linestyle='--', alpha=0.5)
    plt.scatter(df_50['Setting_Value'], df_50['Measured_Duty'], 
                color='green', marker='^', s=60, label='冷光实测 (色温 50%)', zorder=5)
    
    plt.title('双色温恒功率分配算法验证')
    plt.xlabel('软件设定总光强值 (0-1000)')
    plt.ylabel('冷光通道 PWM 占空比 (%)')
    plt.xlim(0, 1050)
    plt.ylim(0, 105)
    plt.grid(True, linestyle=':', alpha=0.7)
    plt.legend(loc='upper left')
    
    plt.savefig('output/pwm_colortemp_algo.pdf', bbox_inches='tight')
    plt.savefig('output/pwm_colortemp_algo.png', bbox_inches='tight')
    plt.close()
    print("已生成: output/pwm_colortemp_algo.pdf")

if __name__ == '__main__':
    plot_linearity()
    plot_colortemp_comparison()
