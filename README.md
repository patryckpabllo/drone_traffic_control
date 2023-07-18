# Description
Tool set in order to build a distributed drone traffic monitoring system based on low latency messaging platform and a small software piece acting as co-processor working togher with a drone main processor.

## Software Stack
 - Ubuntu Desktop 18.04
 - Git
 - PX4 and MAVSDK Dev kit including simulator jMAVSim and QGroundstation
 - QGroundstation
 - Flogo Enterprise (Co-processor - Edge Computing)

# Setup the Environment
 - Go to $HOME dir
 - Create directory PX4 and enter inside: mkdir PX4; cd PX4
 - Install the Unbutu PX4 Firmware and Simulator: https://mavsdk.mavlink.io/develop/en/cpp/quickstart.html. 
   - Session to cover: Setting up a Simulator (don't skip step 3)
 - Build MAVSDK Library following: https://mavsdk.mavlink.io/develop/en/contributing/build.html#install-artifacts
 - Install QGroundstation: https://docs.qgroundcontrol.com/en/getting_started/download_and_install.html
 - Install Qt5 lib and QTCreator: http://web.stanford.edu/class/archive/cs/cs106b/cs106b.1154/qtcreator/installation/linux_gui_install/index.html
 - Download and Install TIBCO Flogo Enterprise: wwww.edelivery.tibco.com 
 
## Build 

- Go to $HOME/PX4 dir
- Clone the code: git clone https://github.com/patryckpabllo/drone_traffic_control.git
- Enter in drone_traffic_contol folder

- Build and install the driver
  - Go to mavlink_driver folder
  - Create build dir: mkdir build
  - Enter in build dir: cd build
  - type: cmake ..
  - after : make
    - Wait compile the driver 
  - Install the driver on system typing: make install
  - Test typing: http_server
  - You should see like below:
 
patryck@ubuntu:~/drone_traffic_control/mavlink_driver/build$ ./http_server 
[11:18:11|Info ] MAVSDK version: 0.28.0 (mavsdk_impl.cpp:26)
Usage : ./http_server <connection_url>
Connection URL format should be :
 For TCP : tcp://[server_host][:server_port]
 For UDP : udp://[bind_host][:bind_port]
 For Serial : serial:///path/to/serial/dev[:baudrate]
For example, to connect to the simulator use URL: udp://:14540

- Build Flogo Telemetry
  - Run the Flogo Web UI
  - Import the Telemetry project located in $HOME/PX4/drone_traffic_contol/flogo
  - Build the project
  - Download the executable to Download folder
  - Apply exection permission: chmod 777 telemetry 
  - Move to bin area: mv telemetry /usr/bin
  - Test typing: telemetry

- Build Flogo SensorFeed (Optional)
  - Run the Flogo Web UI
  - Import the Telemetry project located in $HOME/PX4/drone_traffic_contol/flogo
  - Build the project
  - Download the executable to Download folder
  - Apply exection permission: chmod 777 sensorFeed 
  - Move to bin area: mv sensorFeed /usr/bin
  - Test typing: sensorFeed

# Execute the solution
- Start the Simulator
  - Enter $HOME/PX4/Firmaware
  - No simulator display type: HEADLESS=1 make px4_sitl jmavsim
    - For display remove the HEADLESS variable
    
- Start the Flogo Telemetry application
  - type: FLOGO_APP_PROPS_ENV=auto Mqtt_streamhub_Broker_URL="52.67.94.207:1883" telemetry
  
- Start the Flogo SensorFeed application (Optional)
  - type: FLOGO_APP_PROPS_ENV=auto Mqtt_streamhub_Broker_URL="52.67.94.207:1883" sensorFilter="MTRFF0009993" port="80" sensorfeed
  
- Start mavlink driver
  -  type: http_server udp://:14540
  - 
