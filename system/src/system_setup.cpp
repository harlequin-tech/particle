/**
 ******************************************************************************
 * @file    wifi_credentials_reader.cpp
 * @author  Zachary Crockett and Satish Nair
 * @version V1.0.0
 * @date    24-April-2013
 * @brief
 ******************************************************************************
  Copyright (c) 2013-2015 Particle Industries, Inc.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

#include "system_setup.h"
#include "system_control_internal.h"	// XXX debug
#include "control_request_handler.h"	// XXX debug

#include "delay_hal.h"
#include "ota_flash_hal.h"
#include "wlan_hal.h"
#include "cellular_hal.h"
#include "system_cloud_internal.h"
#include "system_update.h"
#include "spark_wiring.h"   // for serialReadLine
#include "system_network_internal.h"
#include "system_network.h"
#include "system_task.h"
#include "spark_wiring_thread.h"
#include "spark_wiring_wifi_credentials.h"
#include "system_ymodem.h"
#include "mbedtls_util.h"
#include "ota_flash_hal.h"

#include "control/mesh.h"		// XXX debug
#include "control/config.h"		// XXX debug
//#include "control/proto/mesh.pb.h"	// XXX debug

extern ProtocolFacade* sp;
#include "device_code.h"	       // XXX debug
#include "spark_protocol_functions.h"  // XXX debug
#include "product_store_hal.h"	       // XXX debug
#include "deviceid_hal.h"	       // XXX debug

extern const char *ota_module_fail;
extern const char *ota_module_pass;
extern const char *dnh_validate_fail_reason;
extern const char *dnh_networkName;
extern const char *dnh_networkNameOrigin;
const char *dnh_networkNameParent = "<gok>";
//char dnh_networkNameOriginParent[128] = "<gok>";
const char *dnh_networkNameOriginParent = "<gok>";
char dnh_networkNameOriginValue[16];
extern char dnh_networkNameValue[10][16];
#define DEBUG_DATA_SAVERS
#ifdef DEBUG_DATA_SAVERS
const char *dnh_dataSaver[10] = { NULL, };
volatile char dnh_dataSaverCount = 0;
#endif
extern uint32_t dnh_networkNameSet;
//const char *dnh_networkName = "not set";     // XXX debug

void Spark_HandshakeDump(void);
void Spark_Dump_Config(void);

#if SETUP_OVER_SERIAL1
#define SETUP_LISTEN_MAGIC 1
void loop_wifitester(int c);
#include "spark_wiring_usartserial.h"


#include "system_control_internal.h"
namespace particle {

namespace system {
    extern SystemControl g_systemControl;

}
}

static system_tester_handlers_t s_tester_handlers = {0};

int system_set_tester_handlers(system_tester_handlers_t* handlers, void* reserved) {
    memset(&s_tester_handlers, 0, sizeof(s_tester_handlers));
    if (handlers != nullptr) {
        memcpy(&s_tester_handlers, handlers, std::min(static_cast<uint16_t>(sizeof(s_tester_handlers)), handlers->size));
    }
    return 0;
}

#else /* SETUP_OVER_SERIAL1 */

int system_set_tester_handlers(system_tester_handlers_t* handlers, void* reserved) {
    return 1;
}

#endif /* SETUP_OVER_SERIAL1 */

#ifndef SETUP_LISTEN_MAGIC
#define SETUP_LISTEN_MAGIC 0
#endif

#ifndef PRIVATE_KEY_SIZE
#define PRIVATE_KEY_SIZE        (2*1024)
#endif

#ifndef CERTIFICATE_SIZE
#define CERTIFICATE_SIZE        (4*1024)
#endif

// This can be changed to Serial for testing purposes
#define SETUP_SERIAL Serial1

int is_empty(const char *s) {
  while (*s != '\0') {
    if (!isspace(*s))
      return 0;
    s++;
  }
  return 1;
}

class StreamAppender : public Appender
{
    Stream& stream_;

public:
    StreamAppender(Stream& stream) : stream_(stream) {}

    virtual bool append(const uint8_t* data, size_t length) override {
        return append(&stream_, data, length);
    }

    static bool append(void* appender, const uint8_t* data, size_t length) { // appender_fn
        const auto stream = static_cast<Stream*>(appender);
        return (stream->write(data, length) == length);
    }
};

