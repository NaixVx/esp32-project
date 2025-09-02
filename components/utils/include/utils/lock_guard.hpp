#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Simple RAII wrapper for FreeRTOS semaphore mutex
class LockGuard {
   public:
    explicit LockGuard(SemaphoreHandle_t mutex) : mutex_(mutex) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
    ~LockGuard() {
        xSemaphoreGive(mutex_);
    }

    // Prevent copying
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

    // Allow moving if needed (optional)
    LockGuard(LockGuard&& other) noexcept : mutex_(other.mutex_) {
        other.mutex_ = nullptr;
    }
    LockGuard& operator=(LockGuard&& other) noexcept {
        if (this != &other) {
            mutex_ = other.mutex_;
            other.mutex_ = nullptr;
        }
        return *this;
    }

   private:
    SemaphoreHandle_t mutex_;
};
