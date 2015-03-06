if echo AAA; then echo BBB; fi
if false then echo BBB fi
if echo AAA then echo BBB else echo CCC fi
if false then echo BBB else echo CCC fi
if echo AAA; then echo BBB; fi > 1.txt
if false then echo BBB fi > 2.txt
if false then BBB fi > 3.txt
if echo AAA then echo BBB else echo CCC fi > 4.txt
if false then echo BBB else echo CCC fi > 5.txt
(echo AAA) > 6.txt
echo AAA; echo BBB
echo simple > 7.txt
echo OK;
if echo AAA then (echo BBB; echo CCC); fi; echo DDD
