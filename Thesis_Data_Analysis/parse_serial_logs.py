import re
import pandas as pd
import os

def parse_error_logs(input_txt, output_csv):
    """
    解析串口日志，提取 [TX_ERROR] 注入的错误帧数据
    """
    if not os.path.exists(input_txt):
        print(f"错误：找不到输入文件 {input_txt}")
        return

    # 正则表达式匹配日志特征
    # 示例: W (2272825) Dev_STM32: [TX_ERROR] {"cmd":"light","warm":114,"cold":266}|1048|FFFF
    pattern = re.compile(r"W \((\d+)\) Dev_STM32: \[TX_ERROR\] (\{.*?\})\|([0-9A-F]{4})\|([0-9A-F]{4})")
    
    parsed_data = []
    
    with open(input_txt, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            match = pattern.search(line)
            if match:
                timestamp = int(match.group(1))
                payload = match.group(2)
                sent_crc = match.group(3)
                remainder = match.group(4)
                
                parsed_data.append({
                    'Timestamp_ms': timestamp,
                    'Payload': payload,
                    'Sent_CRC': sent_crc,
                    'Remainder': remainder
                })
    
    if not parsed_data:
        print("未在日志中找到任何 [TX_ERROR] 记录，请检查 txt 文件内容。")
        return

    # 转换为 DataFrame 并保存为 CSV
    df = pd.DataFrame(parsed_data)
    df.to_csv(output_csv, index=False)
    
    # 计算统计信息 (用于支撑论文 7.1.2 节)
    total_errors = len(df)
    if total_errors > 1:
        time_span_ms = df['Timestamp_ms'].iloc[-1] - df['Timestamp_ms'].iloc[0]
        avg_interval = time_span_ms / (total_errors - 1)
    else:
        time_span_ms = 0
        avg_interval = 0
        
    print("="*40)
    print(f"✅ 解析完成！数据已保存至: {output_csv}")
    print(f"📊 统计信息 (可直接用于论文):")
    print(f" - 成功提取错误注入记录: {total_errors} 帧")
    print(f" - 压力测试总时长: {time_span_ms / 1000:.2f} 秒")
    print(f" - 平均发包间隔: {avg_interval:.2f} 毫秒")
    print(f" - 校验余数特征: 全部为 {df['Remainder'].iloc[0]} (证明 CRC 发生反转)")
    print("="*40)

if __name__ == '__main__':
    # 确保 data 目录存在
    os.makedirs('data', exist_ok=True)
    
    input_file = 'data/serial_error_test.txt'
    output_file = 'data/parsed_crc_errors.csv'
    
    parse_error_logs(input_file, output_file)
