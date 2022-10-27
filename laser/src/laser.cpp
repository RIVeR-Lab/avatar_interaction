#include <ros/ros.h>

#include "math.h"
#include "std_msgs/String.h"
#include "geometry_msgs/Point.h"
#include "dynamixel_sdk_examples/GetPosition.h"
#include "dynamixel_sdk_examples/SetPosition.h"
#include "dynamixel_sdk/dynamixel_sdk.h"

#include "franka_control/PTIPacket.h"

using namespace dynamixel;

// Control table address
#define ADDR_TORQUE_ENABLE    64
#define ADDR_GOAL_POSITION    116
#define ADDR_PRESENT_POSITION 132

// Protocol version
#define PROTOCOL_VERSION      2.0             // Default Protocol version of DYNAMIXEL X series.

// Default setting
#define DXL1_ID               1               // DXL1 ID
#define DXL2_ID               2               // DXL2 ID

#define BAUDRATE              3000000           // Default Baudrate of DYNAMIXEL X series
#define DEVICE_NAME           "/dev/ttyUSB0"

unsigned int CURRENT_POSITION_RIGHT = 1540; //1540 = 180deg
float RIGHT_DEG_PER_TICK = 0.00151;
unsigned int CURRENT_POSITION_LEFT = 2560;
const unsigned int MAX_LEFT_POSITION = 3580;
const unsigned int MIN_RIGHT_POSITION = 1530;
float LEFT_DEG_PER_TICK = 0.00151;
float Z_VAL = 0.8128;
float X_VAL = 0.4826;
int tick_lim = 4000;

PortHandler * portHandler;
PacketHandler * packetHandler;

bool getPresentPositionCallback(
  dynamixel_sdk_examples::GetPosition::Request & req,
  dynamixel_sdk_examples::GetPosition::Response & res)
{
  uint8_t dxl_error = 0;
  int dxl_comm_result = COMM_TX_FAIL;

  // Position Value of X series is 4 byte data. For AX & MX(1.0) use 2 byte data(int16_t) for the Position Value.
  int32_t position = 0;

  // Read Present Position (length : 4 bytes) and Convert uint32 -> int32
  // When reading 2 byte data from AX / MX(1.0), use read2ByteTxRx() instead.
  dxl_comm_result = packetHandler->read4ByteTxRx(
    portHandler, (uint8_t)req.id, ADDR_PRESENT_POSITION, (uint32_t *)&position, &dxl_error);
  if (dxl_comm_result == COMM_SUCCESS) {
    ROS_INFO("getPosition : [ID:%d] -> [POSITION:%d]", req.id, position);
    res.position = position;
    return true;
  } else {
    ROS_INFO("Failed to get position! Result: %d", dxl_comm_result);
    return false;
  }
}

float calc_angle(float x, float z, bool isRight)
{
        float sum_of_squares = (x * x) + (z * z);
        float dist = sqrt(sum_of_squares);
        float frac = x / dist;
        float theta = asin(frac);
	return theta - 1.5708;
}

unsigned int calc_ticks(float theta, bool isRight)
{
        if (isRight) {
		float ticks = theta / RIGHT_DEG_PER_TICK;
		return (unsigned int) ticks;
	}

	float ticks = theta / LEFT_DEG_PER_TICK;
        return (unsigned int)ticks;
}

void rightCallback(const franka_control::PTIPacket & msg)
{
  uint8_t dxl_error = 0;
  int dxl_comm_result = COMM_TX_FAIL;

  float angle = calc_angle(msg.position.x + X_VAL , Z_VAL, true);
  unsigned int ticks = MIN_RIGHT_POSITION - calc_ticks(angle, true);
  CURRENT_POSITION_RIGHT = ticks;
  ROS_INFO("Angle: %f, ticks: %u", angle, ticks);

  // Write Goal Position (length : 4 bytes)
  // When writing 2 byte data to AX / MX(1.0), use write2ByteTxRx() instead.
  dxl_comm_result = packetHandler->write4ByteTxRx(
    portHandler, DXL1_ID, ADDR_GOAL_POSITION, CURRENT_POSITION_RIGHT, &dxl_error);
  if (dxl_comm_result == COMM_SUCCESS) {
    ROS_INFO("setPositionRight : [ID:%d] [POSITION:%d]", DXL1_ID, CURRENT_POSITION_RIGHT);
  } else {
    ROS_ERROR("Failed to set position! Result: %d", dxl_comm_result);
  }
}

void leftCallback(const franka_control::PTIPacket & msg)
{
  uint8_t dxl_error = 0;
  int dxl_comm_result = COMM_TX_FAIL;

  float angle = calc_angle(msg.position.x + X_VAL , Z_VAL, false);
  unsigned int ticks = MAX_LEFT_POSITION + calc_ticks(angle, false);
  CURRENT_POSITION_LEFT = ticks;
  ROS_INFO("Angle: %f, ticks: %u", angle, ticks);

  // Write Goal Position (length : 4 bytes)
  // When writing 2 byte data to AX / MX(1.0), use write2ByteTxRx() instead.
  dxl_comm_result = packetHandler->write4ByteTxRx(
    portHandler, DXL2_ID, ADDR_GOAL_POSITION, CURRENT_POSITION_LEFT, &dxl_error);
  if (dxl_comm_result == COMM_SUCCESS) {
    ROS_INFO("setPositionRight : [ID:%d] [POSITION:%d]", DXL2_ID, CURRENT_POSITION_LEFT);
  } else {
    ROS_ERROR("Failed to set position! Result: %d", dxl_comm_result);
  }
}

int main(int argc, char ** argv)
{
  uint8_t dxl_error = 0;
  int dxl_comm_result = COMM_TX_FAIL;

  portHandler = PortHandler::getPortHandler(DEVICE_NAME);
  packetHandler = PacketHandler::getPacketHandler(PROTOCOL_VERSION);

  if (!portHandler->openPort()) {
    ROS_ERROR("Failed to open the port!");
    return -1;
  }

  if (!portHandler->setBaudRate(BAUDRATE)) {
    ROS_ERROR("Failed to set the baudrate!");
    return -1;
  }

  dxl_comm_result = packetHandler->write1ByteTxRx(
    portHandler, DXL1_ID, ADDR_TORQUE_ENABLE, 1, &dxl_error);
  if (dxl_comm_result != COMM_SUCCESS) {
    ROS_ERROR("Failed to enable torque for Dynamixel ID %d", DXL1_ID);
    printf("ERROR: %s\n", packetHandler->getRxPacketError(dxl_error));
    return -1;
  }

  dxl_comm_result = packetHandler->write1ByteTxRx(
    portHandler, DXL2_ID, ADDR_TORQUE_ENABLE, 1, &dxl_error);
  if (dxl_comm_result != COMM_SUCCESS) {
    ROS_ERROR("Failed to enable torque for Dynamixel ID %d", DXL2_ID);
    printf("ERROR: %s\n", packetHandler->getRxPacketError(dxl_error));
    return -1;
  }

  ros::init(argc, argv, "read_write_node");
  ros::NodeHandle nh;
  ros::ServiceServer get_position_srv = nh.advertiseService("/get_position", getPresentPositionCallback);
  ros::Subscriber right_sub = nh.subscribe("/pti_right_output", 1, rightCallback);
  ros::Subscriber left_sub = nh.subscribe("/pti_left_output", 1, leftCallback);
  ros::spin();

  portHandler->closePort();
  return 0;
}
