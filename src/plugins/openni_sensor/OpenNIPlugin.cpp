﻿#include "OpenNIPlugin.h"
#include <SenseKit/SenseKit.h>
#include <SenseKitUL/skul_ctypes.h>
#include "OniDepthStream.h"
#include "OniColorStream.h"
#include <Shiny.h>
#include <cstdio>

EXPORT_PLUGIN(sensekit::plugins::OpenNIPlugin)

namespace sensekit
{
    namespace plugins
    {
        void OpenNIPlugin::init_openni()
        {
            PROFILE_FUNC();
            openni::Version version = openni::OpenNI::getVersion();

            get_logger().info("Initializing OpenNI v%d.%d.%d.%d",
                              version.major,
                              version.minor,
                              version.maintenance,
                              version.build);

            openni::Status rc = ::openni::STATUS_OK;

            openni::OpenNI::addDeviceConnectedListener(this);
            openni::OpenNI::addDeviceDisconnectedListener(this);

            rc = openni::OpenNI::initialize();

            bool successful = rc == openni::STATUS_OK;

            if (!successful)
            {
                get_logger().warn("Failed to initialize OpenNI: %s", openni::OpenNI::getExtendedError());
            }
            else
            {
                get_logger().info("Initialized OpenNI v%d.%d.%d.%d",
                                  version.major,
                                  version.minor,
                                  version.maintenance,
                                  version.build);
            }
        }

        void OpenNIPlugin::on_host_event(sensekit_event_id id, const void* data, size_t dataSize)
        {
            PROFILE_FUNC();
#ifdef __ANDROID__
            switch (id)
            {
            case SENSEKIT_EVENT_RESOURCE_AVAILABLE:
                const char* resourceUri = static_cast<const char*>(data);

                unsigned int vid = 0;
                unsigned int pid = 0;
                unsigned int bus = 0;
                unsigned int address = 0;

                sscanf(resourceUri, "usb/%u/%u/%u/%u", &vid, &pid, &bus, &address);

                char oniUri[1024];
                snprintf(oniUri, 1024, "%.4x/%.4x@%x/%x", vid, pid, bus, address);

                openni::Array<openni::DeviceInfo> devices;
                openni::OpenNI::enumerateDevices(&devices);

                for(int i = 0; i < devices.getSize(); i++)
                {
                    const openni::DeviceInfo& info = devices[i];
                    if (strcmp(oniUri, info.getUri()) == 0)
                    {
                        get_logger().info("device connected: %s", info.getUri());

                        OniDeviceStreamSet* set = new OniDeviceStreamSet(get_pluginService(), &info);
                        set->open();

                        m_sets.push_back(SetPtr(set));
                    }
                }
            }
#endif
        }

        void OpenNIPlugin::onDeviceConnected(const ::openni::DeviceInfo* info)
        {
            PROFILE_FUNC();
#ifndef __ANDROID__
            get_logger().info("device connected: %s", info->getUri());

            OniDeviceStreamSet* set = new OniDeviceStreamSet(get_pluginService(), info);
            set->open();

            m_sets.push_back(SetPtr(set));
#endif
        }

        void OpenNIPlugin::onDeviceDisconnected(const ::openni::DeviceInfo* info)
        {
            PROFILE_FUNC();
            get_logger().info("device disconnected: %s", info->getUri());
            auto it = std::find_if(m_sets.begin(), m_sets.end(),
                                   [&info] (SetPtr& setPtr)
                                   -> bool
                                   {
                                       return setPtr->get_uri() == info->getUri();
                                   });

            m_sets.erase(it);
        }

        OpenNIPlugin::~OpenNIPlugin()
        {
            PROFILE_FUNC();
            #ifndef __ANDROID__
                PROFILE_UPDATE();
                PROFILE_OUTPUT("profile_openni_sensor.txt");
            #endif

            m_sets.clear();
            get_logger().info("shutting down openni");
            openni::OpenNI::shutdown();
        }

        void OpenNIPlugin::temp_update()
        {
            PROFILE_FUNC();
            read_streams();
            PROFILE_UPDATE();
        }

        sensekit_status_t OpenNIPlugin::read_streams()
        {
            PROFILE_FUNC();
            for(auto& set : m_sets)
            {
                set->read();
            }

            return SENSEKIT_STATUS_SUCCESS;
        }
    }
}
