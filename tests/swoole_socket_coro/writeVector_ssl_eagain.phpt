--TEST--
swoole_socket_coro: writeVector_ssl eagain
--SKIPIF--
<?php require __DIR__ . '/../include/skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swoole\Coroutine;
use Swoole\Coroutine\Socket;

use function Swoole\Coroutine\run;

$totalLength = 0;
$iovector = [];
$packedStr = '';

for ($i = 0; $i < 10; $i++) {
    $iovector[$i] = str_repeat(get_safe_random(1024), 128);
    $totalLength += strlen($iovector[$i]);
    $packedStr .= $iovector[$i];
}
$totalLength2 = rand(strlen($packedStr) / 2, strlen($packedStr) - 1024 * 128);

$pm = new ProcessManager();
$pm->parentFunc = function ($pid) use ($pm) {
    run(function () use ($pm) {
        global $totalLength, $iovector;
        $conn = new Socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        $conn->setProtocol([
            'open_ssl' => true,
        ]);
        $conn->connect('127.0.0.1', $pm->getFreePort());

        $ret = $conn->writeVectorAll($iovector);
        Assert::eq($ret, $totalLength);
        $conn->recv();
        echo "DONE\n";
    });
};

$pm->childFunc = function () use ($pm) {
    run(function () use ($pm) {
        global $totalLength, $packedStr;
        $socket = new Socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        $socket->setProtocol([
            'open_ssl' => true,
            'ssl_cert_file' => SSL_FILE_DIR . '/server.crt',
            'ssl_key_file' => SSL_FILE_DIR . '/server.key',
        ]);
        Assert::assert($socket->bind('127.0.0.1', $pm->getFreePort()));
        Assert::assert($socket->listen(MAX_CONCURRENCY));

        $pm->wakeup();

        /** @var Socket */
        $conn = $socket->accept();
        Assert::assert($conn, 'error: ' . swoole_last_error());
        $conn->sslHandshake();

        Coroutine::sleep(0.5);

        Assert::eq($conn->recvAll($totalLength), $packedStr, -1);
        $conn->send('close');
    });
};
$pm->childFirst();
$pm->run();
?>
--EXPECT--
DONE
