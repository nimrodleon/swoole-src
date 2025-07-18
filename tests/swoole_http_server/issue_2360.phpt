--TEST--
swoole_http_server: issue 2360 (Swoole\Http\Server silently fails to read requests)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Co\Http\Client;
use Swoole\Event;
use Swoole\Http\Request;
use Swoole\Http\Response;
use Swoole\Http\Server;

define('SOCKET_BUFFER_SIZE', 2 << mt_rand(10, 13)); // 1024 ~ 8192
phpt_echo('SOCKET_BUFFER_SIZE=' . SOCKET_BUFFER_SIZE . PHP_EOL);

$pm = new ProcessManager();
$pm->setRandomFunc(function () {
    $size = mt_rand(SOCKET_BUFFER_SIZE, SOCKET_BUFFER_SIZE << 3); // 1024 ~ 65536
    $data = '';
    for ($i = 0; $i < $size; $i++) {
        $data .= sprintf('%01x', $i % 16);
    }
    return $data;
});
$pm->initRandomDataEx(1, MAX_REQUESTS);
$pm->parentFunc = function () use ($pm) {
    go(function () use ($pm) {
        $cli = new Client('127.0.0.1', $pm->getFreePort());
        $cli->set(['socket_buffer_size' => SOCKET_BUFFER_SIZE]);
        for ($n = MAX_REQUESTS; $n--;) {
            $data = $pm->getRandomData();
            Assert::true($cli->post('/', $data));
            Assert::same($cli->statusCode, 200);
            Assert::same($cli->body, $data);
            phpt_echo('posting ' . strlen($data) . " bytes\n");
        }
        $cli->close();
    });
    Event::wait();
    $pm->kill();
    echo "DONE\n";
};
$pm->childFunc = function () use ($pm) {
    $server = new Server('127.0.0.1', $pm->getFreePort(), SWOOLE_PROCESS);
    $server->set([
        'log_file' => '/dev/null',
        'socket_buffer_size' => SOCKET_BUFFER_SIZE,
    ]);
    $server->on('workerStart', function () use ($pm) {
        $pm->wakeup();
    });
    $server->on('request', function (Request $request, Response $response) use ($pm) {
        phpt_echo("received {$request->header['content-length']} bytes\n");
        if (Assert::assert($request->rawContent() === $pm->getRandomData())) {
            $response->end($request->rawContent());
        }
    });
    $server->start();
};
$pm->childFirst();
$pm->run();
?>
--EXPECT--
DONE
