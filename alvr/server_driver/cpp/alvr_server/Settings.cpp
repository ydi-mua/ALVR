#include "Settings.h"
#include "Logger.h"
#include "ipctools.h"
#include "resource.h"
#define PICOJSON_USE_INT64
#include <picojson.h>
#include <string>
#include <fstream>
#include <streambuf>

using namespace std;

extern uint64_t g_DriverTestMode;

Settings Settings::m_Instance;

Settings::Settings()
	: m_EnableOffsetPos(false)
	, m_loaded(false)
{
	m_OffsetPos[0] = 0.0f;
	m_OffsetPos[1] = 0.0f;
	m_OffsetPos[2] = 0.0f;
}


Settings::~Settings()
{
}

void Settings::Load()
{
	try {
		auto sessionFile = std::ifstream(g_alvrDir + "/session.json"s);

		auto json = std::string(
			std::istreambuf_iterator<char>(sessionFile),
			std::istreambuf_iterator<char>());

		picojson::value v;
		std::string err = picojson::parse(v, json);
		if (!err.empty()) {
			Error("Error on parsing json: %hs", err.c_str());
			return;
		}

		picojson::value *activeConnection = nullptr;
		picojson::value *lastConnection = nullptr;
		auto clientConnections = v.get("lastClients").get<picojson::array>();
		for (auto &connection : clientConnections) {
			if (connection.get("connect_automatically").get<bool>()) {
				if (connection.get("available").get<bool>()) {
					activeConnection = &connection;
				}
				if (lastConnection == nullptr ||
					(connection.get("last_update_ms_since_epoch").get<int64_t>()
					> lastConnection->get("last_update_ms_since_epoch").get<int64_t>()))
				{
					lastConnection = &connection;
				}
			}
		}

		picojson::value connectedClient;
		if (activeConnection != nullptr) {
			connectedClient = *activeConnection;
		} else if (lastConnection != nullptr) {
			connectedClient = *activeConnection;
		} else {
			Error("No client set to connect automatically");
			return;
		}

		auto settings_cache = v.get("settings_cache");

		auto video = settings_cache.get("video");
		auto audio = settings_cache.get("audio");
		auto headset = settings_cache.get("headset");
		auto controllers = headset.get("controllers").get("content");
		auto connection = settings_cache.get("connection");

		mSerialNumber = headset.get("serialNumber").get<std::string>();
		mTrackingSystemName = headset.get("trackingSystemName").get<std::string>();
		mModelNumber = headset.get("modelNumber").get<std::string>();
		mDriverVersion = headset.get("driverVersion").get<std::string>();
		mManufacturerName = headset.get("manufacturerName").get<std::string>();
		mRenderModelName = headset.get("renderModelName").get<std::string>();
		mRegisteredDeviceType = headset.get("registeredDeviceType").get<std::string>();

		m_renderWidth = (int32_t)video.get("renderResolution").get("width").get<int64_t>();
		m_renderHeight = (int32_t)video.get("renderResolution").get("height").get<int64_t>();
		m_recommendedTargetWidth = (int32_t)video.get("recommendedTargetResolution").get("width").get<int64_t>();
		m_recommendedTargetHeight = (int32_t)video.get("recommendedTargetResolution").get("height").get<int64_t>();

		picojson::array &eyeFov = video.get("eyeFov").get<picojson::array>();
		for (int eye = 0; eye < 2; eye++) {
			m_eyeFov[eye].left = static_cast<float>(eyeFov[eye].get("left").get<double>());
			m_eyeFov[eye].right = static_cast<float>(eyeFov[eye].get("right").get<double>());
			m_eyeFov[eye].top = static_cast<float>(eyeFov[eye].get("top").get<double>());
			m_eyeFov[eye].bottom = static_cast<float>(eyeFov[eye].get("bottom").get<double>());
		}

		m_enableSound = audio.get("gameAudio").get("enabled").get<bool>();
		m_soundDevice = audio.get("gameAudio").get("content").get("device").get<std::string>();
		m_streamMic = audio.get("microphone").get<bool>();

		m_flSecondsFromVsyncToPhotons = (float)video.get("secondsFromVsyncToPhotons").get<double>();

		m_flIPD = (float)video.get("ipd").get<double>();

		m_clientRecvBufferSize = (uint32_t)connection.get("clientRecvBufferSize").get<int64_t>();
		m_frameQueueSize = (uint32_t)connection.get("frameQueueSize").get<int64_t>();

		m_force60HZ = video.get("force60hz").get<bool>();

		m_force3DOF = headset.get("force3dof").get<bool>();

		m_aggressiveKeyframeResend = connection.get("aggressiveKeyframeResend").get<bool>();

		m_nAdapterIndex = (int32_t)video.get("adapterIndex").get<int64_t>();

		m_codec = (int32_t)(video.get("codec").get("variant").get<std::string>() == "HEVC");
		m_refreshRate = (int)video.get("refreshRate").get<int64_t>();
		mEncodeBitrate = Bitrate::fromMiBits((int)video.get("encodeBitrateMbs").get<int64_t>());

		mThrottlingBitrate = Bitrate::fromBits((int)video.get("throttlingBitrateBits").get<int64_t>());

		// Listener Parameters
		m_Host = connection.get("listenHost").get<std::string>();
		m_Port = (int)connection.get("listenPort").get<int64_t>();

		m_SendingTimeslotUs = (uint64_t)connection.get("sendingTimeslotUs").get<int64_t>();
		m_LimitTimeslotPackets = (uint64_t)connection.get("limitTimeslotPackets").get<int64_t>();

		m_ControlHost = connection.get("controlHost").get<std::string>();
		m_ControlPort = (int)connection.get("controlPort").get<int64_t>();

		m_AutoConnectHost = connectedClient.get("address").get<std::string>();
		m_AutoConnectPort = 9943;

		m_controllerTrackingSystemName = controllers.get("trackingSystemName").get<std::string>();
		m_controllerManufacturerName = controllers.get("trackingSystemName").get<std::string>();
		m_controllerModelNumber = controllers.get("modelNumber").get<std::string>();
		m_controllerRenderModelNameLeft = controllers.get("renderModelNameLeft").get<std::string>();
		m_controllerRenderModelNameRight = controllers.get("renderModelNameRight").get<std::string>();
		m_controllerSerialNumber = controllers.get("serialNumber").get<std::string>();
		m_controllerType = controllers.get("ctrlType").get<std::string>();
		mControllerRegisteredDeviceType = controllers.get("registeredDeviceType").get<std::string>();
		m_controllerInputProfilePath = controllers.get("inputProfilePath").get<std::string>();

		m_disableController = headset.get("controllers").get("enabled").get<bool>();
		m_controllerTriggerMode = (int32_t)controllers.get("triggerMode").get<int64_t>();
		m_controllerTrackpadClickMode = (int32_t)controllers.get("trackpadClickMode").get<int64_t>();
		m_controllerTrackpadTouchMode = (int32_t)controllers.get("trackpadTouchMode").get<int64_t>();
		m_controllerBackMode = (int32_t)controllers.get("backMode").get<int64_t>();
		m_controllerRecenterButton = (int32_t)controllers.get("recenterButton").get<int64_t>();

		m_useTrackingReference = headset.get("useTrackingReference").get<bool>();

		m_EnableOffsetPos = true;
		auto headsetPositionOffset = headset.get("positionOffset").get<picojson::array>();
		m_OffsetPos[0] = (float)headsetPositionOffset[0].get<double>();
		m_OffsetPos[1] = (float)headsetPositionOffset[1].get<double>();
		m_OffsetPos[2] = (float)headsetPositionOffset[2].get<double>();

		m_trackingFrameOffset = (int32_t)headset.get("trackingFrameOffset").get<int64_t>();
		m_controllerPoseOffset = (double)controllers.get("poseTimeOffset").get<double>();

		auto leftControllerPositionOffset = controllers.get("positionOffsetLeft").get<picojson::array>();
		m_leftControllerPositionOffset[0] = leftControllerPositionOffset[0].get<double>();
		m_leftControllerPositionOffset[1] = leftControllerPositionOffset[1].get<double>();
		m_leftControllerPositionOffset[2] = leftControllerPositionOffset[2].get<double>();

		auto leftControllerRotationOffset = controllers.get("rotationOffsetLeft").get<picojson::array>();
		m_leftControllerRotationOffset[0] = leftControllerRotationOffset[0].get<double>();
		m_leftControllerRotationOffset[1] = leftControllerRotationOffset[1].get<double>();
		m_leftControllerRotationOffset[2] = leftControllerRotationOffset[2].get<double>();

		m_hapticsIntensity = controllers.get("hapticsIntensity").get<double>();

		m_enableFoveatedRendering = video.get("foveatedRendering").get("enabled").get<bool>();
		m_foveationStrength = (float)video.get("foveatedRendering").get("content").get("strength").get<double>();
		m_foveationShape = (float)video.get("foveatedRendering").get("content").get("shape").get<double>();
		m_foveationVerticalOffset = (float)video.get("foveatedRendering").get("content").get("verticalOffset").get<double>();

		m_enableColorCorrection = video.get("colorCorrection").get("enabled").get<bool>();
		m_brightness = (float)video.get("colorCorrection").get("content").get("brightness").get<double>();
		m_contrast = (float)video.get("colorCorrection").get("content").get("contrast").get<double>();
		m_saturation = (float)video.get("colorCorrection").get("content").get("saturation").get<double>();
		m_gamma = (float)video.get("colorCorrection").get("content").get("gamma").get<double>();
		m_sharpening = (float)video.get("colorCorrection").get("content").get("sharpening").get<double>();

		m_controllerMode = (int32_t)controllers.get("modeIdx").get<int64_t>();

		LogDriver("Config JSON: %hs", json.c_str());
		LogDriver("Serial Number: %hs", mSerialNumber.c_str());
		LogDriver("Model Number: %hs", mModelNumber.c_str());
		LogDriver("Render Target: %d %d", m_renderWidth, m_renderHeight);
		LogDriver("Seconds from Vsync to Photons: %f", m_flSecondsFromVsyncToPhotons);
		LogDriver("Refresh Rate: %d", m_refreshRate);
		LogDriver("IPD: %f", m_flIPD);

		LogDriver("EncoderOptions: %hs", m_EncoderOptions.c_str());

		m_loaded = true;
	}
	catch (std::exception &e) {
		FatalLog("Exception on parsing json: %hs", e.what());
	}
}
