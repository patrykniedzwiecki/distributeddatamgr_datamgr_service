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

#include "collection.h"

#include <algorithm>

#include "document_check.h"
#include "doc_errno.h"
#include "log_print.h"

namespace DocumentDB {
Collection::Collection(const std::string &name, KvStoreExecutor *executor) : executor_(executor)
{
    std::string lowerCaseName = name;
    std::transform(lowerCaseName.begin(), lowerCaseName.end(), lowerCaseName.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    name_ = COLL_PREFIX + lowerCaseName;
}

Collection::~Collection()
{
    executor_ = nullptr;
}

int Collection::PutDocument(const Key &key, const Value &document)
{
    if (executor_ == nullptr) {
        return -E_INVALID_ARGS;
    }
    return executor_->PutData(name_, key, document);
}

bool Collection::FindDocument()
{
    if (executor_ == nullptr) {
        return -E_INVALID_ARGS;
    }
    int errCode = 0;
    return executor_->IsCollectionExists(name_, errCode);
}

int Collection::GetDocument(const Key &key, Value &document) const
{
    if (executor_ == nullptr) {
        return -E_INVALID_ARGS;
    }
    return executor_->GetData(name_, key, document);
}

int Collection::DeleteDocument(const Key &key)
{
    if (executor_ == nullptr) {
        return -E_INVALID_ARGS;
    }
    return executor_->DelData(name_, key);
}

int Collection::UpsertDocument(const std::string &id, const std::string &document, bool isReplace)
{
    if (executor_ == nullptr) {
        return -E_INVALID_ARGS;
    }

    int errCode = E_OK;
    bool isCollExist = executor_->IsCollectionExists(name_, errCode);
    if (errCode != E_OK) {
        GLOGE("Check collection failed. %d", errCode);
        return -errCode;
    }
    if (!isCollExist) {
        GLOGE("Collection not created.");
        return -E_NO_DATA;
    }

    JsonObject upsertValue = JsonObject::Parse(document, errCode);
    if (errCode != E_OK) {
        GLOGD("Parse upsert value failed. %d", errCode);
        return errCode;
    }

    Key keyId(id.begin(), id.end());
    Value valSet(document.begin(), document.end());

    if (!isReplace) {
        Value valueGot;
        errCode = executor_->GetData(name_, keyId, valueGot);
        std::string valueGotStr = std::string(valueGot.begin(), valueGot.end());

        if (errCode != E_OK && errCode != -E_NOT_FOUND) {
            GLOGE("Get original document failed. %d", errCode);
            return errCode;
        } else if (errCode == E_OK) { // document has been inserted
            GLOGD("Document has been inserted, append value.");
            JsonObject originValue = JsonObject::Parse(valueGotStr, errCode);
            if (errCode != E_OK) {
                GLOGD("Parse original value failed. %d %s", errCode, valueGotStr.c_str());
                return errCode;
            }

            errCode = JsonCommon::Append(originValue, upsertValue);
            if (errCode != E_OK) {
                GLOGD("Append value failed. %d", errCode);
                return errCode;
            }

            std::string valStr = originValue.Print();
            valSet = {valStr.begin(), valStr.end()};
        }
    }

    return executor_->PutData(name_, keyId, valSet);
}

int Collection::UpdateDocument(const std::string &id, const std::string &update)
{
    if (executor_ == nullptr) {
        return -E_INVALID_ARGS;
    }

    int errCode = E_OK;
    bool isCollExist = executor_->IsCollectionExists(name_, errCode);
    if (errCode != E_OK) {
        GLOGE("Check collection failed. %d", errCode);
        return -errCode;
    }
    if (!isCollExist) {
        GLOGE("Collection not created.");
        return -E_NO_DATA;
    }

    JsonObject updateValue = JsonObject::Parse(update, errCode);
    if (errCode != E_OK) {
        GLOGD("Parse upsert value failed. %d", errCode);
        return errCode;
    }

    Key keyId(id.begin(), id.end());
    Value valueGot;
    errCode = executor_->GetData(name_, keyId, valueGot);
    std::string valueGotStr = std::string(valueGot.begin(), valueGot.end());
    if (errCode != E_OK) {
        GLOGE("Get original document failed. %d", errCode);
        return errCode;
    }

    GLOGD("Update document value.");
    JsonObject originValue = JsonObject::Parse(valueGotStr, errCode);
    if (errCode != E_OK) {
        GLOGD("Parse original value failed. %d %s", errCode, valueGotStr.c_str());
        return errCode;
    }

    errCode = JsonCommon::Append(originValue, updateValue);
    if (errCode != E_OK) {
        GLOGD("Append value failed. %d", errCode);
        return errCode;
    }

    std::string valStr = originValue.Print();
    Value valSet(valStr.begin(), valStr.end());
    return executor_->PutData(name_, keyId, valSet);
}
} // namespace DocumentDB
