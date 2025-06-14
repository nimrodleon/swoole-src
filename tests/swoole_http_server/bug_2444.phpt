--TEST--
swoole_http_server: bug #2444
--SKIPIF--
<?php require __DIR__ . '/../include/skipif.inc';
skip_if_no_database();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
$pm = new ProcessManager;
$pm->parentFunc = function () use ($pm) {
    go(function () use ($pm) {
        echo httpGetBody("http://127.0.0.1:{$pm->getFreePort()}/test") . PHP_EOL;
        $pm->kill();
    });
};
$pm->childFunc = function () use ($pm) {
    $server = new Swoole\Http\Server('0.0.0.0', $pm->getFreePort(), SWOOLE_BASE);
    $server->set(['log_file' => '/dev/null', 'hook_flags' => SWOOLE_HOOK_ALL]);
    $server->on('start', function () use ($pm) {
        $pm->wakeup();
    });
    $server->on('request', function (Swoole\Http\Request $request, Swoole\Http\Response $response) use ($pm) {
        switch ($request->server['request_uri']) {
            case '/test':
                $cli = new Swoole\Coroutine\Http\Client('127.0.0.1', $pm->getFreePort());
                $cli->get('/');
                if (!Assert::assert($cli->statusCode === 200)) {
                    _error:
                    $response->status(500);
                    $response->end('ERROR');
                    return;
                }
                $cli->close();
                $db = new mysqli();
                $db->set_opt(MYSQLI_OPT_INT_AND_FLOAT_NATIVE, 1);
                if (!Assert::assert($db->connect(MYSQL_SERVER_HOST,
                    MYSQL_SERVER_USER,
                    MYSQL_SERVER_PWD,
                    MYSQL_SERVER_DB,
                    MYSQL_SERVER_PORT))) {
                    goto _error;
                }
                if (!Assert::assert($db->query('select 1')->fetch_all()[0][0] === 1)) {
                    goto _error;
                }
                $db->close();
                break;
        }
        $response->end('OK');
    });
    $server->start();
};
$pm->childFirst();
$pm->run();
?>
--EXPECT--
OK
