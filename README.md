# Description
Tool set in order to build a distributed drone traffic monitoring system based on low latency messaging platform and a small software piece acting as co-processor working togher with a drone main processor.

## Software Stack
 - Ubuntu Desktop 18.04
 - Git
 - PX4 and MAVSDK Dev kit including simulator jMAVSim and QGroundstation
 - QGroundstation
 - Flogo (Co-processor - Edge Computing)
 - MQTT (Transport Layer)

# Setup the Environment
 - Go to $HOME dir
 - Create directory PX4 and enter inside: mkdir PX4; cd PX4
 - Install the Unbutu PX4 Firmware and Simulator in https://mavsdk.mavlink.io/develop/en/cpp/quickstart.html. 
   - Session to cover: Setting up a Simulator (don't skip step 3)
 - Build MAVSDK Library following the instructions: https://mavsdk.mavlink.io/develop/en/contributing/build.html#install-artifacts
 - Install QGroundstation in https://docs.qgroundcontrol.com/en/getting_started/download_and_install.html
 - Install Qt5 lib and QTCreator in http://web.stanford.edu/class/archive/cs/cs106b/cs106b.1154/qtcreator/installation/linux_gui_install/index.html
 
## Build 

- Go to $HOME dir
- Clone the code: git clone https://github.com/patryckpabllo/drone_traffic_control.git

- Build the driver
  - Go to drone_traffic folder
  - Create build dir: mkdir build
  - Enter in build dir: cd build
  - type: cmake ..
  - after : make
  - Wait compile the driver 
  - Test typing: ./http_server
  - You could see like below:

patryck@ubuntu:~/drone_traffic_control/mavlink_driver/build$ ./http_server 
[11:18:11|Info ] MAVSDK version: 0.28.0 (mavsdk_impl.cpp:26)
Usage : ./http_server <connection_url>
Connection URL format should be :
 For TCP : tcp://[server_host][:server_port]
 For UDP : udp://[bind_host][:bind_port]
 For Serial : serial:///path/to/serial/dev[:baudrate]
For example, to connect to the simulator use URL: udp://:14540

- Build Flogo



# Execute the solution
