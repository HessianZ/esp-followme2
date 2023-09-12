# ESP-FollowMe2
## 项目介绍
![image](https://github.com/HessianZ/esp-followme2/blob/master/assets/home1.jpg?raw=true)

该项目是基于Adafruit ESP32-S3 TFT Feather开发板的一个简单的示例，通过该示例可以学习到如何使用Adafruit ESP32-S3 TFT Feather开发板的WiFi、GPIO控制及以Neopixel LED控制等功能。

该项目参考ESP-Box-Lite的实现，完成了"Follow me活动”第2期的任务1、2、3以及任务4的分任务1。

在尚未进行配网时启动板卡会自动启动AP热点，通过手机连接该热点会启动CaptivePortal网页引导进行配网，配网成功后会自动关闭AP热点。
* SSID: https://hessian.cn/followme2
* 密码: followme2
> 连接信息可通过menuconfig进行修改

配网完成后首页会显示已连接的WiFi信息以及IP地址，点击屏幕左侧的Boot0按钮会进入LED控制页面并点亮Neopixel LED呼吸灯，在该页面长按Boot0可控制板载的Neopixel LED的颜色切换，颜色可在红绿蓝三种颜色之间进行切换。

> 该项目使用JetBrains CLion基于ESP-IDF进行开发。

## Follow me活动介绍
"Follow me活动”是DigiKey联合EEWORLD发起的为期一年的“跟技术大咖学技术，完成任务返现”活动。2023年共有4期，每3个月技术大咖推荐可玩性与可学性较强的开发板/仪器套件，带着大家实际操作。

* [活动首页](https://www.eeworld.com.cn/huodong/digikey_follow_me/)

## Adafruit ESP32-S3 TFT Feather简介
![image](https://github.com/adafruit/Adafruit-ESP32-S3-TFT-Feather-PCB/raw/main/assets/5483.jpg?raw=true)
Adafruit ESP32-S3 TFT Feather是由开源硬件行业知名公司Adafruit出品的一款富有特色的开源硬件，开发板使用乐鑫ESP32-S3芯片，支持WiFi和蓝牙能，自带高清TFT彩色显示屏。
* [Adafruit ESP32-S3 TFT Feather 产品信息](https://www.adafruit.com/product/5483)
* [Adafruit ESP32-S3 TFT Feather 使用指南](https://learn.adafruit.com/adafruit-esp32-s3-tft-feather)



## 第2期活动任务要求

#### 任务1：控制屏幕显示中文（必做任务）

> 完成屏幕的控制，并且能显示中文

#### 任务2：网络功能使用（必做任务）

> 完成网络功能的使用，能够创建热点和连接到WiFi

#### 任务3：控制WS2812B（必做任务）

> 使用按键控制板载Neopixel LED的显示和颜色切换

#### 任务4：从下方5个分任务中选择1个感兴趣的完成即可（必做任务）

> 分任务1：日历&时钟——完成一个可通过互联网更新的万年历时钟，并显示当地的天气信息
> 
> 分任务2：WS2812B效果控制——完成一个Neopixel(12灯珠或以上)控制器，通过按键和屏幕切换展示效果
> 
> 分任务3：数据检测与记录——按一定时间间隔连续记录温度/亮度信息，保存到SD卡，并可以通过按键调用查看之前的信息，并在屏幕上绘图
> 
> 分任务4：音乐播放功能——实现音乐播放器功能，可以在屏幕上显示列表信息和音乐信息，使用按键进行切换，使用扬声器进行播放
> 
> 分任务5：AI功能应用——结合运动传感器，完成手势识别功能，至少要识别三种手势(如水平左右、前后、垂直上下、水平画圈、垂直画圈，或者更复杂手势


#### 任务5：通过网络控制WS2812B（可选任务，非必做）
> 结合123，在手机上通过网络控制板载Neopixel LED的显示和颜色切换，屏幕同步显示状态
