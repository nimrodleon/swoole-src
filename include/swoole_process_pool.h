/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <rango@swoole.com>                             |
  |         Twosee  <twose@qq.com>                                       |
  +----------------------------------------------------------------------+
*/

#pragma once

#include "swoole.h"

#include <csignal>
#include <unordered_map>
#include <unordered_set>
#include <queue>

#include "swoole_signal.h"
#include "swoole_lock.h"
#include "swoole_pipe.h"
#include "swoole_channel.h"
#include "swoole_msg_queue.h"
#include "swoole_message_bus.h"

enum swWorkerStatus {
    SW_WORKER_BUSY = 1,
    SW_WORKER_IDLE = 2,
    SW_WORKER_EXIT = 3,
};

enum swWorkerType {
    SW_MASTER = 1,
    SW_WORKER = 2,
    SW_MANAGER = 3,
    SW_EVENT_WORKER = 2,
    SW_TASK_WORKER = 4,
    SW_USER_WORKER = 5,
};

enum swIPCMode {
    SW_IPC_NONE = 0,
    SW_IPC_UNIXSOCK = 1,
    SW_IPC_MSGQUEUE = 2,
    SW_IPC_SOCKET = 3,
};

SW_API swoole::WorkerId swoole_get_worker_id();
SW_API pid_t swoole_get_worker_pid();
SW_API int swoole_get_worker_type();
SW_API void swoole_set_worker_pid(pid_t pid);
SW_API void swoole_set_worker_id(swoole::WorkerId worker_id);
SW_API void swoole_set_worker_type(int type);
SW_API char swoole_get_worker_symbol();

namespace swoole {
enum WorkerMessageType {
    SW_WORKER_MESSAGE_STOP = 1,
};

enum ProtocolType {
    SW_PROTOCOL_TASK = 1,
    SW_PROTOCOL_STREAM,
    SW_PROTOCOL_MESSAGE,
};

struct WorkerStopMessage {
    pid_t pid;
    uint16_t worker_id;
};

class ExitStatus {
    pid_t pid_;
    int status_;

  public:
    ExitStatus(pid_t _pid, int _status) : pid_(_pid), status_(_status) {}

    pid_t get_pid() const {
        return pid_;
    }

    int get_status() const {
        return status_;
    }

    int get_code() const {
        return WEXITSTATUS(status_);
    }

    int get_signal() const {
        return WTERMSIG(status_);
    }

    bool is_normal_exit() const {
        return WIFEXITED(status_);
    }
};

static inline ExitStatus wait_process() {
    int status = 0;
    pid_t pid = ::wait(&status);
    return {pid, status};
}

static inline ExitStatus wait_process(pid_t _pid, int options) {
    int status = 0;
    pid_t pid = ::waitpid(_pid, &status, options);
    return {pid, status};
}

struct ProcessPool;
struct Worker;

struct WorkerGlobal {
    WorkerId id;
    uint8_t type;
    pid_t pid;
    bool shutdown;
    bool running;
    uint32_t max_request;
    /**
     * worker is shared memory, visible in other work processes.
     * When a worker process restarts, it may be held by both the old and new processes simultaneously,
     * necessitating careful handling of the state.
     */
    Worker *worker;
    /**
     *  worker_copy is a copy of worker,
     *  but it must be local memory and only used within the current process or thread.
     *  It is not visible to other worker processes.
     */
    Worker *worker_copy;
    time_t exit_time;
};

struct Worker {
    pid_t pid;
    WorkerId id;
    ProcessPool *pool;
    MsgQueue *queue;
    bool shared;

    bool redirect_stdout;
    bool redirect_stdin;
    bool redirect_stderr;

    /**
     * worker status, IDLE or BUSY
     */
    uint8_t status;
    uint8_t type;
    uint8_t msgqueue_mode;
    uint8_t child_process;

    uint32_t concurrency;
    time_t start_time;

    sw_atomic_long_t dispatch_count;
    sw_atomic_long_t request_count;
    sw_atomic_long_t response_count;
    size_t coroutine_num;

    Mutex *lock;
    UnixSocket *pipe_object;

    network::Socket *pipe_master;
    network::Socket *pipe_worker;
    network::Socket *pipe_current;

    void *ptr;

