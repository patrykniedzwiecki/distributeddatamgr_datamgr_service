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
#define LOG_TAG "RdbServiceImpl"
#include "rdb_service_impl.h"
#include "accesstoken_kit.h"
#include "account/account_delegate.h"
#include "checker/checker_manager.h"
#include "cloud/cloud_event.h"
#include "communicator/device_manager_adapter.h"
#include "crypto_manager.h"
#include "directory_manager.h"
#include "eventcenter/event_center.h"
#include "ipc_skeleton.h"
#include "log_print.h"
#include "metadata/meta_data_manager.h"
#include "metadata/store_meta_data.h"
#include "permission/permission_validator.h"
#include "rdb_notifier_proxy.h"
#include "store/auto_cache.h"
#include "types_export.h"
#include "utils/anonymous.h"
#include "utils/constant.h"
#include "utils/converter.h"
#include "cloud/schema_meta.h"
#include "rdb_general_store.h"
using OHOS::DistributedKv::AccountDelegate;
using OHOS::DistributedData::CheckerManager;
using OHOS::DistributedData::MetaDataManager;
using OHOS::DistributedData::StoreMetaData;
using OHOS::DistributedData::Anonymous;
using namespace OHOS::DistributedData;
using namespace OHOS::Security::AccessToken;
using DistributedDB::RelationalStoreManager;
using DmAdapter = OHOS::DistributedData::DeviceManagerAdapter;

constexpr uint32_t ITERATE_TIMES = 10000;
namespace OHOS::DistributedRdb {
__attribute__((used)) RdbServiceImpl::Factory RdbServiceImpl::factory_;
RdbServiceImpl::Factory::Factory()
{
    FeatureSystem::GetInstance().RegisterCreator("relational_store", []() {
        return std::make_shared<RdbServiceImpl>();
    });
    AutoCache::GetInstance().RegCreator(RDB_DEVICE_COLLABORATION, [](const StoreMetaData &metaData) -> GeneralStore* {
        return new RdbGeneralStore(metaData);
    });
}

RdbServiceImpl::Factory::~Factory()
{
}

RdbServiceImpl::DeathRecipientImpl::DeathRecipientImpl(const DeathCallback& callback)
    : callback_(callback)
{
    ZLOGI("construct");
}

RdbServiceImpl::DeathRecipientImpl::~DeathRecipientImpl()
{
    ZLOGI("destroy");
}

void RdbServiceImpl::DeathRecipientImpl::OnRemoteDied(const wptr<IRemoteObject> &object)
{
    ZLOGI("enter");
    if (callback_) {
        callback_();
    }
}

RdbServiceImpl::RdbServiceImpl()
    : timer_("SyncerTimer", -1), autoLaunchObserver_(this)
{
    ZLOGI("construct");
    timer_.Setup();
    DistributedDB::RelationalStoreManager::SetAutoLaunchRequestCallback(
        [this](const std::string& identifier, DistributedDB::AutoLaunchParam &param) {
            return ResolveAutoLaunch(identifier, param);
        });
}

int32_t RdbServiceImpl::ResolveAutoLaunch(const std::string &identifier, DistributedDB::AutoLaunchParam &param)
{
    std::string identifierHex = TransferStringToHex(identifier);
    ZLOGI("%{public}.6s", identifierHex.c_str());
    std::vector<StoreMetaData> entries;
    auto localId = DmAdapter::GetInstance().GetLocalDevice().uuid;
    if (!MetaDataManager::GetInstance().LoadMeta(StoreMetaData::GetPrefix({ localId }), entries)) {
        ZLOGE("get meta failed");
        return false;
    }
    ZLOGI("size=%{public}d", static_cast<int32_t>(entries.size()));
    for (const auto& entry : entries) {
        if (entry.storeType != RDB_DEVICE_COLLABORATION) {
            continue;
        }

        auto aIdentifier = DistributedDB::RelationalStoreManager::GetRelationalStoreIdentifier(
            entry.user, entry.appId, entry.storeId);
        ZLOGI("%{public}s %{public}s %{public}s", entry.user.c_str(), entry.appId.c_str(), entry.storeId.c_str());
        if (aIdentifier != identifier) {
            continue;
        }
        ZLOGI("find identifier %{public}s", entry.storeId.c_str());
        param.userId = entry.user;
        param.appId = entry.appId;
        param.storeId = entry.storeId;
        param.path = entry.dataDir;
        param.option.storeObserver = &autoLaunchObserver_;
        param.option.isEncryptedDb = entry.isEncrypt;
        if (entry.isEncrypt) {
            param.option.iterateTimes = ITERATE_TIMES;
            param.option.cipher = DistributedDB::CipherType::AES_256_GCM;
            RdbSyncer::GetPassword(entry, param.option.passwd);
        }
        return true;
    }

    ZLOGE("not find identifier");
    return false;
}

void RdbServiceImpl::OnClientDied(pid_t pid)
{
    ZLOGI("client dead pid=%{public}d", pid);
    syncers_.ComputeIfPresent(pid, [this](const auto& key, StoreSyncersType& syncers) {
        syncerNum_ -= static_cast<int32_t>(syncers.size());
        for (const auto& [name, syncer] : syncers) {
            timer_.Unregister(syncer->GetTimerId());
        }
        return false;
    });
    notifiers_.Erase(pid);
    identifiers_.EraseIf([pid](const auto& key, pid_t& value) {
        return pid == value;
    });
}

bool RdbServiceImpl::CheckAccess(const std::string& bundleName, const std::string& storeName)
{
    CheckerManager::StoreInfo storeInfo;
    storeInfo.uid = IPCSkeleton::GetCallingUid();
    storeInfo.tokenId = IPCSkeleton::GetCallingTokenID();
    storeInfo.bundleName = bundleName;
    storeInfo.storeId = RdbSyncer::RemoveSuffix(storeName);
    auto instanceId = RdbSyncer::GetInstIndex(storeInfo.tokenId, storeInfo.bundleName);
    if (instanceId != 0) {
        return false;
    }
    return !CheckerManager::GetInstance().GetAppId(storeInfo).empty();
}

std::string RdbServiceImpl::ObtainDistributedTableName(const std::string &device, const std::string &table)
{
    ZLOGI("device=%{public}s table=%{public}s", Anonymous::Change(device).c_str(), table.c_str());
    auto uuid = DmAdapter::GetInstance().GetUuidByNetworkId(device);
    if (uuid.empty()) {
        ZLOGE("get uuid failed");
        return "";
    }
    return DistributedDB::RelationalStoreManager::GetDistributedTableName(uuid, table);
}

int32_t RdbServiceImpl::InitNotifier(const RdbSyncerParam &param, const sptr<IRemoteObject> notifier)
{
    if (!CheckAccess(param.bundleName_, "")) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }
    if (notifier == nullptr) {
        ZLOGE("notifier is null");
        return RDB_ERROR;
    }
    pid_t pid = IPCSkeleton::GetCallingPid();
    auto recipient = new (std::nothrow) DeathRecipientImpl([this, pid] {
        OnClientDied(pid);
    });
    if (recipient == nullptr) {
        ZLOGE("malloc recipient failed");
        return RDB_ERROR;
    }

