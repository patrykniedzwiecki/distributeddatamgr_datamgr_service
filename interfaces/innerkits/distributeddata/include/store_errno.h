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

#ifndef OHOS_DISTRIBUTED_DATA_INTERFACES_DISTRIBUTEDDATA_STORE_ERRNO_H
#define OHOS_DISTRIBUTED_DATA_INTERFACES_DISTRIBUTEDDATA_STORE_ERRNO_H
#include <errors.h>
namespace OHOS::DistributedKv {
constexpr ErrCode DISTRIBUTEDDATAMGR_ERR_OFFSET = ErrCodeOffset(SUBSYS_DISTRIBUTEDDATAMNG, 3);
enum Status : int32_t {
    SUCCESS = ERR_OK,
    ERROR = DISTRIBUTEDDATAMGR_ERR_OFFSET,
    INVALID_ARGUMENT = DISTRIBUTEDDATAMGR_ERR_OFFSET + 1,
    ILLEGAL_STATE = DISTRIBUTEDDATAMGR_ERR_OFFSET + 2,
    SERVER_UNAVAILABLE = DISTRIBUTEDDATAMGR_ERR_OFFSET + 3,
    STORE_NOT_OPEN = DISTRIBUTEDDATAMGR_ERR_OFFSET + 4,
    STORE_NOT_FOUND = DISTRIBUTEDDATAMGR_ERR_OFFSET + 5,
    STORE_ALREADY_SUBSCRIBE = DISTRIBUTEDDATAMGR_ERR_OFFSET + 6,
    STORE_NOT_SUBSCRIBE = DISTRIBUTEDDATAMGR_ERR_OFFSET + 7,
    KEY_NOT_FOUND = DISTRIBUTEDDATAMGR_ERR_OFFSET + 8,
    DB_ERROR = DISTRIBUTEDDATAMGR_ERR_OFFSET + 9,
    NETWORK_ERROR = DISTRIBUTEDDATAMGR_ERR_OFFSET + 10,
    NO_DEVICE_CONNECTED = DISTRIBUTEDDATAMGR_ERR_OFFSET + 11,
    PERMISSION_DENIED = DISTRIBUTEDDATAMGR_ERR_OFFSET + 12,
    IPC_ERROR = DISTRIBUTEDDATAMGR_ERR_OFFSET + 13,
    CRYPT_ERROR = DISTRIBUTEDDATAMGR_ERR_OFFSET + 14,
    TIME_OUT = DISTRIBUTEDDATAMGR_ERR_OFFSET + 15,
    DEVICE_NOT_FOUND = DISTRIBUTEDDATAMGR_ERR_OFFSET + 16,
    NOT_SUPPORT = DISTRIBUTEDDATAMGR_ERR_OFFSET + 17,
    SCHEMA_MISMATCH = DISTRIBUTEDDATAMGR_ERR_OFFSET + 18,
    INVALID_SCHEMA = DISTRIBUTEDDATAMGR_ERR_OFFSET + 19,
    READ_ONLY = DISTRIBUTEDDATAMGR_ERR_OFFSET + 20,
    INVALID_VALUE_FIELDS = DISTRIBUTEDDATAMGR_ERR_OFFSET + 21,
    INVALID_FIELD_TYPE = DISTRIBUTEDDATAMGR_ERR_OFFSET + 22,
    CONSTRAIN_VIOLATION = DISTRIBUTEDDATAMGR_ERR_OFFSET + 23,
    INVALID_FORMAT = DISTRIBUTEDDATAMGR_ERR_OFFSET + 24,
    INVALID_QUERY_FORMAT = DISTRIBUTEDDATAMGR_ERR_OFFSET + 25,
    INVALID_QUERY_FIELD = DISTRIBUTEDDATAMGR_ERR_OFFSET + 26,
    SYSTEM_ACCOUNT_EVENT_PROCESSING = DISTRIBUTEDDATAMGR_ERR_OFFSET + 27,
    RECOVER_SUCCESS = DISTRIBUTEDDATAMGR_ERR_OFFSET + 28,
    RECOVER_FAILED = DISTRIBUTEDDATAMGR_ERR_OFFSET + 29,
    MIGRATION_KVSTORE_FAILED = DISTRIBUTEDDATAMGR_ERR_OFFSET + 30,
    EXCEED_MAX_ACCESS_RATE = DISTRIBUTEDDATAMGR_ERR_OFFSET + 31,
    SECURITY_LEVEL_ERROR = DISTRIBUTEDDATAMGR_ERR_OFFSET + 32,
    OVER_MAX_SUBSCRIBE_LIMITS = DISTRIBUTEDDATAMGR_ERR_OFFSET + 33,
    ALREADY_CLOSED = DISTRIBUTEDDATAMGR_ERR_OFFSET + 34,
    IPC_PARCEL_ERROR = DISTRIBUTEDDATAMGR_ERR_OFFSET + 35,
};
}
#endif // OHOS_DISTRIBUTED_DATA_INTERFACES_DISTRIBUTEDDATA_STORE_ERRNO_H