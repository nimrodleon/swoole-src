--TEST--
swoole_curl/basic: Test curl_error() & curl_errno() function without url
--CREDITS--
TestFest 2009 - AFUP - Perrick Penet <perrick@noparking.net>
--SKIPIF--
<?php require __DIR__ . '/../../include/skipif.inc'; ?>
<?php if (!extension_loaded("curl")) print "skip"; ?>
--FILE--
<?php

//In January 2008 , level 7.18.0 of the curl lib, many of the messages changed.
//The final crlf was removed. This test is coded to work with or without the crlf.

require __DIR__ . '/../../include/bootstrap.php';

$cm = new \SwooleTest\CurlManager();
$cm->run(function ($host) {

    $ch = curl_init();

    curl_exec($ch);
    Assert::contains(curl_error($ch), 'No URL set');
    Assert::eq(curl_errno($ch), CURLE_URL_MALFORMAT);
    curl_close($ch);

    echo "Done\n";
}, false);

?>
--EXPECTF--
Done
