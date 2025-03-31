1、参考这个必须得环境
https://www.bearpi.cn/dev_board/bearpi/hm/nano/software_OpenHarmony/BearPi-HM_Nano%E5%8D%81%E5%88%86%E9%92%9F%E4%B8%8A%E6%89%8B.html
十分钟上手教程
    主要是
    Ubuntu18.4镜像OVF	开发板编译代码所需的环境
    Hiburn	代码烧录工具
    VS Code	用于编写代码
    
2、在虚拟机中克隆源码  http://192.168.10.249:8888/OpenHarmony/HarmonyOS-BearPi.git
3、修改工程指定
    打开 HarmonyOS-BearPi/applications/BearPi/BearPi-HM_Nano/sample/BUILD.gn这个文件
        #"sensor_sf1:application_mian",
        "sensor_ia1:application_mian",
        #"sensor_sc1:application_mian",
        #"sensor_sc2:application_mian",
        #"sensor_is1:application_mian",
    有如上5个sensor示例，选择其中一个
4、在根目录中编译代码
    cd HarmonyOS-BearPi
    python build.py BearPi-HM_Nano      编译代码
5、编译完成后
    在HarmonyOS-BearPi/out/BearPi-HM_Nano/目录中得到新的Hi3861_wifiiot_app_allinone.bin文件
    将其拷贝到window端，使用HiBurn.exe工具下载
    可以参考 https://www.bearpi.cn/dev_board/bearpi/hm/nano/software_OpenHarmony/BearPi-HM_Nano%E5%8D%81%E5%88%86%E9%92%9F%E4%B8%8A%E6%89%8B.html#%E4%B9%9D%E3%80%81%E4%B8%8B%E8%BD%BD%E7%A8%8B%E5%BA%8F
    在在Com settings中设置Baud为：921600，点击确定，这里可以设置最大波特率3000000，下载速度最快
6、下载时候有时候会出现错误，即复位后，连接调试串口有时候会出现校验错误，返回步骤5再下载一次可以解决
7、下载完成后，由于下载器默认是全片擦除，导致没有wifi信息，需要使用xlabtools工具设置wifi等信息（兼容8266命令集）。
8、有时候设置wifi信息时候会出现AT返回busy，再次写入即可
9、由于只有一个LED灯，另一个LED灯是给NFC使用的，所以网络灯和数据灯共用了，网络灯常亮连接智云成功






