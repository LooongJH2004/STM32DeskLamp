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

def parse_voice_logs(input_txt):
    if not os.path.exists(input_txt):
        print(f"❌ 错误：找不到日志文件 {input_txt}")
        return None

    # 优化后的正则：更灵活地匹配时间戳和标签
    # 匹配 I (12345) TAG: [TIMING] T0: 描述
    pattern = re.compile(r"I\s+\((\d+)\).*?\[TIMING\]\s+(T\d)[:\s]+(.*)")
    
    sessions = []
    current_session = {}

    print(f"正在读取文件: {input_txt} ...")
    with open(input_txt, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            match = pattern.search(line)
            if match:
                ts = int(match.group(1))
                tag = match.group(2)
                
                # T0 代表一次新交互的开始
                if tag == 'T0':
                    # 如果当前已经有一个接近完整的会话，保存它
                    if 'T0' in current_session and 'T4' in current_session:
                        sessions.append(current_session)
                    current_session = {'T0': ts}
                else:
                    current_session[tag] = ts
        
        # 保存最后一个会话
        if 'T0' in current_session and 'T4' in current_session:
            sessions.append(current_session)

    if not sessions:
        return pd.DataFrame() # 返回空DataFrame

    data_list = []
    for i, s in enumerate(sessions):
        # 确保 T0-T4 都有，否则跳过
        if all(k in s for k in ['T0', 'T1', 'T2', 'T3', 'T4']):
            data_list.append({
                'Session': f"交互 {i+1}",
                'ASR识别 (百度)': s['T1'] - s['T0'],
                'LLM推理 (DeepSeek)': s['T2'] - s['T1'],
                'TTS合成与下行 (百度)': s['T3'] - s['T2'],
                '系统预缓冲 (RingBuffer)': s['T4'] - s['T3'],
                'Total': s['T4'] - s['T0']
            })

    return pd.DataFrame(data_list)

def plot_latency_analysis(df):
    if df.empty:
        print("⚠️ 警告：没有提取到完整的 T0-T4 语音交互数据，无法绘图。")
        print("请检查日志文件中是否包含 [TIMING] T0 到 T4 的完整链路。")
        return

    # 打印统计结果
    print("="*40)
    print("📊 语音交互延迟统计 (单位: ms):")
    # 只对数值列求平均
    numeric_df = df.drop(columns=['Session'])
    print(numeric_df.mean().to_string())
    print("="*40)

    # 绘图逻辑
    categories = ['ASR识别 (百度)', 'LLM推理 (DeepSeek)', 'TTS合成与下行 (百度)', '系统预缓冲 (RingBuffer)']
    session_labels = df['Session']
    
    fig, ax = plt.subplots(figsize=(10, 6))
    bottom = np.zeros(len(df))
    colors = ['#4e79a7', '#f28e2b', '#e15759', '#76b7b2']
    
    for i, cat in enumerate(categories):
        ax.bar(session_labels, df[cat], bottom=bottom, label=cat, color=colors[i], alpha=0.85)
        bottom += df[cat]

    for i, total in enumerate(df['Total']):
        ax.text(i, total + 50, f"{total}ms", ha='center', va='bottom', fontweight='bold')

    ax.set_ylabel('耗时 (毫秒)')
    ax.set_title('端到端语音交互全链路延迟分解测试 (T0 - T4)')
    ax.legend(loc='upper left', bbox_to_anchor=(1, 1))
    ax.grid(axis='y', linestyle='--', alpha=0.5)

    avg_total = df['Total'].mean()
    avg_llm = df['LLM推理 (DeepSeek)'].mean()
    plt.figtext(0.15, 0.02, f"平均总延迟: {avg_total:.0f}ms | LLM平均占比: {(avg_llm/avg_total)*100:.1f}%", 
                fontsize=10, bbox=dict(facecolor='white', alpha=0.5))

    plt.tight_layout(rect=[0, 0.05, 1, 1])
    plt.savefig('output/voice_latency_analysis.pdf')
    plt.savefig('output/voice_latency_analysis.png')
    print(f"✅ 延迟分析图表已生成: output/voice_latency_analysis.pdf")

if __name__ == '__main__':
    df_result = parse_voice_logs('data/voice_interaction_test.txt')
    plot_latency_analysis(df_result)