class WrappedStreamAppender : public StreamAppender
{
  bool wrotePrefix_;
  const uint8_t* prefix_;
  size_t prefixLength_;
  const uint8_t* suffix_;
  size_t suffixLenght_;
public:
  WrappedStreamAppender(
      Stream& stream,
      const uint8_t* prefix,
      size_t prefixLength,
      const uint8_t* suffix,
      size_t suffixLenght) :
    StreamAppender(stream),
    wrotePrefix_(false),
    prefix_(prefix),
    prefixLength_(prefixLength),
    suffix_(suffix),
    suffixLenght_(suffixLenght)
  {}

  ~WrappedStreamAppender() {
    append(suffix_, suffixLenght_);
  }

  virtual bool append(const uint8_t* data, size_t length) override {
    if (!wrotePrefix_) {
      StreamAppender::append(prefix_, prefixLength_);
      wrotePrefix_ = true;
    }
    return StreamAppender::append(data, length);
  }
};


#if SETUP_OVER_SERIAL1
inline bool setup_serial1() {
    uint8_t value = 0;
    system_get_flag(SYSTEM_FLAG_WIFITESTER_OVER_SERIAL1, &value, nullptr);
    return value;
}
#endif

template <typename Config> SystemSetupConsole<Config>::SystemSetupConsole(const Config& config_)
    : config(config_)
{
    WITH_LOCK(serial);
    if (serial.baud() == 0)
    {
        serial.begin(9600);
    }
#if SETUP_OVER_SERIAL1
    serial1Enabled = false;
    magicPos = 0;
    if (setup_serial1()) {
            //WITH_LOCK(SETUP_SERIAL);
            SETUP_SERIAL.begin(9600);
    }
    this->tester = nullptr;
#endif
}

template <typename Config> SystemSetupConsole<Config>::~SystemSetupConsole()
{
#if SETUP_OVER_SERIAL1
    if (tester != nullptr && s_tester_handlers.destroy) {
        s_tester_handlers.destroy(this->tester, nullptr);
    }
#endif
}

template<typename Config> void SystemSetupConsole<Config>::loop(void)
{
#if SETUP_OVER_SERIAL1
    //TRY_LOCK(SETUP_SERIAL)
    {
        if (setup_serial1() && s_tester_handlers.size != 0) {
            int c = -1;
            if (SETUP_SERIAL.available()) {
                c = SETUP_SERIAL.read();
            }
            if (SETUP_LISTEN_MAGIC) {
                static uint8_t magic_code[] = { 0xe1, 0x63, 0x57, 0x3f, 0xe7, 0x87, 0xc2, 0xa6, 0x85, 0x20, 0xa5, 0x6c, 0xe3, 0x04, 0x9e, 0xa0 };
                if (!serial1Enabled) {
                    if (c>=0) {
                        if (c==magic_code[magicPos++]) {
                            serial1Enabled = magicPos==sizeof(magic_code);
                            if (serial1Enabled) {
                                if (tester == nullptr && s_tester_handlers.create != nullptr) {
                                    tester = s_tester_handlers.create(nullptr);
                                }
                                if (tester != nullptr && s_tester_handlers.setup) {
                                    s_tester_handlers.setup(tester, SETUP_OVER_SERIAL1, nullptr);
                                }
                            }
                        }
                        else {
                            magicPos = 0;
                        }
                        c = -1;
                    }
                }
                else {
                    if (tester != nullptr && s_tester_handlers.loop) {
                        s_tester_handlers.loop(tester, c, nullptr);
                    }
                }
            }
        }
    }
#endif

    TRY_LOCK(serial)
    {
        if (serial.available())
        {
            int c = serial.peek();
            if (c >= 0)
            {
                if (!handle_peek((char)c))
                {
                    if (serial.available())
                    {
                        c = serial.read();
                        handle((char)c);
                    }
                }
            }
        }
    }
}

template <typename Config>
void SystemSetupConsole<Config>::cleanup()
{
}

template <typename Config>
void SystemSetupConsole<Config>::exit()
{
    network_listen(0, NETWORK_LISTEN_EXIT, nullptr);
}

template <typename Config>
bool SystemSetupConsole<Config>::handle_peek(char c)
{
    if (YModem::SOH == c || YModem::STX == c)
    {
        system_firmwareUpdate(&serial);
        return true;
    }
    return false;
}

