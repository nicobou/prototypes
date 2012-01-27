--- Basic Gstreamer Pipeline
----------------------------
(framerate limited by CPU)

gst-launch-0.10 -v -m videotestsrc ! video/x-raw-yuv, width=1920, height=1080, framerate=30/1 ! gdppay ! shmsink socket-path=/tmp/testgdp5 shm-size=94967295

gst-launch shmsrc socket-path=/tmp/testgdp5 ! gdpdepay ! xvimagesink


