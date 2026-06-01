/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  Author(s):  Anton Deguet
  Created on: 2016-11-10

  (C) Copyright 2016-2025 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#include <cisstVector/vctFixedSizeVectorTypes.h>
#include <cisstVector/vctTransformationTypes.h>
#include <cisstMultiTask/mtsInterfaceProvided.h>
#include <cisstOSAbstraction/osaSleep.h>
#include <cisstOSAbstraction/osaGetTime.h>
#include <sawHaplyService/mtsHaply.h>

#include <fstream>

#include <cisstParameterTypes/prmBaseFrame.h>
#include <cisstParameterTypes/prmOperatingState.h>
#include <cisstParameterTypes/prmPositionCartesianGet.h>
#include <cisstParameterTypes/prmPositionCartesianSet.h>
#include <cisstParameterTypes/prmVelocityCartesianGet.h>
#include <cisstParameterTypes/prmForceCartesianGet.h>
#include <cisstParameterTypes/prmForceCartesianSet.h>
#include <cisstParameterTypes/prmForceTorqueJointSet.h>
#include <cisstParameterTypes/prmStateCartesian.h>
#include <cisstParameterTypes/prmStateJoint.h>
#include <cisstParameterTypes/prmConfigurationJoint.h>
#include <cisstParameterTypes/prmEventButton.h>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <json/json.h>
#include <cisstCommon/cmnDataFunctionsJSON.h>
#include <cisstVector/vctDataFunctionsTransformationsJSON.h>

#include <algorithm>

CMN_IMPLEMENT_SERVICES_DERIVED_ONEARG(mtsHaply, mtsTaskContinuous, mtsTaskContinuousConstructorArg);

class mtsHaplySocket
{
 public:
    typedef websocketpp::client<websocketpp::config::asio_client> ws_client_t;
    ws_client_t m_ws_client;
    websocketpp::connection_hdl m_ws_hdl;
    std::string m_ws_uri;
    bool m_ws_connected = false;
    Json::Value m_ws_data;
    Json::Value m_ws_command;

    mtsHaplySocket() {}
    ~mtsHaplySocket() {
        if (m_ws_connected) {
            websocketpp::lib::error_code ec;
            m_ws_client.close(m_ws_hdl, websocketpp::close::status::normal, "", ec);
        }
    }

