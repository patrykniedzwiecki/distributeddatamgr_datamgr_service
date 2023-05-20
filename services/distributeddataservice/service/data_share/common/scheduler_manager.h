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

#ifndef SCHEDULER_MANAGER_H
#define SCHEDULER_MANAGER_H

#include <memory>

#include "db_delegate.h"
#include "task_scheduler.h"
#include "template_manager.h"

namespace OHOS::DataShare {
class SchedulerManager {
public:
    static SchedulerManager &GetInstance();
    void Execute(const std::string &uri, const std::string &rdbDir, int version);
    void Execute(const Key &key, const std::string &rdbDir, int version);
    void SetTimer(const std::string &dbPath, int version, const Key &key, int64_t reminderTime);
    void RemoveTimer(const Key &key);

private:
    static constexpr const char *REMIND_TIMER_FUNC = "remindTimer(";
    static constexpr int REMIND_TIMER_FUNC_LEN = 12;
    static constexpr size_t TIME_TASK_NUM = 10;
    SchedulerManager() = default;
    ~SchedulerManager() = default;
    void GenRemindTimerFuncParams(const std::string &rdbDir, int version, const Key &key, std::string &schedulerSQL);
    void ExecuteSchedulerSQL(const std::string &rdbDir, int version, const Key &key,
        std::shared_ptr<DBDelegate> delegate);

    std::mutex mutex_;
    std::map<Key, TaskScheduler::TaskId> timerCache_;
    std::shared_ptr<TaskScheduler> scheduler_ = nullptr;
};
} // namespace OHOS::DataShare
#endif // SCHEDULER_MANAGER_H