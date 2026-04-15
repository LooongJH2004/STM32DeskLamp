import re
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# ==========================================
# 1. 全局配置
# ==========================================
os.makedirs('data', exist_ok=True)
os.makedirs('output', exist_ok=True)

plt.rcParams['font.sans-serif'] = ['SimHei', 'Songti SC', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['font.size'] = 11
plt.rcParams['figure.dpi'] = 300

def parse_annotated_csi(input_txt):
    if not os.path.exists(input_txt):
        print(f"❌ 错误：找不到日志文件 {input_txt}")
        return None

    # 匹配得分数据：I (时间戳) Dev_CSI: [雷达-V3] 得分: 数值 | 阈值: 数值
    data_pattern = re.compile(r"I\s+\((\d+)\)\s+Dev_CSI:\s+\[雷达-V3\]\s+得分:\s+(\d+)\s+\|\s+阈值:\s+(\d+)")
    # 匹配标签：<标签名> 或 </标签名>
    tag_pattern = re.compile(r"<(/?)([^>]+)>")
    
    parsed_data = []
    current_state = "未标注"

    with open(input_txt, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line: continue

            # 检查是否是标签行
            tag_match = tag_pattern.match(line)
            if tag_match:
                is_closing = tag_match.group(1) == "/"
                tag_name = tag_match.group(2)
                if not is_closing:
                    current_state = tag_name
                else:
                    current_state = "未标注"
                continue

            # 检查是否是数据行
            data_match = data_pattern.search(line)
            if data_match and current_state != "未标注":
                parsed_data.append({
                    'Timestamp_ms': int(data_match.group(1)),
                    'Score': int(data_match.group(2)),
                    'Threshold': int(data_match.group(3)),
                    'State': current_state
                })

    if not parsed_data:
        print("❌ 未找到有效的标注数据，请检查 XML 标签是否正确包裹。")
        return None

    return pd.DataFrame(parsed_data)

def plot_csi_analysis(df):
    # 1. 数据预处理
    start_time = df['Timestamp_ms'].iloc[0]
    df['Time_s'] = (df['Timestamp_ms'] - start_time) / 1000.0
    
    # 2. 创建画布：上方为时序图，下方为箱线图（展示区分度）
    fig = plt.figure(figsize=(12, 8))
    gs = fig.add_gridspec(2, 1, height_ratios=[2, 1], hspace=0.3)
    
    # --- 子图 1: 时序得分图 ---
    ax1 = fig.add_subplot(gs[0])
    states = df['State'].unique()
    colors = {'静止': '#95a5a6', '微动': '#3498db', '活动': '#e74c3c'}
    
    for state in states:
        state_df = df[df['State'] == state]
        ax1.scatter(state_df['Time_s'], state_df['Score'], 
                    label=f'实测得分 ({state})', color=colors.get(state, 'black'), s=30, alpha=0.7)

    # 绘制阈值线
    ax1.axhline(y=df['Threshold'].iloc[0], color='red', linestyle='--', linewidth=2, label='系统判定阈值 (250)')
    
    ax1.set_title('Wi-Fi CSI 环境感知算法得分分布 (V3 算法)')
    ax1.set_ylabel('CSI 变动得分')
    ax1.set_xlabel('测试时间 (秒)')
    ax1.legend(loc='upper right')
    ax1.grid(True, linestyle=':', alpha=0.6)

    # --- 子图 2: 箱线图 (展示算法的区分度) ---
    ax2 = fig.add_subplot(gs[1])
    box_data = [df[df['State'] == s]['Score'] for s in ['静止', '微动', '活动'] if not df[df['State'] == s].empty]
    box_labels = [s for s in ['静止', '微动', '活动'] if not df[df['State'] == s].empty]
    
    bplot = ax2.boxplot(box_data, vert=False, patch_artist=True, labels=box_labels)
    
    # 为箱线图着色
    for patch, label in zip(bplot['boxes'], box_labels):
        patch.set_facecolor(colors.get(label, 'white'))
        patch.set_alpha(0.6)

    ax2.set_title('不同动作状态下的得分区间对比 (算法区分度验证)')
    ax2.set_xlabel('得分数值')
    ax2.grid(axis='x', linestyle=':', alpha=0.6)

    # 3. 统计分析打印
    print("="*40)
    print("📊 CSI 感知算法统计分析:")
    stats = df.groupby('State')['Score'].agg(['mean', 'std', 'max', 'min'])
    print(stats)
    print("="*40)

    plt.tight_layout()
    plt.savefig('output/csi_sensing_analysis.pdf')
    plt.savefig('output/csi_sensing_analysis.png')
    print(f"✅ CSI 分析图表已生成: output/csi_sensing_analysis.pdf")

if __name__ == '__main__':
    df_csi = parse_annotated_csi('data/csi_test_annotated.txt')
    if df_csi is not None:
        plot_csi_analysis(df_csi)
