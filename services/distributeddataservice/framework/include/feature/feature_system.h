/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#ifndef OHOS_DISTRIBUTED_DATA_FRAMEWORK_SYSTEM_SYSTEM_H
#define OHOS_DISTRIBUTED_DATA_FRAMEWORK_SYSTEM_SYSTEM_H
#include <memory>

#include "concurrent_map.h"
#include "error/general_error.h"
#include "executor_pool.h"
#include "visibility.h"
namespace DistributedDB {
struct AutoLaunchParam;
}
namespace OHOS {
class MessageParcel;
namespace DistributedData {
class API_EXPORT FeatureSystem {
public:
    using Error = GeneralError;
    inline static constexpr int32_t STUB_SUCCESS = Error::E_OK;
    enum BindFlag : int32_t {
        BIND_LAZY,
        BIND_NOW
    };
    class API_EXPORT Feature {
    public:
        struct BinderInfo {
            std::string bundleName;
            uint32_t localTokenId;
        };
        virtual ~Feature();
        virtual int OnRemoteRequest(uint32_t code, OHOS::MessageParcel &data, OHOS::MessageParcel &reply) = 0;
        virtual int32_t OnInitialize(const BinderInfo &binderInfo);
        virtual int32_t OnExecutor(std::shared_ptr<ExecutorPool> executors);
        virtual int32_t OnAppExit(pid_t uid, pid_t pid, uint32_t tokenId, const std::string &bundleName);
        virtual int32_t OnAppUninstall(const std::string &bundleName, int32_t user, int32_t index, uint32_t tokenId);
        virtual int32_t OnAppUpdate(const std::string &bundleName, int32_t user, int32_t index, uint32_t tokenId);
        virtual int32_t ResolveAutoLaunch(const std::string &identifier, DistributedDB::AutoLaunchParam &param);
        virtual int32_t OnUserChange(uint32_t code, const std::string &user, const std::string &account);
        virtual int32_t Online(const std::string &device);
        virtual int32_t Offline(const std::string &device);
        virtual int32_t OnReady(const std::string &device);
    };
    using Creator = std::function<std::shared_ptr<Feature>()>;
    static FeatureSystem &GetInstance();
    int32_t RegisterCreator(const std::string &name, Creator creator, int32_t flag = BIND_LAZY);
    Creator GetCreator(const std::string &name);
    std::vector<std::string> GetFeatureName(int32_t flag);

private:
    FeatureSystem() = default;
    FeatureSystem(const FeatureSystem &) = delete;
    FeatureSystem(FeatureSystem &&) noexcept = delete;
    FeatureSystem &operator=(const FeatureSystem &) = delete;
    FeatureSystem &operator=(FeatureSystem &&) = delete;

    ConcurrentMap<std::string, std::pair<Creator, int32_t>> creators_;
};
} // namespace DistributedData
}
#endif // OHOS_DISTRIBUTED_DATA_FRAMEWORK_SYSTEM_SYSTEM_H
