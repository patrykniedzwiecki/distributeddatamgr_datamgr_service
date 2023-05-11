/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef KV_STORE_MANAGER_H
#define KV_STORE_MANAGER_H
#include <string>

#include "db_config.h"
#include "check_common.h"
#include "kv_store_executor.h"

namespace DocumentDB {
class KvStoreManager {
public:
    static int GetKvStore(const std::string &path, const DBConfig &config, bool isFirstOpen,
        KvStoreExecutor *&executor);
};
} // namespace DocumentDB
#endif // KV_STORE_MANAGER_H