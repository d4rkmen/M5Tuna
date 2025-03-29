/**
 * @file usb.cpp
 * @brief USB MSC device handling implementation
 */
#ifdef HAVE_USB
#include "esp_log.h"
#include "../hal.h"
#include "usb.h"
#include <sys/stat.h>
#include <string.h>

static const char* TAG = "USB";
// workaround for msc_host.h cant see MSC_DEVICE_CONNECTED and MSC_DEVICE_DISCONNECTED
#define MSC_DEVICE_CONNECTED 0
#define MSC_DEVICE_DISCONNECTED 1

namespace HAL
{
    const char* USB::MOUNT_POINT = "/usb";
    /**
     * @brief Context structure passed to USB task
     */
    struct USBTaskContext
    {
        USB* usb_instance;
    };

    USB::USB(void* hal)
        : _usb_task_handle(nullptr), _usb_initialized(false), _device_connected(false), _is_mounted(false), _device_addr(0),
          _device_info({}), _msc_device(nullptr), _vfs_handle(nullptr), _hal(hal)
    {
        _app_queue = xQueueCreate(5, sizeof(TaskMessage));
        BaseType_t task_created;
        if (_app_queue == nullptr)
        {
            ESP_LOGE(TAG, "Failed to create app queue");
            goto error;
        }

        _usb_initialized = true;

        task_created = xTaskCreate(usb_task, "usb_task", 4096, this, 5, &_usb_task_handle);
        if (task_created != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create USB task");
            goto error;
        }
        task_created = xTaskCreate(msc_task, "msc_task", 4096, this, 5, &_msc_task_handle);
        if (task_created != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create MSC task");
            goto error;
        }
        return;
    error:
        if (_app_queue != nullptr)
        {
            vQueueDelete(_app_queue);
            _app_queue = nullptr;
        }
        if (_msc_task_handle != nullptr)
        {
            vTaskDelete(_msc_task_handle);
            _msc_task_handle = nullptr;
        }
        if (_usb_task_handle != nullptr)
        {
            vTaskDelete(_usb_task_handle);
            _usb_task_handle = nullptr;
        }
    }

    USB::~USB()
    {
        if (_usb_initialized)
        {
            // Unmount if mounted
            if (_is_mounted)
            {
                unmount();
            }
            // Uninstall USB host
            _usb_initialized = false;
            // Wait for task to clean up
            vTaskDelay(pdMS_TO_TICKS(100));

            // Delete queue
            if (_app_queue)
            {
                vQueueDelete(_app_queue);
                _app_queue = nullptr;
            }
            if (_msc_task_handle)
            {
                vTaskDelete(_msc_task_handle);
                _msc_task_handle = nullptr;
            }
            if (_usb_task_handle)
            {
                vTaskDelete(_usb_task_handle);
                _usb_task_handle = nullptr;
            }
        }
    }

    /**
     * @brief MSC event callback
     * @param event_data Event data
     * @param arg Argument
     */
    void USB::msc_event_callback(const void* event_data, void* arg)
    {
        USB* usb = static_cast<USB*>(arg);

        const msc_host_event_t* event = static_cast<const msc_host_event_t*>(event_data);

        if (event->event == MSC_DEVICE_CONNECTED)
        {
            ESP_LOGI(TAG, "MSC device connected (usb_addr=%d)", event->device.address);
            TaskMessage message = {
                .type = MSG_DEVICE_CONNECTED,
                .device_addr = event->device.address,
            };
            xQueueSend(usb->_app_queue, &message, portMAX_DELAY);
        }
        else if (event->event == MSC_DEVICE_DISCONNECTED)
        {
            ESP_LOGI(TAG, "MSC device disconnected");
            TaskMessage message = {
                .type = MSG_DEVICE_DISCONNECTED,
                .device_addr = 0,
            };
            xQueueSend(usb->_app_queue, &message, portMAX_DELAY);
        }
    }

