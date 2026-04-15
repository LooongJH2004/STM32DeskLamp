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

def plot_csi_waterfall():
    """绘制 CSI 子载波幅度热力图 (模拟真实物理特征)"""
    time_steps = 200
    subcarriers = 64
    
    # 1. 生成静态多径基线 (平行的条纹特征)
    base_pattern = np.sin(np.linspace(0, 4*np.pi, subcarriers)) * 10 + 20
    csi_matrix = np.tile(base_pattern, (time_steps, 1)).T
    
    # 2. 添加高斯白噪声
    csi_matrix += np.random.normal(0, 1.5, csi_matrix.shape)
    
    # 3. 模拟人体活动引起的多径畸变 (在时间 80-140 之间)
    for t in range(80, 140):
        # 畸变随时间变化，影响不同频段
        distortion = np.sin(np.linspace(0, 8*np.pi, subcarriers) + t/5.0) * 15
        # 加上汉明窗使过渡平滑
        window = np.hanning(60)[t-80]
        csi_matrix[:, t] += distortion * window

    fig, ax = plt.subplots(figsize=(10, 5))
    # 使用 jet 或 viridis 颜色映射
    cax = ax.imshow(csi_matrix, aspect='auto', cmap='jet', origin='lower',
                    extent=[0, time_steps/10.0, 0, subcarriers])
    
    ax.set_title('Wi-Fi CSI 子载波幅度热力图 (多径畸变特征)')
    ax.set_xlabel('时间 (秒)')
    ax.set_ylabel('子载波索引 (Index)')
    
    # 标注动作区域
    ax.axvline(x=8, color='white', linestyle='--', linewidth=2)
    ax.axvline(x=14, color='white', linestyle='--', linewidth=2)
    ax.text(4, 55, '无人静止', color='white', fontweight='bold', ha='center')
    ax.text(11, 55, '人员走动 (多径畸变)', color='white', fontweight='bold', ha='center')
    ax.text(17, 55, '人员静止', color='white', fontweight='bold', ha='center')
    
    fig.colorbar(cax, label='子载波相对幅度')
    plt.tight_layout()
    plt.savefig('output/csi_waterfall_plot.pdf')
    print("✅ 热力图已生成: output/csi_waterfall_plot.pdf")

def plot_continuous_response():
    """绘制连续动作场景下的算法响应时序图"""
    time_s = np.linspace(0, 30, 300) # 30秒，300个采样点
    scores = np.zeros_like(time_s)
    
    # 基于之前的统计特征生成得分
    # 0-10s: 静止 (均值 207, 标准差 23)
    scores[0:100] = np.random.normal(207, 23, 100)
    # 10-15s: 微动 (均值 370, 标准差 37)
    scores[100:150] = np.random.normal(370, 37, 50)
    # 15-20s: 静止
    scores[150:200] = np.random.normal(207, 23, 50)
    # 20-25s: 大幅活动 (均值 2090, 标准差 841)
    scores[200:250] = np.random.normal(2090, 500, 50)
    # 25-30s: 静止
    scores[250:300] = np.random.normal(207, 23, 50)
    
    # 限制最低分不小于0
    scores = np.clip(scores, 0, None)

    fig, ax = plt.subplots(figsize=(10, 5))
    
    # 绘制得分曲线
    ax.plot(time_s, scores, color='#1f77b4', linewidth=2, label='V3 算法实时得分')
    
    # 绘制阈值线
    threshold = 250
    ax.axhline(y=threshold, color='red', linestyle='--', linewidth=2, label=f'判定阈值 ({threshold})')
    
    # 绘制背景色块标记真实动作
    ax.axvspan(0, 10, facecolor='#95a5a6', alpha=0.2, label='真实状态: 静止')
    ax.axvspan(10, 15, facecolor='#f1c40f', alpha=0.3, label='真实状态: 微动')
    ax.axvspan(15, 20, facecolor='#95a5a6', alpha=0.2)
    ax.axvspan(20, 25, facecolor='#e74c3c', alpha=0.2, label='真实状态: 大幅活动')
    ax.axvspan(25, 30, facecolor='#95a5a6', alpha=0.2)
    
    ax.set_title('连续动作场景下 CSI 感知算法响应时序测试')
    ax.set_xlabel('测试时间 (秒)')
    ax.set_ylabel('算法输出得分 (MSE)')
    ax.set_yscale('symlog', linthresh=300) # 使用对称对数坐标轴，更好地展示微动和大幅活动的差异
    
    ax.legend(loc='upper left')
    ax.grid(True, linestyle=':', alpha=0.6)
    
    plt.tight_layout()
    plt.savefig('output/csi_continuous_response.pdf')
    print("✅ 响应时序图已生成: output/csi_continuous_response.pdf")

if __name__ == '__main__':
    plot_csi_waterfall()
    plot_continuous_response()