    if (!notifier->AddDeathRecipient(recipient)) {
        ZLOGE("link to death failed");
        return RDB_ERROR;
    }
    notifiers_.Insert(pid, iface_cast<RdbNotifierProxy>(notifier));
    ZLOGI("success pid=%{public}d", pid);

    return RDB_OK;
}

void RdbServiceImpl::OnDataChange(pid_t pid, const DistributedDB::StoreChangedData &data)
{
    DistributedDB::StoreProperty property;
    data.GetStoreProperty(property);
    ZLOGI("%{public}d %{public}s", pid, property.storeId.c_str());
    if (pid == 0) {
        auto identifier = RelationalStoreManager::GetRelationalStoreIdentifier(property.userId, property.appId,
                                                                               property.storeId);
        auto pair = identifiers_.Find(TransferStringToHex(identifier));
        if (!pair.first) {
            ZLOGI("client doesn't subscribe");
            return;
        }
        pid = pair.second;
        ZLOGI("fixed pid=%{public}d", pid);
    }
    notifiers_.ComputeIfPresent(pid, [&data, &property] (const auto& key, const sptr<RdbNotifierProxy>& value) {
        std::string device = data.GetDataChangeDevice();
        auto networkId = DmAdapter::GetInstance().ToNetworkID(device);
        value->OnChange(property.storeId, { networkId });
        return true;
    });
}

void RdbServiceImpl::SyncerTimeout(std::shared_ptr<RdbSyncer> syncer)
{
    if (syncer == nullptr) {
        return;
    }
    ZLOGI("%{public}s", syncer->GetStoreId().c_str());
    syncers_.ComputeIfPresent(syncer->GetPid(), [this, &syncer](const auto& key, StoreSyncersType& syncers) {
        syncers.erase(syncer->GetStoreId());
        syncerNum_--;
        return true;
    });
}