    ssize_t send_pipe_message(const void *buf, size_t n, int flags) const;
    bool has_exceeded_max_request() const;
    void set_max_request(uint32_t max_request, uint32_t max_request_grace);
    static void set_isolation(const std::string &group_, const std::string &user_, const std::string &chroot_);
    void report_error(const ExitStatus &exit_status);
    /**
     * Init global state for worker process.
     * Must be called after the process is spawned and before the main loop is executed.
     */
    void init();
    void shutdown();
    bool is_shutdown();
    static bool is_running();

    void set_status(swWorkerStatus _status) {
        status = _status;
    }

    void set_status_to_idle() {
        set_status(SW_WORKER_IDLE);
    }

    void set_status_to_busy() {
        set_status(SW_WORKER_BUSY);
    }

    void add_request_count() {
        request_count++;
    }

    bool is_busy() const {
        return status == SW_WORKER_BUSY;
    }

    bool is_idle() const {
        return status == SW_WORKER_IDLE;
    }
};

struct StreamInfo {
    network::Socket *socket;
    network::Socket *last_connection;
    char *socket_file;
    int socket_port;
    String *response_buffer;
};

struct ReloadTask {
    std::unordered_map<pid_t, Worker *> workers;
    std::queue<pid_t> kill_queue;
    TimerNode *timer = nullptr;

    size_t count() const {
        return workers.size();
    }

    bool is_completed() const {
        return workers.empty();
    }

    bool exists(pid_t pid) {
        return workers.find(pid) != workers.end();
    }

    ReloadTask() = default;
    ~ReloadTask();
    void kill_one(int signal_number = SIGTERM);
    void kill_all(int signal_number = SIGKILL);
    void add_workers(Worker *list, size_t n);
    void add_timeout_killer(int timeout);
    bool remove(pid_t pid);
};

struct ProcessPool {
    bool running;
    bool reload_init;
    bool read_message;
    bool started;
    bool schedule_by_sysvmsg;
    bool async;

    uint8_t ipc_mode;
    ProtocolType protocol_type_;
    pid_t master_pid;
    uint32_t max_wait_time;
    uint64_t reload_count;
    time_t reload_last_time;

    /**
     * process type
     */
    uint8_t type;

    /**
     * worker->id = start_id + i
     */
    uint16_t start_id;

    /**
     * use message queue IPC
     */
    uint8_t use_msgqueue;

    /**
     * use stream socket IPC
     */
    uint8_t use_socket;

    char *packet_buffer;
    uint32_t max_packet_size_;

    /**
     * message queue key
     */
    key_t msgqueue_key;

    uint32_t worker_num;
    uint32_t max_request;
    uint32_t max_request_grace;

    /**
     * No idle task work process is available.
     */
    uint8_t scheduler_warning;
    time_t warning_time;

    void (*onStart)(ProcessPool *pool);
    void (*onShutdown)(ProcessPool *pool);
    void (*onBeforeReload)(ProcessPool *pool);
    void (*onAfterReload)(ProcessPool *pool);
    int (*onTask)(ProcessPool *pool, Worker *worker, EventData *task);
    void (*onWorkerStart)(ProcessPool *pool, Worker *worker);
    void (*onMessage)(ProcessPool *pool, RecvData *msg);
    void (*onWorkerExit)(ProcessPool *pool, Worker *worker);
    void (*onWorkerStop)(ProcessPool *pool, Worker *worker);
    void (*onWorkerError)(ProcessPool *pool, Worker *worker, const ExitStatus &exit_status);
    void (*onWorkerMessage)(ProcessPool *pool, EventData *msg);
    int (*onWorkerNotFound)(ProcessPool *pool, const ExitStatus &exit_status);
    int (*main_loop)(ProcessPool *pool, Worker *worker);

    sw_atomic_t round_id;

    Worker *workers;
    std::vector<std::shared_ptr<UnixSocket>> *pipes;
    std::unordered_map<pid_t, Worker *> *map_;
    MsgQueue *queue;
    StreamInfo *stream_info_;
    Channel *message_box = nullptr;
    MessageBus *message_bus = nullptr;
    ReloadTask *reload_task = nullptr;

    void *ptr;

    Worker *get_worker(int worker_id) const {
        return &(workers[worker_id - start_id]);
    }

    static TaskId get_task_id(const EventData *task) {
        return task->info.fd;
    }

    static WorkerId get_task_src_worker_id(const EventData *task) {
        return task->info.reactor_id;
    }

    void set_max_packet_size(uint32_t _max_packet_size) {
        max_packet_size_ = _max_packet_size;
    }

    bool is_master() const {
        return swoole_get_worker_type() == SW_MASTER;
    }

    bool is_worker() const {
        return swoole_get_worker_type() == SW_WORKER;
    }

