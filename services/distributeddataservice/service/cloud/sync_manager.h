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

#ifndef OHOS_DISTRIBUTED_DATA_SERVICES_CLOUD_SYNC_MANAGER_H
#define OHOS_DISTRIBUTED_DATA_SERVICES_CLOUD_SYNC_MANAGER_H
#include "eventcenter/event.h"
#include "executor_pool.h"
#include "store/auto_cache.h"
#include "store/general_store.h"
#include "store/general_value.h"
#include "utils/ref_count.h"
#include "concurrent_map.h"
namespace OHOS::CloudData {
class SyncManager {
public:
    using GenAsync = DistributedData::GenAsync;
    using GenStore = DistributedData::GeneralStore;
    using GenQuery = DistributedData::GenQuery;
    using RefCount = DistributedData::RefCount;
    class SyncInfo final {
    public:
        using Store = std::string;
        using Stores = std::vector<Store>;
        using Tables = std::vector<std::string>;
        using MutliStoreTables = std::map<Store, Tables>;
        SyncInfo(int32_t user, const std::string &bundleName = "", const Store &store = "", const Tables &tables = {});
        SyncInfo(int32_t user, const std::string &bundleName, const Stores &stores);
        SyncInfo(int32_t user, const std::string &bundleName, const MutliStoreTables &tables);
        void SetMode(int32_t mode);
        void SetWait(int32_t wait);
        void SetAsyncDetail(GenAsync asyncDetail);
        void SetQuery(std::shared_ptr<GenQuery> query);
        void SetError(int32_t code) const;
        std::shared_ptr<GenQuery> GenerateQuery(const std::string &store, const Tables &tables);
        inline static constexpr const char *DEFAULT_ID = "default";

    private:
        friend SyncManager;
        uint64_t syncId_ = 0;
        int32_t mode_ = GenStore::CLOUD_TIME_FIRST;
        int32_t user_ = 0;
        int32_t wait_ = 0;
        std::string id_ = DEFAULT_ID;
        std::string bundleName_;
        std::map<std::string, std::vector<std::string>> tables_;
        GenAsync async_;
        std::shared_ptr<GenQuery> query_;
    };
    SyncManager();
    ~SyncManager();
    int32_t Bind(std::shared_ptr<ExecutorPool> executor);
    int32_t DoCloudSync(SyncInfo syncInfo);
    int32_t StopCloudSync(int32_t user = 0);

private:
    using Event = DistributedData::Event;
    using Task = ExecutorPool::Task;
    using TaskId = ExecutorPool::TaskId;
    using AutoCache = DistributedData::AutoCache;
    using Duration = ExecutorPool::Duration;
    using StoreMetaData = DistributedData::StoreMetaData;
    using Retryer = std::function<bool(Duration interval, int32_t status)>;

    static constexpr ExecutorPool::Duration RETRY_INTERVAL = std::chrono::seconds(10); // second
    static constexpr ExecutorPool::Duration LOCKED_INTERVAL = std::chrono::seconds(30); // second
    static constexpr int32_t RETRY_TIMES = 6; // normal retry
    static constexpr int32_t CLIENT_RETRY_TIMES = 3; // normal retry
    static constexpr uint64_t USER_MARK = 0xFFFFFFFF00000000; // high 32 bit
    static constexpr int32_t MV_BIT = 32;

    Task GetSyncTask(int32_t times, bool retry, RefCount ref, SyncInfo &&syncInfo);
    void UpdateSchema(const SyncInfo &syncInfo);
    std::function<void(const Event &)> GetSyncHandler(Retryer retryer);
    std::function<void(const Event &)> GetClientChangeHandler();
    Retryer GetRetryer(int32_t times, const SyncInfo &syncInfo);
    static AutoCache::Store GetStore(const StoreMetaData &meta, int32_t user);
    static uint64_t GenerateId(int32_t user);
    RefCount GenSyncRef(uint64_t syncId);
    int32_t Compare(uint64_t syncId, int32_t user);

    static std::atomic<uint32_t> genId_;
    std::shared_ptr<ExecutorPool> executor_;
    ConcurrentMap<uint64_t, TaskId> actives_;
    ConcurrentMap<uint64_t, uint64_t> activeInfos_;
};
} // namespace OHOS::CloudData
#endif // OHOS_DISTRIBUTED_DATA_SERVICES_CLOUD_SYNC_MANAGER_H