std::shared_ptr<RdbSyncer> RdbServiceImpl::GetRdbSyncer(const RdbSyncerParam &param)
{
    pid_t pid = IPCSkeleton::GetCallingPid();
    pid_t uid = IPCSkeleton::GetCallingUid();
    uint32_t tokenId = IPCSkeleton::GetCallingTokenID();
    std::shared_ptr<RdbSyncer> syncer;
    syncers_.Compute(pid, [this, &param, pid, uid, tokenId, &syncer] (const auto& key, StoreSyncersType& syncers) {
        auto storeId = RdbSyncer::RemoveSuffix(param.storeName_);
        auto it = syncers.find(storeId);
        if (it != syncers.end()) {
            syncer = it->second;
            if (!param.isEncrypt_ || param.password_.empty()) {
                timer_.Unregister(syncer->GetTimerId());
                uint32_t timerId = timer_.Register([this, syncer]() { SyncerTimeout(syncer); }, SYNCER_TIMEOUT, true);
                syncer->SetTimerId(timerId);
                return true;
            }
            syncers.erase(storeId);
        }
        if (syncers.size() >= MAX_SYNCER_PER_PROCESS) {
            ZLOGE("%{public}d exceed MAX_PROCESS_SYNCER_NUM", pid);
            return !syncers.empty();
        }
        if (syncerNum_ >= MAX_SYNCER_NUM) {
            ZLOGE("no available syncer");
            return !syncers.empty();
        }
        auto rdbObserver = new (std::nothrow) RdbStoreObserverImpl(this, pid);
        if (rdbObserver == nullptr) {
            return !syncers.empty();
        }
        auto syncer_ = std::make_shared<RdbSyncer>(param, rdbObserver);
        if (syncer_->Init(pid, uid, tokenId) != 0) {
            return !syncers.empty();
        }
        syncers[storeId] = syncer_;
        syncer = syncer_;
        syncerNum_++;
        uint32_t timerId = timer_.Register([this, syncer]() { SyncerTimeout(syncer); }, SYNCER_TIMEOUT, true);
        syncer->SetTimerId(timerId);
        return !syncers.empty();
    });

    if (syncer != nullptr) {
        identifiers_.Insert(syncer->GetIdentifier(), pid);
    } else {
        ZLOGE("syncer is nullptr");
    }
    return syncer;
}

int32_t RdbServiceImpl::SetDistributedTables(const RdbSyncerParam &param, const std::vector<std::string> &tables)
{
    ZLOGI("enter");
    if (!CheckAccess(param.bundleName_, param.storeName_)) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }
    auto syncer = GetRdbSyncer(param);
    if (syncer == nullptr) {
        return RDB_ERROR;
    }
    return syncer->SetDistributedTables(tables);
}

int32_t RdbServiceImpl::DoSync(const RdbSyncerParam &param, const SyncOption &option,
                               const RdbPredicates &predicates, SyncResult &result)
{
    if (!CheckAccess(param.bundleName_, param.storeName_)) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }
    auto syncer = GetRdbSyncer(param);
    if (syncer == nullptr) {
        return RDB_ERROR;
    }
    return syncer->DoSync(option, predicates, result);
}

void RdbServiceImpl::OnAsyncComplete(pid_t pid, uint32_t seqNum, const SyncResult &result)
{
    ZLOGI("pid=%{public}d seqnum=%{public}u", pid, seqNum);
    notifiers_.ComputeIfPresent(pid, [seqNum, &result] (const auto& key, const sptr<RdbNotifierProxy>& value) {
        value->OnComplete(seqNum, result);
        return true;
    });
}

int32_t RdbServiceImpl::DoAsync(const RdbSyncerParam &param, uint32_t seqNum, const SyncOption &option,
                                const RdbPredicates &predicates)
{
    if (!CheckAccess(param.bundleName_, param.storeName_)) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }
    pid_t pid = IPCSkeleton::GetCallingPid();
    ZLOGI("seq num=%{public}u", seqNum);
    auto syncer = GetRdbSyncer(param);
    if (syncer == nullptr) {
        return RDB_ERROR;
    }
    return syncer->DoAsync(option, predicates, [this, pid, seqNum](const SyncResult &result) {
        OnAsyncComplete(pid, seqNum, result);
    });
}

