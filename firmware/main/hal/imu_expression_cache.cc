#include "imu_expression_cache.h"
#include <mutex>
namespace imu_expression {
namespace{std::mutex cache_mutex;Model model;}
void PublishOwnerReading(const Reading& reading){std::lock_guard<std::mutex> lock(cache_mutex);model.Push(reading);}
void PublishOwnerUnavailable(Availability status){std::lock_guard<std::mutex> lock(cache_mutex);model.Unavailable(status);}
Report ReadCached(uint64_t now){std::lock_guard<std::mutex> lock(cache_mutex);return model.Read(now);}
}
