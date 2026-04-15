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
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

    # ==========================================
    # 子图 1: 算法单包得分映射函数对比 (V1 vs V3)
    # ==========================================
    x = np.linspace(0, 15, 100)
    y_v1 = x * 10       # V1: 绝对差值 (线性放大以便于观察)
    y_v3 = x**2         # V3: 均方误差 (非线性放大)
    
    ax1.plot(x, y_v1, label='V1: 绝对差值法 (线性)', linestyle='--', color='gray', linewidth=2)
    ax1.plot(x, y_v3, label='V3: 均方误差法 (非线性)', color='#d62728', linewidth=2)
    
    ax1.set_title('(a) 算法单包得分映射函数对比')
    ax1.set_xlabel('子载波轮廓偏差幅度 $|N_i - B_i|$')
    ax1.set_ylabel('单包理论输出得分')
    ax1.legend(loc='upper left')
    ax1.grid(True, linestyle=':', alpha=0.7)

    # ==========================================
    # 子图 2: 截尾均值滤波抗干扰理论效果
    # ==========================================
    np.random.seed(42)
    samples = 150
    # 模拟基础环境底噪
    raw_score = np.random.normal(15, 8, samples)
    
    # 模拟真实人体微动 (汉明窗平滑曲线)
    raw_score[60:90] += np.hanning(30) * 200
    
    # 模拟突发的射频脉冲噪声 (极值毛刺)
    raw_score[20] = 250
    raw_score[120] = 220

    # 应用截尾均值滤波 (模拟 C 代码逻辑: 窗口32, 剔除上下20%)
    window = 16 # 为了在150个采样点中视觉效果更好，窗口等比例缩小
    trim_cnt = int(window * 0.2)
    filtered = np.zeros(samples)
    
    for i in range(samples):
        if i < window:
            filtered[i] = np.mean(raw_score[:i+1])
        else:
            win_data = np.sort(raw_score[i-window:i])
            # 剔除最高和最低的 trim_cnt 个数据
            filtered[i] = np.mean(win_data[trim_cnt:-trim_cnt])

    ax2.plot(raw_score, label='原始 MSE 得分 (含射频脉冲噪声)', alpha=0.4, color='gray', linestyle='--')
    ax2.plot(filtered, label='截尾均值滤波后得分 (V3)', color='#1f77b4', linewidth=2.5)
    ax2.axhline(100, color='red', linestyle=':', label='理论判定阈值')
    
    ax2.set_title('(b) 截尾均值滤波抗干扰理论效果')
    ax2.set_xlabel('时间窗口 (采样点)')
    ax2.set_ylabel('最终输出得分')
    ax2.legend(loc='upper right')
    ax2.grid(True, linestyle=':', alpha=0.7)

    # 保存图表
    plt.tight_layout()
    output_pdf = 'output/csi_v3_theory.pdf'
    plt.savefig(output_pdf)
    print(f"✅ 理论分析图表已生成: {output_pdf}")

if __name__ == '__main__':
    plot_csi_theory()