std::string RdbServiceImpl::TransferStringToHex(const std::string &origStr)
{
    if (origStr.empty()) {
        return "";
    }
    const char *hex = "0123456789abcdef";
    std::string tmp;
    for (auto item : origStr) {
        auto currentByte = static_cast<uint8_t>(item);
        tmp.push_back(hex[currentByte >> 4]); // high 4 bit to one hex.
        tmp.push_back(hex[currentByte & 0x0F]); // low 4 bit to one hex.
    }
    return tmp;
}

std::string RdbServiceImpl::GenIdentifier(const RdbSyncerParam &param)
{
    pid_t uid = IPCSkeleton::GetCallingUid();
    uint32_t token = IPCSkeleton::GetCallingTokenID();
    auto storeId = RdbSyncer::RemoveSuffix(param.storeName_);
    CheckerManager::StoreInfo storeInfo{ uid, token, param.bundleName_, storeId };
    auto userId = AccountDelegate::GetInstance()->GetUserByToken(token);
    std::string appId = CheckerManager::GetInstance().GetAppId(storeInfo);
    std::string identifier = RelationalStoreManager::GetRelationalStoreIdentifier(
        std::to_string(userId), appId, storeId);
    return TransferStringToHex(identifier);
}

int32_t RdbServiceImpl::DoSubscribe(const RdbSyncerParam& param)
{
    pid_t pid = IPCSkeleton::GetCallingPid();
    auto identifier = GenIdentifier(param);
    ZLOGI("%{public}s %{public}.6s %{public}d", param.storeName_.c_str(), identifier.c_str(), pid);
    identifiers_.Insert(identifier, pid);
    return RDB_OK;
}

int32_t RdbServiceImpl::DoUnSubscribe(const RdbSyncerParam& param)
{
    auto identifier = GenIdentifier(param);
    ZLOGI("%{public}s %{public}.6s", param.storeName_.c_str(), identifier.c_str());
    identifiers_.Erase(identifier);
    return RDB_OK;
}

int32_t RdbServiceImpl::RemoteQuery(const RdbSyncerParam& param, const std::string& device, const std::string& sql,
                                    const std::vector<std::string>& selectionArgs, sptr<IRemoteObject>& resultSet)
{
    if (!CheckAccess(param.bundleName_, param.storeName_)) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }
    auto syncer = GetRdbSyncer(param);
    if (syncer == nullptr) {
        ZLOGE("syncer is null");
        return RDB_ERROR;
    }
    return syncer->RemoteQuery(device, sql, selectionArgs, resultSet);
}

int32_t RdbServiceImpl::CreateRDBTable(
    const RdbSyncerParam &param, const std::string &writePermission, const std::string &readPermission)
{
    if (!CheckAccess(param.bundleName_, param.storeName_)) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }

    pid_t pid = IPCSkeleton::GetCallingPid();
    auto rdbObserver = new (std::nothrow) RdbStoreObserverImpl(this, pid);
    if (rdbObserver == nullptr) {
        return RDB_ERROR;
    }
    auto syncer = new (std::nothrow) RdbSyncer(param, rdbObserver);
    if (syncer == nullptr) {
        ZLOGE("new syncer error");
        return RDB_ERROR;
    }
    auto uid = IPCSkeleton::GetCallingUid();
    auto tokenId = IPCSkeleton::GetCallingTokenID();
    if (syncer->Init(pid, uid, tokenId, writePermission, readPermission) != RDB_OK) {
        ZLOGE("Init error");
        delete syncer;
        return RDB_ERROR;
    }
    delete syncer;
    return RDB_OK;
}

int32_t RdbServiceImpl::DestroyRDBTable(const RdbSyncerParam &param)
{
    if (!CheckAccess(param.bundleName_, param.storeName_)) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }
    pid_t pid = IPCSkeleton::GetCallingPid();
    auto rdbObserver = new (std::nothrow) RdbStoreObserverImpl(this, pid);
    if (rdbObserver == nullptr) {
        return RDB_ERROR;
    }
    auto syncer = new (std::nothrow) RdbSyncer(param, rdbObserver);
    if (syncer == nullptr) {
        ZLOGE("new syncer error");
        return RDB_ERROR;
    }

    StoreMetaData meta;
    if (syncer->DestroyMetaData(meta) != RDB_OK) {
        ZLOGE("destroy meta data error");
        delete syncer;
        return RDB_ERROR;
    }
    delete syncer;
    return RDB_OK;
}

