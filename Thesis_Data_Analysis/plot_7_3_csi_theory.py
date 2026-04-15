import numpy as np
import matplotlib.pyplot as plt
import os

# ==========================================
# 1. 全局配置
# ==========================================
os.makedirs('output', exist_ok=True)
plt.rcParams['font.sans-serif'] = ['SimHei', 'Songti SC', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['font.size'] = 11
plt.rcParams['figure.dpi'] = 300

def plot_csi_theory():
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5.5))

    # ==========================================
    # 子图 1: 物理扰动到算法得分的理论映射模型 (保持不变)
    # ==========================================
    x_deviation = np.linspace(0, 50, 200)
    y_v1_linear = x_deviation * 10 
    y_v3_squared = x_deviation ** 2 
    
    ax1.plot(x_deviation, y_v1_linear, label='V1 算法: 线性映射 ($S \propto \Delta N$)', 
             linestyle='--', color='gray', linewidth=2)
    ax1.plot(x_deviation, y_v3_squared, label='V3 算法: 平方映射 ($S \propto \Delta N^2$)', 
             color='#d62728', linewidth=2.5)
    
    ax1.axhline(250, color='black', linestyle='-.', linewidth=1.5, label='系统判定阈值 (250)')
    
    ax1.axvspan(0, 15.8, facecolor='#95a5a6', alpha=0.2, label='静止/环境底噪区')
    ax1.axvspan(15.8, 25, facecolor='#3498db', alpha=0.2, label='微动映射区 (如翻书)')
    ax1.axvspan(25, 50, facecolor='#e74c3c', alpha=0.2, label='大幅活动映射区')
    
    ax1.set_title('(a) 物理扰动量至算法得分的映射模型')
    ax1.set_xlabel('子载波轮廓平均偏差幅度 $\Delta N$ (%)')
    ax1.set_ylabel('理论输出得分 $S$')
    ax1.set_xlim(0, 50)
    ax1.set_ylim(0, 2500)
    ax1.legend(loc='upper left', fontsize=9)
    ax1.grid(True, linestyle=':', alpha=0.7)

    # ==========================================
    # 子图 2: 单个 0.5s 判定窗口内的截尾滤波剖析
    # ==========================================
    np.random.seed(42)
    window_size = 12 # 0.5秒内的 12 个数据包
    
    # 模拟静止状态下的 12 个包得分 (均值约 150)
    raw_data = np.random.normal(150, 20, window_size)
    # 注入一个极端的射频脉冲毛刺
    raw_data[5] = 1450 
    
    # 算法逻辑：排序并剔除上下各 2 个极值
    sorted_indices = np.argsort(raw_data)
    trim_cnt = 2
    trimmed_indices = np.concatenate((sorted_indices[:trim_cnt], sorted_indices[-trim_cnt:]))
    retained_indices = sorted_indices[trim_cnt:-trim_cnt]
    
    # 计算两种均值
    standard_mean = np.mean(raw_data)
    trimmed_mean = np.mean(raw_data[retained_indices])
    
    # 绘制柱状图
    x_pos = np.arange(1, window_size + 1)
    colors = ['#1f77b4' if i in retained_indices else '#bdc3c7' for i in range(window_size)]
    bars = ax2.bar(x_pos, raw_data, color=colors, alpha=0.8, edgecolor='black')
    
    # 标注被剔除的柱子
    for i in trimmed_indices:
        ax2.text(x_pos[i], raw_data[i] + 20, '剔除', ha='center', va='bottom', color='#7f8c8d', fontsize=9)
        
    # 绘制均值线与阈值线
    ax2.axhline(standard_mean, color='#e67e22', linestyle='--', linewidth=2, 
                label=f'普通算术均值 ({standard_mean:.0f}) $\\rightarrow$ 误触发')
    ax2.axhline(trimmed_mean, color='#27ae60', linestyle='-', linewidth=2.5, 
                label=f'截尾均值 ({trimmed_mean:.0f}) $\\rightarrow$ 成功抗扰')
    ax2.axhline(250, color='red', linestyle='-.', linewidth=1.5, label='系统判定阈值 (250)')
    
    ax2.set_title('(b) 单个 0.5s 判定窗口内的截尾滤波剖析 (含突发干扰)')
    ax2.set_xlabel('窗口内数据包序号 (1~12)')
    ax2.set_ylabel('单包 MSE 得分')
    ax2.set_xticks(x_pos)
    ax2.set_ylim(0, 1600)
    ax2.legend(loc='upper left', fontsize=9)
    ax2.grid(axis='y', linestyle=':', alpha=0.7)

    plt.tight_layout()
    output_pdf = 'output/csi_v3_theory.pdf'
    plt.savefig(output_pdf)
    print(f"✅ 理论分析图表已生成: {output_pdf}")

if __name__ == '__main__':
    plot_csi_theory()
