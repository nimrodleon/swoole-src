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
  +----------------------------------------------------------------------+
*/

#ifndef SW_C_API_H_
#define SW_C_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "swoole_config.h"

enum swGlobalHookType {
    SW_GLOBAL_HOOK_BEFORE_SERVER_START,
    SW_GLOBAL_HOOK_BEFORE_CLIENT_START,
    SW_GLOBAL_HOOK_BEFORE_WORKER_START,
    SW_GLOBAL_HOOK_ON_CORO_START,
    SW_GLOBAL_HOOK_ON_CORO_STOP,
    SW_GLOBAL_HOOK_ON_REACTOR_CREATE,
    SW_GLOBAL_HOOK_BEFORE_SERVER_SHUTDOWN,
    SW_GLOBAL_HOOK_AFTER_SERVER_SHUTDOWN,
    SW_GLOBAL_HOOK_BEFORE_WORKER_STOP,
    SW_GLOBAL_HOOK_ON_REACTOR_DESTROY,
    SW_GLOBAL_HOOK_BEFORE_SERVER_CREATE,
    SW_GLOBAL_HOOK_AFTER_SERVER_CREATE,
    SW_GLOBAL_HOOK_AFTER_FORK,
    SW_GLOBAL_HOOK_USER = 24,
    SW_GLOBAL_HOOK_END = SW_MAX_HOOK_TYPE - 1,
};

typedef void (*swHookFunc)(void *data);

int swoole_add_hook(swGlobalHookType type, swHookFunc cb, int push_back);
void swoole_call_hook(swGlobalHookType type, void *arg);
bool swoole_isset_hook(swGlobalHookType type);

const char *swoole_version(void);
int swoole_version_id(void);
int swoole_api_version_id(void);

#ifdef __cplusplus
} /* end extern "C" */
#endif
#endif