bool filter_key(const char* src, char* dest, size_t size) {
	memcpy(dest, src, size);
	if (!strcmp(src, "imei")) {
		strcpy(dest, "IMEI");
	}
	else if (!strcmp(src, "iccid")) {
		strcpy(dest, "ICCID");
	}
	else if (!strcmp(src, "sn")) {
		strcpy(dest, "Serial Number");
	}
	else if (!strcmp(src, "ms")) {
		strcpy(dest, "Device Secret");
	}
	return false;
}

template<typename Config> void SystemSetupConsole<Config>::handle(char c)
{
    if ('i' == c)
    {
    	// see if we have any additional properties. This is true
    	// for Cellular and Mesh devices.
    	hal_system_info_t info = {};
    	info.size = sizeof(info);
    	HAL_OTA_Add_System_Info(&info, true, nullptr);
    	LOG(TRACE, "device info key/value count: %d", info.key_value_count);
    	if (info.key_value_count) {
    		print("Device ID: ");
    		String id = spark_deviceID();
			print(id.c_str());
			print("\r\n");

			for (int i=0; i<info.key_value_count; i++) {
				char key[20];
				if (!filter_key(info.key_values[i].key, key, sizeof(key))) {
					print(key);
					print(": ");
					print(info.key_values[i].value);
					print("\r\n");
				}
			}
		}
    	else {
			print("Your device id is ");
			String id = spark_deviceID();
			print(id.c_str());
			print("\r\n");
    	}
    	HAL_OTA_Add_System_Info(&info, false, nullptr);
    }
    else if ('m' == c)
    {
        print("Your device MAC address is\r\n");
        IPConfig config = {};
    #if !HAL_PLATFORM_WIFI    
        auto conf = static_cast<const IPConfig*>(network_config(0, 0, 0));
    #else
        auto conf = static_cast<const IPConfig*>(network_config(NETWORK_INTERFACE_WIFI_STA, 0, 0));
    #endif
        if (conf && conf->size) {
            memcpy(&config, conf, std::min(sizeof(config), (size_t)conf->size));
        }
        const uint8_t* addr = config.nw.uaMacAddr;
        print(bytes2hex(addr++, 1).c_str());
        for (int i = 1; i < 6; i++)
        {
            print(":");
            print(bytes2hex(addr++, 1).c_str());
        }
        print("\r\n");
    }
    else if ('f' == c)
    {
        serial.println("Waiting for the binary file to be sent ... (press 'a' to abort)");
        system_firmwareUpdate(&serial);
    }
    else if ('x' == c)
    {
        exit();
    }
    else if ('s' == c)
    {
        auto prefix = "{";
        auto suffix = "}\r\n";
        WrappedStreamAppender appender(serial, (const uint8_t*)prefix, strlen(prefix), (const uint8_t*)suffix, strlen(suffix));
        system_module_info(append_instance, &appender);
    }
    else if ('v' == c)
    {
        StreamAppender appender(serial);
        append_system_version_info(&appender);
        print("\r\n");
    }
    else if ('L' == c)
    {
        system_set_flag(SYSTEM_FLAG_STARTUP_LISTEN_MODE, 1, nullptr);
        System.enterSafeMode();
    }
    else if ('c' == c)
    {
            bool claimed = HAL_IsDeviceClaimed(nullptr);
            print("Device claimed: ");
            print(claimed ? "yes" : "no");
            print("\r\n");
	    char claimCode[64];
	    if (HAL_Get_Claim_Code(claimCode, sizeof(claimCode)) == 0) {
		print("Claim code: ");
		print(bytes2hex((uint8_t *)claimCode, 64).c_str());
		//print(claimCode);
		print("\r\n");
	    }
    }
    else if ('C' == c)
    {
            char code[64];
            print("Enter 63-digit claim code: ");
            read_line(code, 63);
            if (strlen(code)==63) {
                HAL_Set_Claim_Code(code);
                print("Claim code set to: ");
                print(code);
            }
            else {
                print("Sorry, claim code is not 63 characters long. Claim code unchanged.");
}
        print("\r\n");
    }
    else if ('d' == c)
    {
        system_format_diag_data(nullptr, 0, 0, StreamAppender::append, &serial, nullptr);
        print("\r\n");
    } else switch (c) {
	case 'u':
                if (HAL_Set_Claim_Code("002b63786c70714e7a6e664b334e4a2f326256587438316a6e6444485055626473672b697365754a69773779523672796149416a3277324673656a6343547a00") == 0) {
		    print("Claim code set\r\n");
		} else {
		    print("Failed to set claim code\r\n");
		}
		break;
	case 'U':
                if (HAL_Set_Claim_Code(NULL) == 0) {
		    print("Claim code cleared, device claimed\r\n");
		} else {
		    print("Failed to clear claim code and claim device\r\n");
		}
		break;
	case 'S': {	// toggle device setup done
	    char buf[128];
	    int res = particle::control::config::isDeviceSetupDoneX();
	    if (res < 0) {
		print("failed to get device setup done: res=");
		snprintf(buf, sizeof(buf), "%i", res);
		print(buf); print("\r\n");
		break;
	    }
	    snprintf(buf, sizeof(buf), "deviceSetupDone %i -> %i\r\n", res, !res);
	    print(buf);
	    res = particle::control::config::setDeviceSetupDoneX(!res);
	    snprintf(buf, sizeof(buf), "res = %i\r\n", res);
	    print(buf);
	}
	break;
	case 'a': {	// add joiner 
	    char eui64[32] = "";
	    char password[64] = "";
            print("Joiner eui64: ");
            read_line(eui64, sizeof(eui64)-1);
	    if (strlen(eui64) == 0) {
		print("cancelled\n\r");
		break;
	    }
            print("Joiner password: ");
            read_line(password, sizeof(password)-1);
	    if (strlen(password) == 0) {
		print("cancelled\n\r");
		break;
	    }
	    int res = particle::ctrl::mesh::addJoinerX(eui64, password, 0);
	    print("addJoiner res = ");
	    snprintf(eui64, sizeof(eui64), "%i", res);
	    print(eui64);
	    print("\r\n");
	}
	break;
	case 'r': {	// remove joiner 
	    char eui64[32] = "";
            print("Joiner eui64: ");
            read_line(eui64, sizeof(eui64)-1);
	    if (strlen(eui64) == 0) {
		print("cancelled\n\r");
		break;
	    }
	    int res = particle::ctrl::mesh::removeJoinerX(eui64);
	    print("removeJoiner res = ");
	    snprintf(eui64, sizeof(eui64), "%i", res);
	    print(eui64);
	    print("\r\n");
	}
	break;
	case 'j': {	// join network
	    char buf[32];
	    print("Joining network\r\n");
	    int res = particle::ctrl::mesh::joinNetworkX(60);
	    print("joinNetwork res = ");
	    snprintf(buf, sizeof(buf), "%i", res);
	    print(buf);
	    print("\r\n");
	}
	break;
	case 'o': {
	    char buf[32];
	    print("Starting commissioner\r\n");
	    int res = particle::ctrl::mesh::startCommissionerX(300);
	    print("startCommissioner res = ");
	    snprintf(buf, sizeof(buf), "%i", res);
	    print(buf); print("\r\n");
        }
	break;
	case 'O': {
	    char buf[32];
	    int res = particle::ctrl::mesh::stopCommissioner(nullptr);
	    print("stopCommissioner res = ");
	    snprintf(buf, sizeof(buf), "%i", res);
	    print(buf); print("\r\n");
        }
	break;
	case 'p': {	// prep to join	network
	    char networkId[24 + 1] = "5dec24ab58fa4e00012e7b7e";
	    print("Network Ids: earthship 5dec24ab58fa4e00012e7b7e\n\r");
	    uint16_t panId = 0x7256; // 0248d
	    char panIdStr[4+1] = "248d";
            print("Network Id: [");
	    print(networkId);
	    print("] ");
            read_line(networkId, sizeof(networkId)-1);
	    if (networkId[0] == 0) {
		networkId[0] = '5';
	    }
	    if (strlen(networkId) != 24) {
		print("cancelled\n\r");
		break;
	    }
	    print("Pan Ids: earthship 248d, config 7256\n\r");
            print("Pan Id: [0x248d] 0x");
            read_line(panIdStr, sizeof(panIdStr)-1);
	    if (panIdStr[0] == 0) {
		panIdStr[0] = '2';
	    }
	    if (strlen(panIdStr) != 4) {
		print("cancelled\n\r");
		break;
	    }
	    char *endp;
	    panId = strtol(panIdStr, &endp, 16);
	    if (*endp != '\0') {
		print("cancelled\n\r");
		break;
	    }
	    //particle::ctrl::mesh::getNetworkId(networkId, sizeof(networkId));
	    int res = particle::ctrl::mesh::prepareJoinerX(panId, networkId, sizeof(networkId)-1);
	    char buf[32];
	    print("prepareJoinerX(0x");
	    print(panIdStr);
	    print(", \"");
	    print(networkId);
	    print(") res = ");
	    snprintf(buf, sizeof(buf), "%i", res);
	    print(buf); print("\r\n");
	    if (res == 0) {
		print("      eui64: \"");
		print(particle::ctrl::mesh::getEui64Str());
		print("\"\r\n");
		print("    joinPwd: \"");
		print(particle::ctrl::mesh::getJoinPwd());
		print("\"\r\n");
	    }
#if 0
	    //int res = particle::ctrl::mesh::startCommissionerX(30);
	    particle::system::SystemControl *systemControl = particle::system::system_ctrl_instance(); 
	    particle_ctrl_mesh_StartCommissionerRequest req = { 30 };
	    //particle::ControlRequestChannel *crq = nullptr;
	    systemControl->processRequest((ctrl_request *)&req, NULL);
	    //ctrl::mesh::startCommissioner(NULL);
	    //const auto thread = threadInstance();
            //CHECK_THREAD(otCommissionerStart(thread, nullptr, nullptr, nullptr));
#endif
	    }
	break;
	case 'D':
	    print("Entering DFU mode\r\n");
	    System.dfu(true);
	    break;
	case 'M': {
	    uint8_t mode = (uint8_t)system_mode();
	    uint32_t reasonCount = 0;
	    const char *reason = get_system_mode_reason(&reasonCount);
	    const char *modes[] = { "DEFAULT", "AUTOMATIC", "SEMI_AUTOMATIC", "MANUAL", "SAFE_MODE" };
	    print("System Mode: ");
	    if (mode < (sizeof modes / sizeof modes[0])) {
		print(modes[mode]);
		print("\r\n");
	    } else {
		print(bytes2hex(&mode, 1).c_str());
		print("\r\n");
	    }
	    if (reason != NULL) {
		print("Mode Reason: ");
		print(reason);
		print(", count: ");
		mode = (uint8_t)reasonCount;
		print(bytes2hex(&mode, 1).c_str());
		print("\r\n");
	    }
	    print("OTA module pass: ");
	    print(ota_module_pass);
	    print("\r\n");
	    print("OTA module fail: ");
	    print(ota_module_fail);
	    print("\r\n");
	    print("Validation fail: ");
	    print(dnh_validate_fail_reason);
	    print("\r\n");
	    print("\r\n");
	    Spark_Dump_Config();
	    Spark_HandshakeDump();
	    break;
        }
	case 'k': {
	    product_details_t info;
	    info.size = sizeof(info);
	    spark_protocol_get_product_details(sp, &info);

	    // User code was run, so persist the current values stored in the comms lib.
	    // These will either have been left as default or overridden via PRODUCT_ID/PRODUCT_VERSION macros
	    if (system_mode() != SAFE_MODE) {
		print("SPARK: not SAFE_MODE");
	    } else {      // user code was not executed, use previously persisted values
		print("SPARK: SAFE_MODE, loading product details from flash");
		info.product_id = HAL_GetProductStore(PRODUCT_STORE_ID);
		info.product_version = HAL_GetProductStore(PRODUCT_STORE_VERSION);
	    }
	    print("SPARK: product_id 0x");
	    print(bytes2hex((uint8_t *)&info.product_id, 2).c_str());
	    print(", firmware_version 0x");
	    print(bytes2hex((uint8_t *)&info.product_version, 2).c_str());
	    print("\r\n");

	    do {
		uint8_t pubkey[EXTERNAL_FLASH_SERVER_PUBLIC_KEY_LENGTH];

		memset(&pubkey, 0xff, sizeof(pubkey));
		HAL_FLASH_Read_ServerPublicKey(pubkey);
		print("SPARK: pubkey  ");
		print(bytes2hex(pubkey, 4).c_str());
		print(" ... ");
		print(bytes2hex(&pubkey[sizeof(pubkey) - 4], 4).c_str());
		print("\r\n");
	    } while (0);

	    do {
		uint8_t private_key[EXTERNAL_FLASH_CORE_PRIVATE_KEY_LENGTH];
		private_key_generation_t genspec;
		genspec.size = sizeof(genspec);
		genspec.gen = PRIVATE_KEY_GENERATE_NEVER;

		memset(&private_key, 0xff, sizeof(private_key));
		HAL_FLASH_Read_CorePrivateKey(private_key, &genspec);
		print("SPARK: privkey ");
		print(bytes2hex(private_key, 4).c_str());
		print(" ... ");
		print(bytes2hex(&private_key[sizeof(private_key) - 4], 4).c_str());
		print("\r\n");
	    } while (0);

	    uint8_t id_length = HAL_device_ID(NULL, 0);
	    uint8_t id[id_length];
	    HAL_device_ID(id, id_length);

	    do {
		char buf[128];
		if (hal_get_device_serial_number(buf, sizeof buf, nullptr) >= 0) {
		    print("SPARK: Device Serial ");
		    print(buf);
		} else {
		    print("SPARK: Device Serial <gok>");
		}
		print("\r\n");
	    } while (0);

	    print("NCP NetworkName: ");
	    print(dnh_networkName);
	    print(", count=0x");
	    uint8_t count = (uint8_t)dnh_networkNameSet;
	    print(bytes2hex(&count, 1).c_str());
	    print("\r\n");
	    for (uint32_t ind=0; ind<dnh_networkNameSet; ind++) {
		print("    - ");
		print(dnh_networkNameValue[ind]);
		print("\r\n");
	    }
	    print("Origin: ");
	    print(dnh_networkNameOrigin);
	    print(" -> ");
	    print(dnh_networkNameOriginValue);
	    print("\r\n");
	    print("Origin parent: ");
	    print(dnh_networkNameOriginParent);
	    print("\r\n");
#ifdef DEBUG_DATA_SAVERS
	    print("Data savers:\r\n");
	    for (uint32_t ind=0; ind<dnh_dataSaverCount; ind++) {
		print("    - ");
		if (dnh_dataSaver[ind]) {
		    print(dnh_dataSaver[ind]);
		} else {
		    print("(null)");
		}
		print("\r\n");
	    }
#endif
	    break;
	}
	case 'n': {
	    // print networkname
	    char buf[128];
	    if (get_device_name(buf, sizeof buf) > 0) {
		print("Device name: ");
		print(buf);
	    } else {
		print("Device name: (null)");
	    }
	    print("\r\n");

	    char networkId[24 + 1] = {};
	    int res = particle::ctrl::mesh::getNetworkId(networkId, sizeof(networkId));
	    if (res != 0) {
		snprintf(buf, sizeof(buf), "getNetworkId() failed: %d\n", res);
		print(buf);
		print("\r\n");
	    } else {
		print("Network Id: \"");
		print(networkId);
		print("\"\r\n");
	    }

	    uint16_t panId = particle::ctrl::mesh::getPanId();
	    snprintf(buf, sizeof(buf), "Pan Id: %04x\r\n", panId);
	    print(buf);

	    res = particle::control::config::isDeviceSetupDoneX();
	    print("isDeviceSetupDoneX() =  ");
	    snprintf(buf, sizeof(buf), "%i", res);
	    print(buf); print("\r\n");
	    break;
	}
	case 'N': {
	    // set networkname
	    char name[64];
	    print("Enter network name: ");
	    read_line(name, 63);
	    if (strlen(name)>0) {
		//HAL_Set_Claim_Code(code);
		print("Network name set to: ");
		print(name);
		print("\r\n");
	    }
	    else {
		print("Network name unchanged.\r\n");
	    }
	    break;
	}
	default:
	    break;
    }
}