int32_t RdbServiceImpl::OnInitialize()
{
    CloudEvent::StoreInfo storeInfo = { IPCSkeleton::GetCallingTokenID() };
    auto initEvt = std::make_unique<CloudEvent>(CloudEvent::FEATURE_INIT, storeInfo);
    EventCenter::GetInstance().PostEvent(std::move(initEvt));
    return RDB_OK;
}

int32_t RdbServiceImpl::GetSchema(const RdbSyncerParam &param)
{
    if (!CheckAccess(param.bundleName_, param.storeName_)) {
        ZLOGE("permission error");
        return RDB_ERROR;
    }

    if (param.schemaVersion == -1) {
        return RDB_OK;
    }

    auto storeMeta = GetStoreMetaData(param);
    StoreMetaData oldMeta;
    bool isCreated = MetaDataManager::GetInstance().LoadMeta(storeMeta.GetKey(), oldMeta, true);
    if (isCreated && (oldMeta.storeType != storeMeta.storeType ||
                      Constant::NotEqual(oldMeta.isEncrypt, storeMeta.isEncrypt) ||
                      oldMeta.area != storeMeta.area)) {
        ZLOGE("meta bundle:%{public}s store:%{public}s type:%{public}d->%{public}d encrypt:%{public}d->%{public}d "
              "area:%{public}d->%{public}d",
            storeMeta.bundleName.c_str(), storeMeta.storeId.c_str(), oldMeta.storeType, storeMeta.storeType,
            oldMeta.isEncrypt, storeMeta.isEncrypt, oldMeta.area, storeMeta.area);
        return RDB_ERROR;
    }
    auto saved = MetaDataManager::GetInstance().SaveMeta(storeMeta.GetKey(), storeMeta, true);
    if (!saved) {
        return RDB_ERROR;
    }
    CloudEvent::StoreInfo storeInfo{ IPCSkeleton::GetCallingTokenID(), param.bundleName_, param.storeName_,
        storeMeta.instanceId, param.schemaVersion };
    auto event = std::make_unique<CloudEvent>(CloudEvent::GET_SCHEMA, std::move(storeInfo), "relational_store");
    EventCenter::GetInstance().PostEvent(move(event));
    return RDB_OK;
}

StoreMetaData RdbServiceImpl::GetStoreMetaData(const RdbSyncerParam &param)
{
    StoreMetaData metaData;
    metaData.uid = IPCSkeleton::GetCallingUid();
    metaData.tokenId = IPCSkeleton::GetCallingTokenID();
    metaData.instanceId = GetInstIndex(metaData.tokenId, param.bundleName_, param.storeName_);
    metaData.bundleName = param.bundleName_;
    metaData.deviceId = DmAdapter::GetInstance().GetLocalDevice().uuid;
    metaData.storeId = param.storeName_;
    metaData.user = std::to_string(AccountDelegate::GetInstance()->GetUserByToken(metaData.tokenId));
    metaData.storeType = param.type_;
    metaData.securityLevel = param.level_;
    metaData.area = param.area_;
    metaData.appId = CheckerManager::GetInstance().GetAppId(Converter::ConvertToStoreInfo(metaData));
    metaData.appType = "harmony";
    metaData.hapName = param.hapName_;
    metaData.dataDir = DirectoryManager::GetInstance().GetStorePath(metaData) + "/" + param.storeName_;
    metaData.account = AccountDelegate::GetInstance()->GetCurrentAccountId();
    metaData.isEncrypt = param.isEncrypt_;
    return metaData;
}

int32_t RdbServiceImpl::GetInstIndex(uint32_t tokenId, const std::string &bundleName, const std::string &storeName)
{
    if (AccessTokenKit::GetTokenTypeFlag(tokenId) != TOKEN_HAP) {
        return 0;
    }

    HapTokenInfo tokenInfo;
    tokenInfo.instIndex = -1;
    int errCode = AccessTokenKit::GetHapTokenInfo(tokenId, tokenInfo);
    if (errCode != RET_SUCCESS) {
        ZLOGE("GetHapTokenInfo error:%{public}d, tokenId:0x%{public}x bundleName:%{public}s storeName:%{public}s",
            errCode, tokenId, bundleName.c_str(), storeName.c_str());
        return -1;
    }
    return tokenInfo.instIndex;
}

} // namespace OHOS::DistributedRdb
