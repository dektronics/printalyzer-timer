/**
 * Logging configuration file, as a stand-in for sdkconfig.h
 */

#ifndef __ESP_LOG_CONFIG_H__
#define __ESP_LOG_CONFIG_H__

#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#ifndef CONFIG_LOG_COLORS
#define CONFIG_LOG_COLORS
#endif

#ifndef CONFIG_LOG_TIMESTAMP_SOURCE_RTOS
#define CONFIG_LOG_TIMESTAMP_SOURCE_RTOS
#endif

#endif /* __ESP_LOG_CONFIG_H__ */
