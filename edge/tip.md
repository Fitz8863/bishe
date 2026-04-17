如果开机的时候没有使用usb麦克风为输入源的时候，可以使用下面方式实现

执行 ： pactl list sources short

jetson@jetson-orin-nano:~$ pactl list sources short
1       alsa_output.usb-Yundea_Technology_Yundea_1076_415035333633350F-01.analog-stereo.monitor module-alsa-card.c      s16le 2ch 48000Hz  SUSPENDED
2       alsa_input.usb-Yundea_Technology_Yundea_1076_415035333633350F-01.mono-fallback  module-alsa-card.c      s16le 1ch 16000Hz SUSPENDED
3       alsa_output.platform-sound.analog-stereo.monitor        module-alsa-card.c      s16le 2ch 44100Hz       SUSPENDED
4       alsa_input.platform-sound.analog-stereo module-alsa-card.c      s16le 2ch 44100Hz       SUSPENDED


然后执行这个指令：pactl set-default-source +名称

立即切换（即时生效）
pactl set-default-source alsa_input.usb-Yundea_Technology_Yundea_1076_415035333633350F-01.mono-fallback