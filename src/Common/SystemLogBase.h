#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>
#include <base/types.h>

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>
#include <Storages/IStorage_fwd.h>
#include <Common/ThreadPool_fwd.h>

#define SYSTEM_LOG_ELEMENTS(M) \
    M(AsynchronousMetricLogElement) \
    M(CrashLogElement) \
    M(MetricLogElement) \
    M(OpenTelemetrySpanLogElement) \
    M(PartLogElement) \
    M(QueryLogElement) \
    M(QueryThreadLogElement) \
    M(QueryViewsLogElement) \
    M(SessionLogElement) \
    M(TraceLogElement) \
    M(TransactionsInfoLogElement) \
    M(ZooKeeperLogElement) \
    M(ProcessorProfileLogElement) \
    M(TextLogElement) \
    M(FilesystemCacheLogElement) \
    M(FilesystemReadPrefetchesLogElement) \
    M(AsynchronousInsertLogElement)

namespace Poco
{
class Logger;
namespace Util
{
    class AbstractConfiguration;
}
}

namespace DB
{

struct StorageID;

class ISystemLog
{
public:
    virtual String getName() const = 0;

    //// force -- force table creation (used for SYSTEM FLUSH LOGS)
    virtual void flush(bool force = false) = 0; /// NOLINT
    virtual void prepareTable() = 0;

    /// Start the background thread.
    virtual void startup() = 0;

    /// Stop the background flush thread before destructor. No more data will be written.
    virtual void shutdown() = 0;

    virtual void stopFlushThread() = 0;

    virtual ~ISystemLog();

    virtual void savingThreadFunction() = 0;

protected:
    std::unique_ptr<ThreadFromGlobalPool> saving_thread;
};

template <typename LogElement>
class SystemLogQueue
{
public:
    SystemLogQueue(const String & name_);

    void add(const LogElement & element);
    size_t size() const { return queue.size(); }
    //void push_back(const LogElement & element) { queue.push_back(element); }
    void shutdown() { is_shutdown = true; }

    // Queue is bounded. But its size is quite large to not block in all normal cases.
    std::vector<LogElement> queue;
    // An always-incrementing index of the first message currently in the queue.
    // We use it to give a global sequential index to every message, so that we
    // can wait until a particular message is flushed. This is used to implement
    // synchronous log flushing for SYSTEM FLUSH LOGS.
    uint64_t queue_front_index = 0;

    /// Data shared between callers of add()/flush()/shutdown(), and the saving thread
    std::mutex mutex;
    std::condition_variable flush_event;

    // Requested to flush logs up to this index, exclusive
    uint64_t requested_flush_up_to = 0;

    // Logged overflow message at this queue front index
    uint64_t logged_queue_full_at_index = -1;

private:
    Poco::Logger * log;
    bool is_shutdown = false;
};

template <typename LogElement>
class SystemLogBase : public ISystemLog
{
public:
    using Self = SystemLogBase;

    SystemLogBase(
        const String & name_,
        std::shared_ptr<SystemLogQueue<LogElement>> queue_ = nullptr);

    /** Append a record into log.
      * Writing to table will be done asynchronously and in case of failure, record could be lost.
      */
    void add(const LogElement & element);

    /// Flush data in the buffer to disk. Block the thread until the data is stored on disk.
    void flush(bool force) override;

    void startup() override;

     void stopFlushThread() override;

    /// Non-blocking flush data in the buffer to disk.
    void notifyFlush(bool force);

    String getName() const override { return LogElement::name(); }

    static const char * getDefaultOrderBy() { return "event_date, event_time"; }

protected:
    Poco::Logger * log;

    std::shared_ptr<SystemLogQueue<LogElement>> queue;

    // A flag that says we must create the tables even if the queue is empty.
    bool is_force_prepare_tables = false;

    // Flushed log up to this index, exclusive
    uint64_t flushed_up_to = 0;

    bool is_shutdown = false;

    // Logged overflow message at this queue front index
    uint64_t logged_queue_full_at_index = -1;

private:
    uint64_t notifyFlushImpl(bool force);


};

}