/* private methods */

template<typename Config> void SystemSetupConsole<Config>::print(const char *s)
{
    for (size_t i = 0; i < strlen(s); ++i)
    {
        serial.write(s[i]);
    }
}

template<typename Config> void SystemSetupConsole<Config>::read_line(char *dst, int max_len)
{
    serialReadLine(&serial, dst, max_len, 0); //no timeout
    print("\r\n");
    while (0 < serial.available())
        serial.read();
}

template<typename Config> void SystemSetupConsole<Config>::read_multiline(char *dst, int max_len)
{
    char *ptr = dst;
    int len = max_len;
    while(len > 3) {
        serialReadLine(&serial, ptr, len, 0); //no timeout
        print("\r\n");
        int l = strlen(ptr);
        len -= l;
        ptr += l;
        if (len > 3) {
            if (l != 0) {
                *ptr++ = '\r';
                *ptr++ = '\n';
            }
            *ptr = '\0';
        }
        if (l == 0)
            return;
    }
    while (0 < serial.available())
        serial.read();
}

#if Wiring_WiFi

WiFiSetupConsole::WiFiSetupConsole(const WiFiSetupConsoleConfig& config)
 : SystemSetupConsole(config)
{
}

WiFiSetupConsole::~WiFiSetupConsole()
{
}

