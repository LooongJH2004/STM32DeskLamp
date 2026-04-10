import urllib.request
import urllib.parse
import json
import wave

# 替换为你的 API Key 和 Secret Key
API_KEY = "w3Jwped6RKLCjRecVZN2qedh"
SECRET_KEY = "IeOjLgwKyyr9ixv3ldD9OVJafazxaRq0"
TEXT = "网络连接成功，我是你的智能台灯。很高兴为你服务！"

print("1. 正在获取 Token...")
token_url = f"https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id={API_KEY}&client_secret={SECRET_KEY}"
req = urllib.request.Request(token_url)
with urllib.request.urlopen(req) as response:
    res = json.loads(response.read())
    token = res["access_token"]

print("2. 正在请求 TTS 音频数据 (aue=4 PCM格式)...")
tts_url = "http://tsn.baidu.com/text2audio"
data = urllib.parse.urlencode({
    "tex": TEXT,
    "tok": token,
    "cuid": "python_test_001",
    "ctp": 1,
    "lan": "zh",
    "spd": 5,
    "pit": 5,
    "vol": 5,
    "per": 0,
    "aue": 4  # 核心：4表示请求原始PCM数据
}).encode('utf-8')

req = urllib.request.Request(tts_url, data=data)
with urllib.request.urlopen(req) as response:
    pcm_data = response.read()

print(f"3. 成功获取到 {len(pcm_data)} 字节的 PCM 数据。")

print("4. 正在保存为 test_baidu_tts.wav...")
# 将纯 PCM 数据包装上 WAV 头，以便电脑播放器能识别
with wave.open("test_baidu_tts.wav", "wb") as wav_file:
    wav_file.setnchannels(1)      # 单声道
    wav_file.setsampwidth(2)      # 16-bit (2 bytes)
    wav_file.setframerate(16000)  # 16kHz 采样率
    wav_file.writeframes(pcm_data)

print("✅ 完成！请在当前目录下找到 test_baidu_tts.wav 并双击播放。")
