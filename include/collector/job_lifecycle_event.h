// job_lifecycle_event.h
#pragma once
#include <functional>
#include "collector/collector_type.h"

struct Job;

enum class JobEvent {
    Added,
    Removed,
    Updated   // 预留，方便以后做属性热更新
};

// 回调签名：事件类型 + Job 常量引用
using JobLifecycleCb = std::function<void(JobEvent, const Job&)>;