    /**
     * SW_PROTOCOL_TASK
     * ==================================================================
     * The `EventData` structure must be sent as a single message and cannot be split into multiple transmissions.
     * If the length of the message content exceeds the size limit of the data field in EventData,
     * it should be written to a temporary file.
     * In this case, set the SW_TASK_TMPFILE flag in info.ext_flags.
     * Only the path to the temporary file will be transmitted,
     * and the receiving end should retrieve the actual message content from this temporary file.
     * Reference: Server::task_pack()
     *
     * SW_PROTOCOL_MESSAGE
     * ==================================================================
     * When sending the `EventData` structure, the message can be split into multiple transmissions.
     * When sending data in multiple parts, you must set a unique info.msg_id.
     * For the first slice, set the info.flags with the SW_EVENT_DATA_CHUNK | SW_EVENT_DATA_BEGIN flag,
     * and for the last slice, set the info.flags with the SW_EVENT_DATA_CHUNK | SW_EVENT_DATA_END flag.
     * The receiving end will place the data into a memory cache table, merge the data,
     * and only execute the onMessage callback once the complete message has been received.
     *
     * Reference: MessageBus::write() and MessageBus::read()
     *
     * SW_PROTOCOL_STREAM
     * ==================================================================
     *  +-------------------------------+-------------------------------+
     *  | Payload Length     ( 4 byte, network byte order)              |
     *  | Payload Data ...   ( Payload Length byte )                    |
     *  +-------------------------------- - - - - - - - - - - - - - - - +
     *
     *  The packet consists of a 4 byte length header followed by the data payload.
     *  The receiving end should first use `socket.recv(&payload_len, 4)` to obtain the length of the data payload.
     *  Then, execute `socket.recv(payload, payload_len)` to receive the complete data.
     *  Please note that sufficient memory space must be allocated for the payload,
     *  for example, `payload = malloc(payload_len)`.
     */
    void set_protocol(ProtocolType _protocol_type);
    void set_type(int _type);
    void set_start_id(int _start_id);
    void set_max_request(uint32_t _max_request, uint32_t _max_request_grace);
    bool detach();
    int wait();
    int start_check();
    int start();
    bool shutdown();
    bool reload();
    void reopen_logger();

    void rigger_read_message_event() {
        read_message = true;
    }

    pid_t spawn(Worker *worker);
    void stop(Worker *worker);
    void kill_all_workers(int signo = SIGKILL);
    swResultCode dispatch(EventData *data, int *worker_id);
    int response(const char *data, uint32_t length) const;
    swResultCode dispatch_sync(EventData *data, int *dst_worker_id);
    swResultCode dispatch_sync(const char *data, uint32_t len);
    void add_worker(Worker *worker) const;
    bool del_worker(const Worker *worker) const;
    Worker *get_worker_by_pid(pid_t pid) const;
    void destroy();
    int create(uint32_t worker_num, key_t msgqueue_key = 0, swIPCMode ipc_mode = SW_IPC_NONE);
    int create_message_box(size_t memory_size);
    int create_message_bus();
    int push_message(uint8_t type, const void *data, size_t length) const;
    int push_message(const EventData *msg) const;
    bool send_message(WorkerId worker_id, const char *message, size_t l_message) const;
    int pop_message(void *data, size_t size);
    int listen(const char *socket_file, int backlog) const;
    int listen(const char *host, int port, int backlog) const;
    int schedule();
    bool is_worker_running(Worker *worker);

  private:
    static int recv_packet(Reactor *reactor, Event *event);
    static int recv_message(Reactor *reactor, Event *event);
    static int run_with_task_protocol(ProcessPool *pool, Worker *worker);
    static int run_with_stream_protocol(ProcessPool *pool, Worker *worker);
    static int run_with_message_protocol(ProcessPool *pool, Worker *worker);
    static int run_async(ProcessPool *pool, Worker *worker);
    void at_worker_enter(Worker *worker);
    void at_worker_exit(Worker *worker);

    bool wait_detached_worker(std::unordered_set<pid_t> &detached_workers, pid_t pid);
};
};  // namespace swoole

static sw_inline int swoole_kill(pid_t _pid, int _sig) {
    return kill(_pid, _sig);
}

typedef swoole::ProtocolType swProtocolType;

extern SW_THREAD_LOCAL swoole::WorkerGlobal SwooleWG;

static inline swoole::Worker *sw_worker() {
    return SwooleWG.worker;
}