    void USB::usb_task(void* arg)
    {
        USB* usb = static_cast<USB*>(arg);
        ESP_LOGI(TAG, "USB task started");

        // Install USB Host Library
        const usb_host_config_t host_config = {.skip_phy_setup = false,
                                               .root_port_unpowered = false,
                                               .intr_flags = ESP_INTR_FLAG_LEVEL1,
                                               .enum_filter_cb = nullptr};
        ESP_ERROR_CHECK(usb_host_install(&host_config));

        bool has_clients = false;
        while (usb->_usb_initialized)
        {
            if (!has_clients)
            {
                vTaskDelay(pdMS_TO_TICKS(10));
                const msc_host_driver_config_t msc_config = {.create_backround_task = true,
                                                             .task_priority = 5,
                                                             .stack_size = 4096,
                                                             .core_id = tskNO_AFFINITY,
                                                             .callback = (msc_host_event_cb_t)msc_event_callback,
                                                             .callback_arg = arg};
                // Install MSC driver
                ESP_LOGI(TAG, "Installing MSC driver");
                ESP_ERROR_CHECK(msc_host_install(&msc_config));
                has_clients = true;
            }

            // Handle USB library events
            uint32_t event_flags;
            usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
            // Release devices once all clients has deregistered
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            {
                has_clients = false;
                usb->_device_connected = false;
                usb->_is_mounted = false;
                if (usb_host_device_free_all() == ESP_OK)
                {
                    ESP_LOGI(TAG, "All clients freed, uninstalling MSC driver");
                    msc_host_uninstall();
                };
            }
        }
        ESP_LOGD(TAG, "Deinitializing USB");
        vTaskDelay(pdMS_TO_TICKS(10)); // Give clients some time to uninstall
        usb_host_uninstall();
        vTaskDelete(NULL);
    }
    void USB::msc_task(void* arg)
    {
        USB* usb = static_cast<USB*>(arg);
        ESP_LOGI(TAG, "MSC task started");
        HAL::Hal* hal = (HAL::Hal*)usb->_hal;
        esp_err_t err;
        while (usb->_usb_initialized)
        {
            TaskMessage message;
            if (xQueueReceive(usb->_app_queue, &message, portMAX_DELAY) == pdPASS)
            {
                switch (message.type)
                {
                case MSG_DEVICE_CONNECTED:
                    // play connected sound
                    if (hal)
                    {
                        hal->playDeviceConnectedSound();
                    }
                    // unmount old device if connected
                    if (usb->_device_connected)
                    {
                        usb->unmount();
                        err = msc_host_uninstall_device(usb->_msc_device);
                        if (err != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Failed to uninstall MSC device: %s", esp_err_to_name(err));
                        }
                    }
                    usb->_device_addr = message.device_addr;
                    // Open MSC device
                    err = msc_host_install_device(message.device_addr, &usb->_msc_device);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to install MSC device: %s", esp_err_to_name(err));
                        usb->_device_connected = false;
                    }
                    else
                    {
                        ESP_LOGI(TAG, "MSC device installed: %p", usb->_msc_device);
                        usb->_device_connected = true;
                    }
                    break;
                case MSG_DEVICE_DISCONNECTED:
                    // play disconnected sound
                    if (hal)
                    {
                        hal->playDeviceDisconnectedSound();
                    }
                    usb->unmount();
                    err = msc_host_uninstall_device(usb->_msc_device);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to uninstall MSC device: %s", esp_err_to_name(err));
                    }
                    usb->_msc_device = nullptr;
                    usb->_device_addr = 0;
                    usb->_device_connected = false;
                    break;
                }
            }
        }
        ESP_LOGI(TAG, "MSC task stopped");
        vTaskDelete(NULL);
    }

    bool USB::mount()
    {
        if (!_usb_initialized || !_device_connected)
        {
            ESP_LOGW(TAG, "Mount: USB not initialized or no device connected");
            return false;
        }

        if (_is_mounted)
        {
            ESP_LOGI(TAG, "Mount: device already mounted");
            return true;
        }
        const esp_vfs_fat_mount_config_t mount_config = {.format_if_mount_failed = false,
                                                         .max_files = 3,
                                                         .allocation_unit_size = 8192,
                                                         .disk_status_check_enable = false,
                                                         .use_one_fat = false};
        ESP_LOGI(TAG, "Mounting USB device: %p %s", _msc_device, MOUNT_POINT);
        esp_err_t err = msc_host_vfs_register(_msc_device, MOUNT_POINT, &mount_config, &_vfs_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register VFS: %s", esp_err_to_name(err));
            return false;
        }
        // Read device info
        msc_host_device_info_t device_info;
        err = msc_host_get_device_info(_msc_device, &device_info);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get device info: %s", esp_err_to_name(err));
            return false;
        }
        // Update device info
        _device_info.idVendor = device_info.idVendor;
        _device_info.idProduct = device_info.idProduct;
        std::wstring ws_man(device_info.iManufacturer);
        _device_info.manufacturer = std::string(ws_man.begin(), ws_man.end());
        std::wstring ws_prod(device_info.iProduct);
        _device_info.product = std::string(ws_prod.begin(), ws_prod.end());
        std::wstring ws_serial(device_info.iSerialNumber);
        _device_info.serialNumber = std::string(ws_serial.begin(), ws_serial.end());
        _device_info.sectorSize = device_info.sector_size;
        _device_info.sectorCount = device_info.sector_count;
        _device_info.capacityBytes = (uint64_t)device_info.sector_size * device_info.sector_count;
        ESP_LOGD(TAG,
                 "Device info: %s %s %s %ldx%ld=%lld",
                 _device_info.manufacturer.c_str(),
                 _device_info.product.c_str(),
                 _device_info.serialNumber.c_str(),
                 _device_info.sectorSize,
                 _device_info.sectorCount,
                 _device_info.capacityBytes);
        // msc_host_print_descriptors(_msc_device);

        _is_mounted = true;
        ESP_LOGI(TAG, "Mount: device mounted at %s", MOUNT_POINT);
        return true;
    }

    bool USB::unmount()
    {
        ESP_LOGI(TAG, "Unmounting USB device");
        if (!_is_mounted)
        {
            ESP_LOGI(TAG, "Unmount: device is not mounted");
            return true;
        }

        esp_err_t err;
        if (_vfs_handle != nullptr)
        {
            err = msc_host_vfs_unregister(_vfs_handle);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to unmount VFS: %s", esp_err_to_name(err));
            }
            _vfs_handle = nullptr;
        }
        _is_mounted = false;

        ESP_LOGD(TAG, "USB device unmounted");
        return true;
    }

} // namespace HAL
#endif