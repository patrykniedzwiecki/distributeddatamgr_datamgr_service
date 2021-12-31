/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifdef RELATIONAL_STORE
#include "sqlite_relational_store.h"

#include "db_errno.h"
#include "log_print.h"
#include "db_types.h"
#include "sqlite_relational_store_connection.h"

namespace DistributedDB {
SQLiteRelationalStore::~SQLiteRelationalStore()
{
    delete sqliteStorageEngine_;
}


// Called when a new connection created.
void SQLiteRelationalStore::IncreaseConnectionCounter()
{
    connectionCount_.fetch_add(1, std::memory_order_seq_cst);
    if (connectionCount_.load() > 0) {
        sqliteStorageEngine_->SetConnectionFlag(true);
    }
}

RelationalStoreConnection *SQLiteRelationalStore::GetDBConnection(int &errCode)
{
    std::lock_guard<std::mutex> lock(connectMutex_);
    RelationalStoreConnection* connection = new (std::nothrow) SQLiteRelationalStoreConnection(this);

    if (connection == nullptr) {
        errCode = -E_OUT_OF_MEMORY;
        return nullptr;
    }
    IncObjRef(this);
    IncreaseConnectionCounter();
    return connection;
}

static void InitDataBaseOption(const DBProperties &kvDBProp, OpenDbProperties &option)
{
    option.uri = kvDBProp.GetStringProp(KvDBProperties::DATA_DIR, "");
    option.createIfNecessary = kvDBProp.GetBoolProp(KvDBProperties::CREATE_IF_NECESSARY, false);
}

int SQLiteRelationalStore::InitStorageEngine(const DBProperties &kvDBProp)
{
    OpenDbProperties option;
    InitDataBaseOption(kvDBProp, option);

    StorageEngineAttr poolSize = {1, 1, 0, 16}; // at most 1 write 16 read.
    int errCode = sqliteStorageEngine_->InitSQLiteStorageEngine(poolSize, option);
    if (errCode != E_OK) {
        LOGE("Init the sqlite storage engine failed:%d", errCode);
    }
    return errCode;
}

int SQLiteRelationalStore::Open(const DBProperties &properties)
{
    sqliteStorageEngine_ = new (std::nothrow) SQLiteSingleRelationalStorageEngine();
    if (sqliteStorageEngine_ == nullptr) {
        LOGE("[RelationalStore] Create storage engine failed");
        return -E_OUT_OF_MEMORY;
    }

    int errCode = InitStorageEngine(properties);
    if (errCode != E_OK) {
        LOGE("[RelationalStore][Open] Init database context fail! errCode = [%d]", errCode);
        return errCode;
    }
    storageEngine_ = new(std::nothrow) RelationalSyncAbleStorage(sqliteStorageEngine_);
    syncEngine_ = std::make_shared<SyncAbleEngine>(storageEngine_);
    return E_OK;
}

void SQLiteRelationalStore::OnClose(const std::function<void(void)> &notifier)
{
    AutoLock lockGuard(this);
    if (notifier) {
        closeNotifiers_.push_back(notifier);
    } else {
        LOGW("Register 'Close()' notifier failed, notifier is null.");
    }
}

SQLiteSingleVerRelationalStorageExecutor *SQLiteRelationalStore::GetHandle(bool isWrite, int &errCode) const
{
    if (sqliteStorageEngine_ == nullptr) {
        errCode = -E_INVALID_DB;
        return nullptr;
    }

    return static_cast<SQLiteSingleVerRelationalStorageExecutor *>(sqliteStorageEngine_->FindExecutor(isWrite,
        OperatePerm::NORMAL_PERM, errCode));
}
void SQLiteRelationalStore::ReleaseHandle(SQLiteSingleVerRelationalStorageExecutor *&handle) const
{
    if (handle == nullptr) {
        return;
    }

    if (sqliteStorageEngine_ != nullptr) {
        StorageExecutor *databaseHandle = handle;
        sqliteStorageEngine_->Recycle(databaseHandle);
        handle = nullptr;
    }
}

int SQLiteRelationalStore::Sync(const ISyncer::SyncParma &syncParam)
{
    return syncEngine_->Sync(syncParam);
}

// Called when a connection released.
void SQLiteRelationalStore::DecreaseConnectionCounter()
{
    int count = connectionCount_.fetch_sub(1, std::memory_order_seq_cst);
    if (count <= 0) {
        LOGF("Decrease db connection counter failed, count <= 0.");
        return;
    }
    if (count != 1) {
        return;
    }

    LockObj();
    auto notifiers = std::move(closeNotifiers_);
    UnlockObj();

    for (auto &notifier : notifiers) {
        if (notifier) {
            notifier();
        }
    }

    // Sync Close
    syncEngine_->Close();

    if (sqliteStorageEngine_ != nullptr) {
        delete sqliteStorageEngine_;
        sqliteStorageEngine_ = nullptr;
    }
    // close will dec sync ref of storageEngine_
    DecObjRef(storageEngine_);
}

void SQLiteRelationalStore::ReleaseDBConnection(RelationalStoreConnection *connection)
{
    if (connectionCount_.load() == 1) {
        sqliteStorageEngine_->SetConnectionFlag(false);
    }

    connectMutex_.lock();
    if (connection != nullptr) {
        KillAndDecObjRef(connection);
        DecreaseConnectionCounter();
        connectMutex_.unlock();
        KillAndDecObjRef(this);
    } else {
        connectMutex_.unlock();
    }
}

void SQLiteRelationalStore::WakeUpSyncer()
{
    syncEngine_->WakeUpSyncer();
}
}
#endif