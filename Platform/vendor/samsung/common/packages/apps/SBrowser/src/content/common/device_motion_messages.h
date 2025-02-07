// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device motion.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START DeviceMotionMsgStart

IPC_STRUCT_BEGIN(DeviceMotionMsg_Updated_Params)
  // These fields have the same meaning as in device_motion::Motion.
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_x)
  IPC_STRUCT_MEMBER(double, acceleration_x)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_y)
  IPC_STRUCT_MEMBER(double, acceleration_y)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_z)
  IPC_STRUCT_MEMBER(double, acceleration_z)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_including_gravity_x)
  IPC_STRUCT_MEMBER(double, acceleration_including_gravity_x)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_including_gravity_y)
  IPC_STRUCT_MEMBER(double, acceleration_including_gravity_y)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_including_gravity_z)
  IPC_STRUCT_MEMBER(double, acceleration_including_gravity_z)
  IPC_STRUCT_MEMBER(bool, can_provide_rotation_rate_alpha)
  IPC_STRUCT_MEMBER(double, rotation_rate_alpha)
  IPC_STRUCT_MEMBER(bool, can_provide_rotation_rate_beta)
  IPC_STRUCT_MEMBER(double, rotation_rate_beta)
  IPC_STRUCT_MEMBER(bool, can_provide_rotation_rate_gamma)
  IPC_STRUCT_MEMBER(double, rotation_rate_gamma)
  IPC_STRUCT_MEMBER(bool, can_provide_interval)
  IPC_STRUCT_MEMBER(double, interval)
  #if defined(SBROWSER_DEVICEMOTION_IMPL)
  IPC_STRUCT_MEMBER(bool, can_provide_x)
  IPC_STRUCT_MEMBER(double, x)
  IPC_STRUCT_MEMBER(bool, can_provide_y)
  IPC_STRUCT_MEMBER(double, y)
  IPC_STRUCT_MEMBER(bool, can_provide_z)
  IPC_STRUCT_MEMBER(double, z)
  #endif
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Notification that the device's motion has changed.
IPC_MESSAGE_ROUTED1(DeviceMotionMsg_Updated,
                    DeviceMotionMsg_Updated_Params)

// Messages sent from the renderer to the browser.

// A RenderView requests to start receiving device motion updates.
IPC_MESSAGE_CONTROL1(DeviceMotionHostMsg_StartUpdating,
                     int /* render_view_id */)

// A RenderView requests to stop receiving device motion updates.
IPC_MESSAGE_CONTROL1(DeviceMotionHostMsg_StopUpdating,
                     int /* render_view_id */)