    void Configure(const std::string & uri) {
        m_ws_uri = uri;
        m_ws_client.init_asio();
        m_ws_client.clear_access_channels(websocketpp::log::alevel::all);
        m_ws_client.clear_error_channels(websocketpp::log::elevel::all);

        m_ws_client.set_message_handler([this](websocketpp::connection_hdl, ws_client_t::message_ptr msg) {
            if (msg->get_opcode() != websocketpp::frame::opcode::text) {
                return;
            }
            Json::Value incoming;
            Json::Reader reader;
            if (reader.parse(msg->get_payload(), incoming)) {
                // Merge top-level arrays which contain multiple devices
                for (auto const & key : incoming.getMemberNames()) {
                    if (incoming[key].isArray()) {
                        for (unsigned int i = 0; i < incoming[key].size(); ++i) {
                            const Json::Value & item = incoming[key][i];
                            bool found = false;
                            if (m_ws_data.isMember(key) && m_ws_data[key].isArray()) {
                                for (unsigned int j = 0; j < m_ws_data[key].size(); ++j) {
                                    if (m_ws_data[key][j].isMember("device_id") && item.isMember("device_id")
                                        && m_ws_data[key][j]["device_id"] == item["device_id"]) {
                                        m_ws_data[key][j] = item;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found) {
                                m_ws_data[key].append(item);
                            }
                        }
                    }
                    else {
                        // For non-array members, just update (e.g. session info)
                        m_ws_data[key] = incoming[key];
                    }
                }
            }
        });
        websocketpp::lib::error_code ec;
        ws_client_t::connection_ptr con = m_ws_client.get_connection(m_ws_uri, ec);
        if (ec) {
            return;
        }
        m_ws_client.connect(con);
        m_ws_hdl = con->get_handle();
        m_ws_connected = true;
    }

    void SendRequest(const std::string & request) {
        if (m_ws_connected) {
            websocketpp::lib::error_code ec;
            m_ws_client.send(m_ws_hdl, request, websocketpp::frame::opcode::text, ec);
            if (ec) {
                return;
            }
            m_ws_client.poll();
        }
    }

    void Poll() {
        if (m_ws_connected) {
            m_ws_client.poll();
        }
    }
};

class mtsHaplyDevice
{
 public:
    typedef std::list<mtsInterfaceProvided *> ButtonInterfaces;

    mtsHaplyDevice(const std::string & inverse3Id,
                   const std::string & verseGripId,
                   const std::string & name,
                   const prmBaseFrame & baseFrame,
                   const bool emulateGripper,
                   const double gripperRate,
                   const double gripperMin,
                   const double gripperMax,
                   mtsStateTable * stateTable,
                   mtsInterfaceProvided * interfaceProvided,
                   const ButtonInterfaces & buttonInterfaces,
                   mtsHaplySocket * websocket);
    ~mtsHaplyDevice();
    void Startup(void);
    void Run(void);
    void Cleanup(void);
    inline const std::string & Name(void) const { return m_name; }

    void GetButtonNames(std::list<std::string> & result) const;
    void GetConfigurationJs(prmConfigurationJoint & result) const;

 protected:
    std::string reference_frame(void) const;
    const std::string & moving_frame(void) const;
    void GetRobotData(void);
    void update_measured_cs(void);
    void SetControlMode(const mtsHaply::ControlModeType & mode);

    mtsHaplySocket * m_websocket;

    // crtk state
    void state_command(const std::string & command);
    void emit_operating_state_event(void);
    prmOperatingState m_operating_state;
    mtsFunctionWrite m_operating_state_event;

    void servo_cp(const prmPositionCartesianSet & newPosition);
    void body_servo_cf(const prmForceCartesianSet & newForce);
    void use_gravity_compensation(const bool & gravityCompensation);
    void set_base_frame(const prmPositionCartesianSet & baseFrame);
    void hold(void);
    void free(void);

    std::string m_inverse3_id;
    std::string m_verse_grip_id;
    std::string m_name;
    prmBaseFrame m_base_frame;
    struct {
        bool emulate;
        double rate;
        double min;
        double max;
    } m_gripper;
    mtsStateTable * m_state_table;
    mtsInterfaceProvided * m_interface;

    uint mPreviousButtonMask;
    struct ButtonData {
        std::string Name;
        mtsFunctionWrite Function;
        bool Pressed;
    };
    typedef std::list<ButtonData *> ButtonsData;

    ButtonsData mButtonCallbacks;

    prmStateCartesian m_local_measured_cs, m_measured_cs;
    prmPositionCartesianGet m_setpoint_cp;
    prmForceCartesianGet m_body_measured_cf;
    prmStateJoint m_gripper_measured_js;
    prmConfigurationJoint m_gripper_configuration_js;

    mtsHaply::ControlModeType mControlMode;

    bool m_new_servo_cp;
    prmPositionCartesianSet m_servo_cp;
    prmForceCartesianSet m_body_servo_cf;
    bool m_use_gravity_compensation;

    bool m_info_sent = false;
    bool m_first_data_sent = false;
    bool m_first_battery_sent = false;
    bool m_is_calibrated = false;
    bool m_calibration_warning_sent = false;
    bool m_awake_warning_sent = false;
    int m_last_battery_decile = 10;
};


mtsHaplyDevice::mtsHaplyDevice(const std::string & inverse3Id,
                               const std::string & verseGripId,
                               const std::string & name,
                               const prmBaseFrame & baseFrame,
                               const bool emulateGripper,
                               const double gripperRate,
                               const double gripperMin,
                               const double gripperMax,
                               mtsStateTable * stateTable,
                               mtsInterfaceProvided * interfaceProvided,
                               const mtsHaplyDevice::ButtonInterfaces & buttonInterfaces,
                               mtsHaplySocket * websocket):
    m_websocket(websocket),
    m_inverse3_id(inverse3Id),
    m_verse_grip_id(verseGripId),
    m_name(name),
    m_base_frame(baseFrame),
    m_state_table(stateTable),
    m_interface(interfaceProvided) {

    m_gripper.emulate = emulateGripper;
    m_gripper.rate = gripperRate;
    m_gripper.min = gripperMin;
    m_gripper.max = gripperMax;

    m_operating_state.IsBusy() = false;
    m_operating_state.SetValid(true);

    m_new_servo_cp = false;
    mControlMode = mtsHaply::UNDEFINED;
    m_use_gravity_compensation = true;

    m_body_servo_cf.Force().SetAll(0.0);

    m_local_measured_cs.ReferenceFrame() = reference_frame();
    m_local_measured_cs.MovingFrame() = moving_frame();
    m_local_measured_cs.Position().Assign(vctFrm3::Identity());
    m_local_measured_cs.PositionIsValid() = false;
    m_local_measured_cs.Velocity().Assign(vct6(0.0));
    m_local_measured_cs.VelocityIsValid() = false;
    m_local_measured_cs.Force().Assign(vct6(0.0));
    m_local_measured_cs.ForceIsValid() = false;
    update_measured_cs();

    m_setpoint_cp.SetReferenceFrame(m_base_frame.reference_frame());
    m_setpoint_cp.SetMovingFrame(m_name);
    m_setpoint_cp.SetValid(false);
    m_gripper_measured_js.Name().resize(1);
    m_gripper_measured_js.Name()[0] = "gripper";
    m_gripper_measured_js.Position().SetSize(1);
    m_gripper_measured_js.Position()[0] = m_gripper.max;
    m_gripper_measured_js.SetValid(true);

    m_gripper_configuration_js.Name().resize(1);
    m_gripper_configuration_js.Name()[0] = "gripper";
    m_gripper_configuration_js.PositionMin().SetSize(1);
    m_gripper_configuration_js.PositionMin()[0] = m_gripper.min;
    m_gripper_configuration_js.PositionMax().SetSize(1);
    m_gripper_configuration_js.PositionMax()[0] = m_gripper.max;

    m_state_table->SetAutomaticAdvance(false);
    m_state_table->AddData(m_operating_state, "operating_state");
    m_state_table->AddData(m_base_frame, "base_frame");
    m_state_table->AddData(m_local_measured_cs, "local/measured_cs");
    m_state_table->AddData(m_measured_cs, "measured_cs");
    m_state_table->AddData(m_body_measured_cf, "body/measured_cf");
    m_state_table->AddData(m_gripper_measured_js, "gripper/measured_js");
    m_state_table->AddData(m_setpoint_cp, "setpoint_cp");

    if (m_interface) {
        // system messages
        m_interface->AddMessageEvents();
        // state
        m_interface->AddCommandReadState(*m_state_table, m_base_frame, "base_frame");
        m_interface->AddCommandReadState(*m_state_table, m_local_measured_cs, "local/measured_cs");
        m_interface->AddCommandFilteredReadState(*m_state_table, m_local_measured_cs,
                                                 prmStateCartesian::ToPositionCartesianGet,
                                                 "local/measured_cp");
        m_interface->AddCommandFilteredReadState(*m_state_table, m_local_measured_cs,
                                                 prmStateCartesian::ToVelocityCartesianGet,
                                                 "local/measured_cv");
        m_interface->AddCommandReadState(*m_state_table, m_measured_cs, "measured_cs");
        m_interface->AddCommandFilteredReadState(*m_state_table, m_measured_cs,
                                                 prmStateCartesian::ToPositionCartesianGet,
                                                 "measured_cp");
        m_interface->AddCommandFilteredReadState(*m_state_table, m_measured_cs,
                                                 prmStateCartesian::ToVelocityCartesianGet,
                                                 "measured_cv");
        m_interface->AddCommandReadState(*m_state_table, m_body_measured_cf, "body/measured_cf");
        m_interface->AddCommandReadState(*m_state_table, m_gripper_measured_js, "gripper/measured_js");
        m_interface->AddCommandReadState(*m_state_table, m_setpoint_cp, "setpoint_cp");
        // commands
        m_interface->AddCommandWrite(&mtsHaplyDevice::servo_cp, this, "servo_cp");
        m_interface->AddCommandWrite(&mtsHaplyDevice::body_servo_cf, this, "body/servo_cf");
        m_interface->AddCommandWrite(&mtsHaplyDevice::use_gravity_compensation, this, "use_gravity_compensation");
        m_interface->AddCommandWrite(&mtsHaplyDevice::set_base_frame, this, "set_base_frame");
        m_interface->AddCommandVoid(&mtsHaplyDevice::hold, this, "hold");
        m_interface->AddCommandVoid(&mtsHaplyDevice::free, this, "free");
        // configuration
        m_interface->AddCommandRead(&mtsHaplyDevice::GetButtonNames, this, "get_button_names");
        m_interface->AddCommandRead(&mtsHaplyDevice::GetConfigurationJs, this, "gripper/get_configuration_js");
        // robot State
        m_interface->AddCommandWrite(&mtsHaplyDevice::state_command, this, "state_command", std::string(""));
        m_interface->AddCommandReadState(*m_state_table, m_operating_state, "operating_state");
        m_interface->AddEventWrite(m_operating_state_event, "operating_state", prmOperatingState());
        // stats
        m_interface->AddCommandReadState(*m_state_table, m_state_table->PeriodStats, "period_statistics");
    }

    // buttons
    const ButtonInterfaces::const_iterator endButtons = buttonInterfaces.end();
    ButtonInterfaces::const_iterator buttonInterface;
    for (buttonInterface = buttonInterfaces.begin(); buttonInterface != endButtons; ++buttonInterface) {
        ButtonData * data = new ButtonData;
        mButtonCallbacks.push_back(data);
        data->Name = (*buttonInterface)->GetName();
        data->Pressed = false;
        (*buttonInterface)->AddEventWrite(data->Function, "Button", prmEventButton());
    }
}

mtsHaplyDevice::~mtsHaplyDevice(void) {
    for (auto data : mButtonCallbacks) {
        delete data;
    }
    mButtonCallbacks.clear();
    delete m_state_table;
}

std::string mtsHaplyDevice::reference_frame(void) const
{
    return m_name + "_base";
}

const std::string & mtsHaplyDevice::moving_frame(void) const
{
    return m_name;
}

void mtsHaplyDevice::update_measured_cs(void)
{
    if (!m_base_frame.Valid() || !m_base_frame.Fixed()) {
        m_measured_cs = m_local_measured_cs;
        m_measured_cs.PositionIsValid() = false;
        m_measured_cs.VelocityIsValid() = false;
        m_measured_cs.ForceIsValid() = false;
        return;
    }

    try {
        m_base_frame.ApplyTo(m_local_measured_cs, m_measured_cs);
    } catch (std::exception & exception) {
        if (m_interface) {
            m_interface->SendError(m_name + ": failed to apply base_frame (" + exception.what() + ")");
        }
        m_measured_cs = m_local_measured_cs;
        m_measured_cs.PositionIsValid() = false;
        m_measured_cs.VelocityIsValid() = false;
        m_measured_cs.ForceIsValid() = false;
    }
}

void mtsHaplyDevice::Startup(void) {
    std::stringstream ss;
    ss << m_name << ": properly initialized with ID 0x" << m_inverse3_id;
    if (!m_verse_grip_id.empty()) {
        ss << " and verse_grip ID 0x" << m_verse_grip_id;
    }
    m_interface->SendStatus(ss.str());

    // update current state
    m_state_table->Start();
    m_operating_state.IsHomed() = m_is_calibrated;
    m_operating_state.State() = prmOperatingState::ENABLED;
    emit_operating_state_event();

    GetRobotData();
    this->free();
    m_state_table->Advance();
}

void mtsHaplyDevice::Run(void) {
    m_state_table->Start();
    // process mts commands
    m_interface->ProcessMailBoxes();

    GetRobotData();

    // build command for this device
    Json::Value inverse3_command;
    inverse3_command["device_id"] = m_inverse3_id;
    Json::Value commands = Json::Value(Json::objectValue);

    switch (mControlMode) {
    case mtsHaply::SERVO_CF: {
        vct3 force(m_body_servo_cf.Force()[0], m_body_servo_cf.Force()[1], m_body_servo_cf.Force()[2]);
        vct3 forceInBase = force;
        if (m_base_frame.Valid() && m_base_frame.Fixed()) {
            vctFrm3 baseFrame;
            baseFrame.From(m_base_frame.transform());
            forceInBase = baseFrame.Rotation().Transpose() * force;
        }
        Json::Value values;
        values["x"] = forceInBase.X();
        values["y"] = forceInBase.Y();
        values["z"] = forceInBase.Z();
        commands["set_cursor_force"]["values"] = values;
        commands["set_gravity_compensation"] = m_use_gravity_compensation;
        m_setpoint_cp.Position().Assign(m_measured_cs.Position());
        m_setpoint_cp.SetReferenceFrame(m_measured_cs.ReferenceFrame());
        m_setpoint_cp.SetMovingFrame(m_measured_cs.MovingFrame());
        m_setpoint_cp.SetValid(m_measured_cs.PositionIsValid());
    } break;
    case mtsHaply::SERVO_CP: {
        vct3 goalInBase = m_servo_cp.Goal().Translation();
        if (m_base_frame.Valid() && m_base_frame.Fixed()) {
            vctFrm3 baseFrame;
            baseFrame.From(m_base_frame.transform());
            goalInBase = baseFrame.Inverse() * m_servo_cp.Goal().Translation();
        }
        Json::Value values;
        values["x"] = goalInBase.X();
        values["y"] = goalInBase.Y();
        values["z"] = goalInBase.Z();
        commands["set_cursor_position"]["values"] = values;
        commands["set_gravity_compensation"] = m_use_gravity_compensation;
        m_setpoint_cp.Position().Assign(m_servo_cp.Goal());
        m_setpoint_cp.SetReferenceFrame(m_base_frame.reference_frame());
        m_setpoint_cp.SetMovingFrame(m_name);
        m_setpoint_cp.SetValid(true);
    } break;
    default:
        commands["probe_position"] = Json::Value(Json::objectValue);
        break;
    }
    inverse3_command["commands"] = commands;
    m_websocket->m_ws_command["inverse3"].append(inverse3_command);

    if (!m_verse_grip_id.empty()) {
        Json::Value grip_command;
        grip_command["device_id"] = m_verse_grip_id;
        Json::Value grip_commands = Json::Value(Json::objectValue);
        grip_commands["probe_orientation"] = Json::Value(Json::objectValue);
        grip_command["commands"] = grip_commands;
        // try wired first, then others?
        // actually we should know the type from discovery or just send to all
        m_websocket->m_ws_command["verse_grip"].append(grip_command);
        m_websocket->m_ws_command["custom_verse_grip"].append(grip_command);
        m_websocket->m_ws_command["wireless_verse_grip"].append(grip_command);
    }

    m_state_table->Advance();
}

void mtsHaplyDevice::Cleanup(void) {
    // Send a command to set force to zero and disable gravity compensation
    Json::Value inverse3_command;
    inverse3_command["device_id"] = m_inverse3_id;
    Json::Value commands = Json::Value(Json::objectValue);
    Json::Value values;
    values["x"] = 0.0;
    values["y"] = 0.0;
    values["z"] = 0.0;
    commands["set_cursor_force"]["values"] = values;
    commands["set_gravity_compensation"] = false;
    inverse3_command["commands"] = commands;
    m_websocket->m_ws_command["inverse3"].append(inverse3_command);
}

void mtsHaplyDevice::GetButtonNames(std::list<std::string> & result) const {
    result.clear();
    const ButtonsData::const_iterator end = mButtonCallbacks.end();
    ButtonsData::const_iterator button;
    for (button = mButtonCallbacks.begin(); button != end; ++button) {
        result.push_back((*button)->Name);
    }
}

void mtsHaplyDevice::GetConfigurationJs(prmConfigurationJoint & result) const {
    result = m_gripper_configuration_js;
}

void mtsHaplyDevice::GetRobotData(void) {
    if (m_websocket && m_websocket->m_ws_connected) {
        // Inverse3 data
        if (m_websocket->m_ws_data.isMember("inverse3")) {
            const Json::Value devices = m_websocket->m_ws_data["inverse3"];
            if (devices.isArray()) {
                bool foundInverse3 = false;
                for (unsigned int i = 0; i < devices.size(); ++i) {
                    if (devices[i]["device_id"].asString() == m_inverse3_id) {
                        foundInverse3 = true;
                        if (!m_info_sent) {
                            const Json::Value deviceNode = devices[i];
                            const Json::Value config
                                = deviceNode.isMember("config") ? deviceNode["config"] : deviceNode;
                            const Json::Value info = config.isMember("device_info") ? config["device_info"] : config;
                            std::stringstream ss;
                            ss << m_name << ": found 0x" << m_inverse3_id;
                            if (info.isMember("device_type")) {
                                ss << ", type=" << info["device_type"].asInt();
                            }
                            if (info.isMember("major_version") && info.isMember("minor_version")) {
                                ss << ", version=" << info["major_version"].asInt() << "."
                                   << info["minor_version"].asInt();
                            }
                            m_interface->SendStatus(ss.str());
                            m_info_sent = true;
                        }
                        const Json::Value state = devices[i]["state"];
                        const Json::Value status = devices[i]["status"];
                        if (status.isMember("calibrated")) {
                            bool was_calibrated = m_is_calibrated;
                            m_is_calibrated = status["calibrated"].asBool();
                            if (m_is_calibrated != was_calibrated) {
                                m_operating_state.IsHomed() = m_is_calibrated;
                                emit_operating_state_event();
                                if (m_is_calibrated) {
                                    m_interface->SendStatus(m_name + ": device is calibrated.");
                                }
                            }
                        }
                        else if (state.isMember("calibration_state")) {
                            bool was_calibrated = m_is_calibrated;
                            m_is_calibrated = (state["calibration_state"].asInt() >= 2); // 2 is typically "calibrated"
                            if (m_is_calibrated != was_calibrated) {
                                m_operating_state.IsHomed() = m_is_calibrated;
                                emit_operating_state_event();
                                if (m_is_calibrated) {
                                    m_interface->SendStatus(m_name + ": device is calibrated.");
                                }
                            }
                        }
                        if (!m_is_calibrated) {
                            if (!m_calibration_warning_sent) {
                                m_interface->SendWarning(m_name + ": device is not calibrated. Please place "
                                                                  "the end-effector in the home position.");
                                m_calibration_warning_sent = true;
                            }
                            m_local_measured_cs.PositionIsValid() = false;
                            m_local_measured_cs.VelocityIsValid() = false;
                        }
                        else {
                            m_calibration_warning_sent = false;
                        }

                        if (state.isMember("cursor_position")) {
                            vct3 pos(state["cursor_position"]["x"].asDouble(),
                                     state["cursor_position"]["y"].asDouble(),
                                     state["cursor_position"]["z"].asDouble());
                            m_local_measured_cs.Position().Translation() = pos;
                            if (m_is_calibrated) {
                                m_local_measured_cs.PositionIsValid() = true;
                            }

                            vct3 vel(state["cursor_velocity"]["x"].asDouble(),
                                     state["cursor_velocity"]["y"].asDouble(),
                                     state["cursor_velocity"]["z"].asDouble());
                            m_local_measured_cs.Velocity().XYZ().Assign(vel);
                            if (m_is_calibrated) {
                                m_local_measured_cs.VelocityIsValid() = true;
                            }
                            // Log that we have the first set of data
                            if (!m_first_data_sent) {
                                std::stringstream ss;
                                ss << m_name << ": first data point received (" << devices.size()
                                   << " devices in cache)";
                                m_interface->SendStatus(ss.str());
                                m_first_data_sent = true;
                            }
                        }
                        else {
                            m_interface->SendWarning(m_name + ": state contains no cursor_position");
                            m_local_measured_cs.PositionIsValid() = false;
                            m_local_measured_cs.VelocityIsValid() = false;
                        }
                        break;
                    }
                }
                if (!foundInverse3 && !m_info_sent) {
                    std::stringstream ss;
                    ss << m_name << ": device 0x" << m_inverse3_id << " not found in [";
                    for (unsigned int i = 0; i < devices.size(); ++i) {
                        ss << "\"0x" << devices[i]["device_id"].asString() << "\"";
                        if (i < devices.size() - 1)
                            ss << ", ";
                    }
                    ss << "]";
                    m_interface->SendStatus(ss.str());
                    // m_info_sent = true;
                }
            }
        }
        else {
            // No inverse3 key in m_ws_data
            // m_interface->SendWarning(m_name + ": m_ws_data has no inverse3");
        }

        // VerseGrip data for orientation
        if (!m_verse_grip_id.empty()) {
            const char * gripCollections[] = {"verse_grip", "custom_verse_grip", "wireless_verse_grip"};
            bool foundGrip = false;
            for (int c = 0; c < 3; ++c) {
                if (m_websocket->m_ws_data.isMember(gripCollections[c])) {
                    const Json::Value grips = m_websocket->m_ws_data[gripCollections[c]];
                    if (grips.isArray()) {
                        for (unsigned int i = 0; i < grips.size(); ++i) {
                            if (grips[i]["device_id"].asString() == m_verse_grip_id) {
                                foundGrip = true;
                                const Json::Value state = grips[i]["state"];
                                const Json::Value status = grips[i]["status"];

                                if (status.isMember("awake") && !status["awake"].asBool()) {
                                    if (!m_awake_warning_sent) {
                                        m_interface->SendWarning(m_name + ": verse grip is not awake. Please press the "
                                                                          "main button to wake it up.");
                                        m_awake_warning_sent = true;
                                    }
                                }
                                else {
                                    if (m_awake_warning_sent) {
                                        m_interface->SendStatus(m_name + ": verse grip is awake.");
                                        m_awake_warning_sent = false;
                                    }
                                }

                                if (state.isMember("orientation")) {
                                    vctQuatRot3 quat(state["orientation"]["x"].asDouble(),
                                                     state["orientation"]["y"].asDouble(),
                                                     state["orientation"]["z"].asDouble(),
                                                     state["orientation"]["w"].asDouble(),
                                                     VCT_NORMALIZE);
                                    vctMatRot3 rot(quat);
                                    m_local_measured_cs.Position().Rotation().Assign(rot);
                                }

                                // Buttons
                                if (state.isMember("buttons")) {
                                    const Json::Value buttons = state["buttons"];
                                    if (buttons.isObject()) {
                                        std::vector<bool> pressed(mButtonCallbacks.size(), false);
                                        // Map JSON keys to button indices: Home=0, A=1, B=2, C=3
                                        if (buttons.isMember("home") && (pressed.size() > 0))
                                            pressed[0] = buttons["home"].asBool();
                                        if (buttons.isMember("a") && (pressed.size() > 1))
                                            pressed[1] = buttons["a"].asBool();
                                        if (buttons.isMember("b") && (pressed.size() > 2))
                                            pressed[2] = buttons["b"].asBool();
                                        if (buttons.isMember("c") && (pressed.size() > 3))
                                            pressed[3] = buttons["c"].asBool();

                                        if (m_gripper.emulate && (pressed.size() > 2)) {
                                            const bool close_pressed = pressed[1]; // A
                                            const bool open_pressed = pressed[2];  // B
                                            if (close_pressed != open_pressed) {
                                                double gripper = m_gripper_measured_js.Position()[0];
                                                gripper += (close_pressed ? -m_gripper.rate : m_gripper.rate) * m_state_table->PeriodStats.PeriodAvg();
                                                gripper = std::max(m_gripper.min, std::min(m_gripper.max, gripper));
                                                m_gripper_measured_js.Position()[0] = gripper;
                                                m_gripper_measured_js.SetValid(true);
                                            }
                                        }

                                        int bIdx = 0;
                                        for (auto & data : mButtonCallbacks) {
                                            if (m_gripper.emulate && ((bIdx == 1) || (bIdx == 2))) {
                                                data->Pressed = pressed[bIdx];
                                                bIdx++;
                                                continue;
                                            }
                                            if (data->Pressed != pressed[bIdx]) {
                                                data->Pressed = pressed[bIdx];
                                                prmEventButton event;
                                                event.SetValid(true);
                                                event.SetTimestamp(m_state_table->GetTic());
                                                event.Type() = data->Pressed ? prmEventButton::PRESSED
                                                                             : prmEventButton::RELEASED;
                                                data->Function(event);
                                            }
                                            bIdx++;
                                        }
                                    }
                                }
                                // Battery
                                if (state.isMember("battery_level")) {
                                    double battery = state["battery_level"].asDouble();
                                    int current_decile = (int)(battery * 10.0);
                                    if (!m_first_battery_sent) {
                                        std::stringstream ss;
                                        ss << m_name << ": verse grip battery level is " << (int)(battery * 100.0)
                                           << "%";
                                        m_interface->SendStatus(ss.str());
                                        m_first_battery_sent = true;
                                        m_last_battery_decile = current_decile;
                                    }
                                    else {
                                        if (current_decile < m_last_battery_decile) {
                                            std::stringstream ss;
                                            ss << m_name << ": verse grip battery dropped below "
                                               << (current_decile + 1) * 10 << "% (" << (int)(battery * 100.0) << "%).";
                                            m_interface->SendWarning(ss.str());
                                            m_last_battery_decile = current_decile;
                                        }
                                        else if (current_decile > m_last_battery_decile) {
                                            m_last_battery_decile = current_decile;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                if (foundGrip)
                    break;
            }
        }
        else {
            // Identity orientation if no grip
            m_local_measured_cs.Position().Rotation().Assign(vctMatRot3::Identity());
        }
    }
    update_measured_cs();
}

void mtsHaplyDevice::emit_operating_state_event(void)
{
    m_operating_state.SetTimestamp(m_state_table->GetTic());
    m_operating_state_event(m_operating_state);
}

void mtsHaplyDevice::state_command(const std::string & command) {
    std::string humanReadableMessage;
    prmOperatingState::StateType newOperatingState;
    try {
        if (m_operating_state.ValidCommand(prmOperatingState::CommandTypeFromString(command),
                                           newOperatingState,
                                           humanReadableMessage)) {
            if (command == "enable") {
                m_operating_state.State() = prmOperatingState::ENABLED;
            }
            else if (command == "disable") {
                m_operating_state.State() = prmOperatingState::DISABLED;
            }
            else {
                m_interface->SendStatus(this->m_name + ": state command \"" + command + "\" is not supported yet");
            }
            // always emit event with current device state
            m_interface->SendStatus(this->m_name + ": current state is \""
                                    + prmOperatingState::StateTypeToString(m_operating_state.State()) + "\"");
            m_operating_state.SetValid(true);
            emit_operating_state_event();
        }
        else {
            m_interface->SendWarning(this->m_name + ": " + humanReadableMessage);
        }
    }
    catch (std::runtime_error & e) {
        m_interface->SendWarning(this->m_name + ": " + command + " doesn't seem to be a valid state_command ("
                                 + e.what() + ")");
    }
}

void mtsHaplyDevice::SetControlMode(const mtsHaply::ControlModeType & mode) {
    // return if we are already in this mode
    if (mode == mControlMode) {
        return;
    }
    // transition to new mode
    switch (mode) {
    case mtsHaply::SERVO_CP:
        break;
    case mtsHaply::SERVO_CF:
        break;
    default:
        break;
    }
    // assign mode
    mControlMode = mode;
}

void mtsHaplyDevice::body_servo_cf(const prmForceCartesianSet & wrench) {
    if (!m_is_calibrated) {
        return;
    }
    SetControlMode(mtsHaply::SERVO_CF);
    m_body_servo_cf = wrench;
}

void mtsHaplyDevice::servo_cp(const prmPositionCartesianSet & position) {
    if (!m_is_calibrated) {
        return;
    }
    SetControlMode(mtsHaply::SERVO_CP);
    m_servo_cp = position;
    m_new_servo_cp = true;
}

void mtsHaplyDevice::hold(void) {
    if (!m_is_calibrated) {
        return;
    }
    GetRobotData();
    SetControlMode(mtsHaply::SERVO_CP);
    m_servo_cp.Goal().Assign(m_measured_cs.Position());
    m_new_servo_cp = true;
}

void mtsHaplyDevice::use_gravity_compensation(const bool & gravity) { m_use_gravity_compensation = gravity; }

void mtsHaplyDevice::set_base_frame(const prmPositionCartesianSet & baseFrame) {
    if (baseFrame.Valid()) {
        m_base_frame.reference_frame() = baseFrame.ReferenceFrame().empty()
            ? std::string("user")
            : baseFrame.ReferenceFrame();
        m_base_frame.transform().FromNormalized(baseFrame.Goal());
    } else {
        m_base_frame.reference_frame().clear();
        m_base_frame.transform().Assign(vctFrm4x4::Identity());
    }
    update_measured_cs();
    m_setpoint_cp.SetReferenceFrame(m_measured_cs.ReferenceFrame());
}

void mtsHaplyDevice::free(void) {
    SetControlMode(mtsHaply::SERVO_CF);
    m_body_servo_cf.Force().Zeros();
    use_gravity_compensation(true);
}

void mtsHaply::Init(void) {
    mConfigured = false;
    m_websocket = std::make_unique<mtsHaplySocket>();
}

mtsHaply::mtsHaply(const std::string & componentName):
    mtsTaskContinuous(componentName, 256) {
    Init();
}

mtsHaply::mtsHaply(const mtsTaskContinuousConstructorArg & arg):
    mtsTaskContinuous(arg) {
    Init();
}

mtsHaply::~mtsHaply(void) {
    for (auto device : mDevices) {
        delete device;
    }
    mDevices.clear();
}

void mtsHaply::Configure(const std::string & filename) {
    if (mConfigured) {
        CMN_LOG_CLASS_INIT_VERBOSE << "Configure: already configured" << std::endl;
        return;
    }

    struct DeviceRequest {
        std::string Name;
        unsigned int Inverse3Serial;
        unsigned int VerseGripSerial;
        std::string Inverse3ID;
        std::string VerseGripID;
        bool Inverse3Found;
        bool VerseGripFound;
        prmBaseFrame BaseFrame;
        bool GripperEmulate;
        double GripperRate;
        double GripperMin;
        double GripperMax;
    };
    std::vector<DeviceRequest> requestedDevices;

    if (filename != "") {
        // read JSON file passed as param
        std::ifstream jsonStream;
        jsonStream.open(filename.c_str());

        Json::Value jsonConfig;
        Json::Reader jsonReader;
        if (!jsonReader.parse(jsonStream, jsonConfig)) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to parse " << filename << std::endl
                                     << jsonReader.getFormattedErrorMessages();
            exit(-1);
        }

        if (!jsonConfig["uri"].empty()) {
            m_uri = jsonConfig["uri"].asString();
        }

        const Json::Value jsonDevices = jsonConfig["devices"];
        for (unsigned int index = 0; index < jsonDevices.size(); ++index) {
            Json::Value jsonValue = jsonDevices[index];
            DeviceRequest dr;
            dr.Name = jsonValue["name"].asString();
            if (dr.Name == "") {
                std::stringstream defaultName;
                defaultName << "Inverse3-" << std::setfill('0') << std::setw(2) << index;
                dr.Name = defaultName.str();
            }
            dr.Inverse3Serial = 0;
            dr.VerseGripSerial = 0;
            dr.Inverse3Found = false;
            dr.VerseGripFound = false;

            dr.BaseFrame.reference_frame() = "user";
            dr.BaseFrame.transform().Assign(vctFrm4x4::Identity());
            if (jsonValue.isMember("base_frame")) {
                const Json::Value jsonBaseFrame = jsonValue["base_frame"];
                if (jsonBaseFrame.isMember("reference_frame")) {
                    dr.BaseFrame.reference_frame() = jsonBaseFrame["reference_frame"].asString();
                }
                if (jsonBaseFrame.isMember("transform")) {
                    vctFrm4x4 frame;
                    cmnDataJSON<vctFrm4x4>::DeSerializeText(frame, jsonBaseFrame["transform"]);
                    dr.BaseFrame.transform().Assign(frame);
                }
            }

            dr.GripperEmulate = true;
            dr.GripperRate = 1.0;
            dr.GripperMin = -0.5;
            dr.GripperMax = 1.0;
            if (jsonValue.isMember("gripper")) {
                const Json::Value jsonGripper = jsonValue["gripper"];
                if (jsonGripper.isMember("emulate")) {
                    dr.GripperEmulate = jsonGripper["emulate"].asBool();
                }
                if (jsonGripper.isMember("rate")) {
                    dr.GripperRate = jsonGripper["rate"].asDouble();
                }
                if (jsonGripper.isMember("min")) {
                    dr.GripperMin = jsonGripper["min"].asDouble();
                }
                if (jsonGripper.isMember("max")) {
                    dr.GripperMax = jsonGripper["max"].asDouble();
                }
            }

            if (jsonValue.isMember("base_serial")) {
                if (jsonValue["base_serial"].isString()) {
                    try {
                        dr.Inverse3Serial = std::stoull(jsonValue["base_serial"].asString(), nullptr, 16);
                    }
                    catch (...) {
                        CMN_LOG_CLASS_INIT_ERROR
                            << "Configure: invalid base_serial string: " << jsonValue["base_serial"].asString()
                            << std::endl;
                        exit(-1);
                    }
                }
                else {
                    dr.Inverse3Serial = jsonValue["base_serial"].asUInt();
                }
            }
            else if (jsonValue.isMember("serial")) {
                if (jsonValue["serial"].isString()) {
                    try {
                        dr.Inverse3Serial = std::stoull(jsonValue["serial"].asString(), nullptr, 16);
                    }
                    catch (...) {
                        CMN_LOG_CLASS_INIT_ERROR
                            << "Configure: invalid serial string: " << jsonValue["serial"].asString() << std::endl;
                        exit(-1);
                    }
                }
                else {
                    dr.Inverse3Serial = jsonValue["serial"].asUInt();
                }
            }
            else if (jsonValue.isMember("inverse3")) {
                if (jsonValue["inverse3"].isString()) {
                    try {
                        dr.Inverse3Serial = std::stoull(jsonValue["inverse3"].asString(), nullptr, 16);
                    }
                    catch (...) {
                        CMN_LOG_CLASS_INIT_ERROR
                            << "Configure: invalid inverse3 string: " << jsonValue["inverse3"].asString() << std::endl;
                        exit(-1);
                    }
                }
                else {
                    dr.Inverse3Serial = jsonValue["inverse3"].asUInt();
                }
            }

            if (jsonValue.isMember("stylus_serial")) {
                if (jsonValue["stylus_serial"].isString()) {
                    try {
                        dr.VerseGripSerial = std::stoull(jsonValue["stylus_serial"].asString(), nullptr, 16);
                    }
                    catch (...) {
                        CMN_LOG_CLASS_INIT_ERROR
                            << "Configure: invalid stylus_serial string: " << jsonValue["stylus_serial"].asString()
                            << std::endl;
                        exit(-1);
                    }
                }
                else {
                    dr.VerseGripSerial = jsonValue["stylus_serial"].asUInt();
                }
            }
            else if (jsonValue.isMember("verse_grip")) {
                if (jsonValue["verse_grip"].isString()) {
                    try {
                        dr.VerseGripSerial = std::stoull(jsonValue["verse_grip"].asString(), nullptr, 16);
                    }
                    catch (...) {
                        CMN_LOG_CLASS_INIT_ERROR
                            << "Configure: invalid verse_grip string: " << jsonValue["verse_grip"].asString()
                            << std::endl;
                        exit(-1);
                    }
                }
                else {
                    dr.VerseGripSerial = jsonValue["verse_grip"].asUInt();
                }
            }
            requestedDevices.push_back(dr);
        }
    }
    else {
        // Default configuration
        DeviceRequest dr;
        dr.Name = "Test";
        dr.Inverse3Serial = 0;
        dr.VerseGripSerial = 0;
        dr.Inverse3Found = false;
        dr.VerseGripFound = false;
        dr.BaseFrame.reference_frame() = "user";
        dr.BaseFrame.transform().Assign(vctFrm4x4::Identity());
        dr.GripperEmulate = true;
        dr.GripperRate = 1.0;
        dr.GripperMin = -0.5;
        dr.GripperMax = 1.0;
        requestedDevices.push_back(dr);
    }

    // Connect to WebSocket and discover devices
    m_websocket->Configure(m_uri);
    Json::Value update_request;
    update_request["session"]["force_render_full_state"] = Json::Value(Json::objectValue);
    Json::FastWriter writer;
    m_websocket->SendRequest(writer.write(update_request));

    // Poll for discovery
    bool receivedData = false;
    for (int i = 0; i < 200; ++i) { // ~2 seconds timeout
        m_websocket->Poll();
        if (m_websocket->m_ws_connected
            && (m_websocket->m_ws_data.isMember("inverse3") || m_websocket->m_ws_data.isMember("verse_grip"))) {
            receivedData = true;
            break;
        }
        osaSleep(10.0 * cmn_ms);
    }

    if (!receivedData) {
        CMN_LOG_CLASS_INIT_ERROR << "Configure: failed to connect to service at " << m_uri
                                 << " or no devices discovered." << std::endl;
        exit(-1);
    }

    const Json::Value discoveredDevices = m_websocket->m_ws_data["inverse3"];

    // Validate requested devices against discovered
    for (auto & req : requestedDevices) {
        // Find Inverse3
        if (m_websocket->m_ws_data.isMember("inverse3")) {
            const Json::Value discoveredInv3 = m_websocket->m_ws_data["inverse3"];
            if (discoveredInv3.isArray()) {
                for (unsigned int i = 0; i < discoveredInv3.size(); ++i) {
                    try {
                        unsigned int foundSerial = std::stoull(discoveredInv3[i]["device_id"].asString(), nullptr, 16);
                        if (foundSerial == req.Inverse3Serial) {
                            req.Inverse3Found = true;
                            req.Inverse3ID = discoveredInv3[i]["device_id"].asString();
                            break;
                        }
                    }
                    catch (...) {
                        // Ignore invalid device IDs
                    }
                }
            }
        }
        // Find VerseGrip
        if (req.VerseGripSerial != 0) {
            const char * gripCollections[] = {"verse_grip", "custom_verse_grip", "wireless_verse_grip"};
            for (int c = 0; c < 3; ++c) {
                if (m_websocket->m_ws_data.isMember(gripCollections[c])) {
                    const Json::Value grips = m_websocket->m_ws_data[gripCollections[c]];
                    if (grips.isArray()) {
                        for (unsigned int i = 0; i < grips.size(); ++i) {
                            try {
                                unsigned int foundSerial = std::stoull(grips[i]["device_id"].asString(), nullptr, 16);
                                if (foundSerial == req.VerseGripSerial) {
                                    req.VerseGripFound = true;
                                    req.VerseGripID = grips[i]["device_id"].asString();
                                    break;
                                }
                            }
                            catch (...) {
                                // Ignore invalid device IDs
                            }
                        }
                    }
                }
                if (req.VerseGripFound)
                    break;
            }
        }
        else {
            req.VerseGripFound = true; // Not requested, so ok
        }
    }

    // Check if all requested devices were found
    bool allFound = true;
    for (const auto & req : requestedDevices) {
        if (!req.Inverse3Found) {
            allFound = false;
            std::stringstream ss;
            ss << "Configure: device \"" << req.Name << "\" with inverse3 serial 0x"
               << std::hex << req.Inverse3Serial << std::dec << " not found. Discovered inverse3 devices: [";
            if (m_websocket->m_ws_data.isMember("inverse3")) {
                const Json::Value & disc = m_websocket->m_ws_data["inverse3"];
                if (disc.isArray() && disc.size() > 0) {
                    for (unsigned int i = 0; i < disc.size(); ++i) {
                        ss << "\"0x" << disc[i]["device_id"].asString() << "\"";
                        if (i < disc.size() - 1) ss << ", ";
                    }
                } else {
                    ss << "none";
                }
            } else {
                ss << "none";
            }
            ss << "]";
            CMN_LOG_CLASS_INIT_ERROR << ss.str() << std::endl;
        }
        if (!req.VerseGripFound) {
            allFound = false;
            std::stringstream ss;
            ss << "Configure: device \"" << req.Name << "\" with verse_grip serial 0x"
               << std::hex << req.VerseGripSerial << std::dec << " not found. Discovered verse_grip devices: [";
            const char * gripCollections[] = {"verse_grip", "custom_verse_grip", "wireless_verse_grip"};
            bool anyGrip = false;
            bool firstEntry = true;
            for (int c = 0; c < 3; ++c) {
                if (m_websocket->m_ws_data.isMember(gripCollections[c])) {
                    const Json::Value & grips = m_websocket->m_ws_data[gripCollections[c]];
                    if (grips.isArray()) {
                        for (unsigned int i = 0; i < grips.size(); ++i) {
                            if (!firstEntry) ss << ", ";
                            ss << "\"0x" << grips[i]["device_id"].asString() << "\" (" << gripCollections[c] << ")";
                            firstEntry = false;
                            anyGrip = true;
                        }
                    }
                }
            }
            if (!anyGrip) {
                ss << "none";
            }
            ss << "]";
            CMN_LOG_CLASS_INIT_ERROR << ss.str() << std::endl;
        }
    }

    if (!allFound) {
        exit(-1);
    }

    // Actually create the components
    for (const auto & req : requestedDevices) {
        // create list of buttons
        std::list<mtsInterfaceProvided *> buttonInterfaces;
        buttonInterfaces.push_back(this->AddInterfaceProvided(req.Name + "/Home"));
        buttonInterfaces.push_back(this->AddInterfaceProvided(req.Name + "/A"));
        buttonInterfaces.push_back(this->AddInterfaceProvided(req.Name + "/B"));
        buttonInterfaces.push_back(this->AddInterfaceProvided(req.Name + "/C"));

        // create the device data and add to list of devices
        mtsStateTable * stateTable = new mtsStateTable(StateTable.GetHistoryLength(), req.Name);
        mtsInterfaceProvided * interfaceProvided = this->AddInterfaceProvided(req.Name);
        if (!interfaceProvided) {
            CMN_LOG_CLASS_INIT_ERROR << "Configure: can't create interface provided with name \"" << req.Name << "\""
                                     << std::endl;
            exit(-1);
        }
        mtsHaplyDevice * device = new mtsHaplyDevice(req.Inverse3ID,
                                                     req.VerseGripID,
                                                     req.Name,
                                                     req.BaseFrame,
                                                     req.GripperEmulate,
                                                     req.GripperRate,
                                                     req.GripperMin,
                                                     req.GripperMax,
                                                     stateTable,
                                                     interfaceProvided,
                                                     buttonInterfaces,
                                                     m_websocket.get());
        mDevices.push_back(device);
    }
    mConfigured = true;
}


void mtsHaply::Startup(void) {
    // Start WebSocket connection (already configured in Configure)
    const DevicesType::iterator end = mDevices.end();
    DevicesType::iterator device;
    for (device = mDevices.begin(); device != end; ++device) {
        (*device)->Startup();
    }
}


void mtsHaply::GetDeviceNames(std::list<std::string> & result) const {
    result.clear();
    const DevicesType::const_iterator end = mDevices.end();
    DevicesType::const_iterator device;
    for (device = mDevices.begin(); device != end; ++device) {
        result.push_back((*device)->Name());
    }
}

void mtsHaply::GetButtonNames(const std::string & deviceName, std::list<std::string> & result) const {
    result.clear();
    const DevicesType::const_iterator end = mDevices.end();
    DevicesType::const_iterator device;
    for (device = mDevices.begin(); device != end; ++device) {
        if ((*device)->Name() == deviceName) {
            (*device)->GetButtonNames(result);
            return;
        }
    }
}

void mtsHaply::Run(void) {
    // 1. Poll the WebSocket
    // Drain all pending messages to get the latest state for all devices
    size_t n = 0;
    while ((n = m_websocket->m_ws_client.poll()) > 0) {
        // handlers are called by poll()
    }

    // 2. Clear previous commands
    m_websocket->m_ws_command = Json::Value(Json::objectValue);

    // 3. Handle device discovery if no data yet
    if (m_websocket->m_ws_connected) {
        if (!m_websocket->m_ws_data.isMember("inverse3") || m_websocket->m_ws_data["inverse3"].empty()) {
            Json::Value update_request;
            update_request["session"]["force_render_full_state"] = Json::Value(Json::objectValue);
            Json::FastWriter writer;
            m_websocket->SendRequest(writer.write(update_request));
        }
    }

    // 4. Run each device logic
    m_websocket->m_ws_command["inverse3"] = Json::Value(Json::arrayValue);
    const DevicesType::iterator end = mDevices.end();
    DevicesType::iterator device;
    for (device = mDevices.begin(); device != end; ++device) {
        (*device)->Run();
    }

    // 5. Send the command back to the service
    if (m_websocket->m_ws_connected && !m_websocket->m_ws_command["inverse3"].empty()) {
        Json::FastWriter writer;
        m_websocket->SendRequest(writer.write(m_websocket->m_ws_command));
    }
}

void mtsHaply::Cleanup(void) {
    m_websocket->m_ws_command = Json::Value(Json::objectValue);
    m_websocket->m_ws_command["inverse3"] = Json::Value(Json::arrayValue);

    const DevicesType::iterator end = mDevices.end();
    DevicesType::iterator device;
    for (device = mDevices.begin(); device != end; ++device) {
        (*device)->Cleanup();
    }

    if (m_websocket->m_ws_connected && !m_websocket->m_ws_command["inverse3"].empty()) {
        Json::FastWriter writer;
        m_websocket->SendRequest(writer.write(m_websocket->m_ws_command));
    }
}
