/**
 * @file discovery_entity.h
 * @brief Создание строки для публикации автодискавери 
 * @version 0.1
 * @date 2023-02-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef HA_DISCOVERY_ENTITY_H_
#define HA_DISCOVERY_ENTITY_H_

#include <PubSubClient.h>
#include "resources.h"
#include "json_constructor.h"

extern void update_channel_names(int channel, String &entity_id, String &entity_name);

#endif