void WiFiSetupConsole::handle(char c)
{
    if ('w' == c)
    {
        memset(ssid, 0, sizeof(ssid));
        memset(password, 0, sizeof(password));
        memset(security_type_string, 0, sizeof(security_type_string));
        security_ = WLAN_SEC_NOT_SET;
        cipher_ = WLAN_CIPHER_NOT_SET;

#if Wiring_WpaEnterprise == 1
        spark::WiFiAllocatedCredentials credentials;
        memset(eap_type_string, 0, sizeof(eap_type_string));
#else
        spark::WiFiCredentials credentials;
#endif
        WLanCredentials creds;

        do {
        print("SSID: ");
        read_line(ssid, 32);
        } while (strlen(ssid) == 0);

        wlan_scan([](WiFiAccessPoint* ap, void* ptr) {
            if (ptr) {
                WiFiSetupConsole* self = reinterpret_cast<WiFiSetupConsole*>(ptr);
                if (ap) {
                    if (ap->ssidLength && !strncmp(self->ssid, ap->ssid, std::max((size_t)ap->ssidLength, (size_t)strlen(self->ssid)))) {
                        self->security_ = ap->security;
                        self->cipher_ = ap->cipher;
                    }
                }
            }
        }, this);

        if (security_ == WLAN_SEC_NOT_SET)
        {
        do
        {
#if Wiring_WpaEnterprise == 0
            print("Security 0=unsecured, 1=WEP, 2=WPA, 3=WPA2: ");
            read_line(security_type_string, 1);
        }
        while ('0' > security_type_string[0] || '3' < security_type_string[0]);
#else
                print("Security 0=unsecured, 1=WEP, 2=WPA, 3=WPA2, 4=WPA Enterprise, 5=WPA2 Enterprise: ");
                read_line(security_type_string, 1);
            }
            while ('0' > security_type_string[0] || '5' < security_type_string[0]);
#endif
            security_ = (WLanSecurityType)(security_type_string[0] - '0');
        }

        if (security_ != WLAN_SEC_UNSEC)
            password[0] = '1'; // non-empty password so security isn't set to None

        credentials.setSsid(ssid);
        credentials.setPassword(password);
        credentials.setSecurity(security_);
        credentials.setCipher(cipher_);

        // dry run
        creds = credentials.getHalCredentials();
        if (this->config.connect_callback2(this->config.connect_callback_data, &creds, true)==WLAN_SET_CREDENTIALS_CIPHER_REQUIRED ||
            (cipher_ == WLAN_CIPHER_NOT_SET))
        {
            do
            {
                print("Security Cipher 1=AES, 2=TKIP, 3=AES+TKIP: ");
                read_line(security_type_string, 1);
            }
            while ('1' > security_type_string[0] || '3' < security_type_string[0]);
            switch (security_type_string[0]-'0') {
                case 1: cipher_ = WLAN_CIPHER_AES; break;
                case 2: cipher_ = WLAN_CIPHER_TKIP; break;
                case 3: cipher_ = WLAN_CIPHER_AES_TKIP; break;
            }
            credentials.setCipher(cipher_);
        }

        if (0 < security_ && security_ <= 3)
        {
            print("Password: ");
            read_line(password, sizeof(password) - 1);
            credentials.setPassword(password);
        }
#if Wiring_WpaEnterprise == 1
        else if (security_ >= 4)
        {
            do
            {
                print("EAP Type 0=PEAP/MSCHAPv2, 1=EAP-TLS: ");
                read_line(eap_type_string, 1);
            }
            while ('0' > eap_type_string[0] || '1' < eap_type_string[0]);
            int eap_type = eap_type_string[0] - '0';

            if (!tmp_) {
                tmp_.reset(new (std::nothrow) char[CERTIFICATE_SIZE]);
            }
            if (!tmp_) {
                print("Error while preparing to store enterprise credentials.\r\n\r\n");
                return;
            }

            if (eap_type == 1) {
                // EAP-TLS
                // Required:
                // - client certificate
                // - private key
                // Optional:
                // - root CA
                // - outer identity
                credentials.setEapType(WLAN_EAP_TYPE_TLS);

                memset(tmp_.get(), 0, CERTIFICATE_SIZE);
                print("Client certificate in PEM format:\r\n");
                read_multiline((char*)tmp_.get(), CERTIFICATE_SIZE - 1);
                {
                    uint8_t* der = NULL;
                    size_t der_len = 0;
                    if (!mbedtls_x509_crt_pem_to_der((const char*)tmp_.get(), strnlen(tmp_.get(), CERTIFICATE_SIZE - 1) + 1, &der, &der_len)) {
                        credentials.setClientCertificate(der, der_len);
                        free(der);
                    }
                }

                memset(tmp_.get(), 0, CERTIFICATE_SIZE);
                print("Private key in PEM format:\r\n");
                read_multiline((char*)tmp_.get(), PRIVATE_KEY_SIZE - 1);
                {
                    uint8_t* der = NULL;
                    size_t der_len = 0;
                    if (!mbedtls_pk_pem_to_der((const char*)tmp_.get(), strnlen(tmp_.get(), PRIVATE_KEY_SIZE - 1) + 1, &der, &der_len)) {
                        credentials.setPrivateKey(der, der_len);
                        free(der);
                    }
                }
            } else {
                // PEAP/MSCHAPv2
                // Required:
                // - inner identity
                // - password
                // Optional:
                // - root CA
                // - outer identity
                credentials.setEapType(WLAN_EAP_TYPE_PEAP);

                memset(tmp_.get(), 0, CERTIFICATE_SIZE);
                print("Username: ");
                read_line((char*)tmp_.get(), 64);
                credentials.setIdentity((const char*)tmp_.get());

                print("Password: ");
                read_line(password, sizeof(password) - 1);
                credentials.setPassword(password);
        }

            memset(tmp_.get(), 0, CERTIFICATE_SIZE);
            print("Outer identity (optional): ");
            read_line((char*)tmp_.get(), 64);
            if (strlen(tmp_.get())) {
                credentials.setOuterIdentity((const char*)tmp_.get());
            }

            memset(tmp_.get(), 0, CERTIFICATE_SIZE);
            print("Root CA in PEM format (optional):\r\n");
            read_multiline((char*)tmp_.get(), CERTIFICATE_SIZE - 1);
            if (strlen(tmp_.get()) && !is_empty(tmp_.get())) {
                uint8_t* der = NULL;
                size_t der_len = 0;
                if (!mbedtls_x509_crt_pem_to_der((const char*)tmp_.get(), strnlen(tmp_.get(), CERTIFICATE_SIZE - 1) + 1, &der, &der_len)) {
                    credentials.setRootCertificate(der, der_len);
                    free(der);
                }
            }
            tmp_.reset();
        }
#endif

        print("Thanks! Wait while I save those credentials...\r\n\r\n");
        creds = credentials.getHalCredentials();
        if (this->config.connect_callback2(this->config.connect_callback_data, &creds, false)==0)
        {
            print("Awesome. Now we'll connect!\r\n\r\n");
            print("If you see a pulsing cyan light, your device \r\n");
            print("has connected to the Cloud and is ready to go!\r\n\r\n");
            print("If your LED flashes red or you encounter any other problems,\r\n");
            print("visit https://www.particle.io/support to debug.\r\n\r\n");
            print("    Particle <3 you!\r\n\r\n");
        }
        else
        {
            print("Derp. Sorry, we couldn't save the credentials.\r\n\r\n");
        }
        cleanup();
    }
    else {
        super::handle(c);
    }
}

#if Wiring_WpaEnterprise == 1
void WiFiSetupConsole::cleanup()
{
    tmp_.reset();
}
#endif

void WiFiSetupConsole::exit()
{
    network_listen(0, NETWORK_LISTEN_EXIT, 0);
}

#endif


#if Wiring_Cellular

CellularSetupConsole::CellularSetupConsole(const CellularSetupConsoleConfig& config)
 : SystemSetupConsole(config)
{
}

CellularSetupConsole::~CellularSetupConsole()
{
}

void CellularSetupConsole::exit()
{
    network_listen(0, NETWORK_LISTEN_EXIT, 0);
}

void CellularSetupConsole::handle(char c)
{
	super::handle(c);
}

#endif

template class SystemSetupConsole<SystemSetupConsoleConfig>;
