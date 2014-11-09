proxyserver [-p port] [ -u ctrl_socket_path ]

default port is 9134
a proxy that uses the same ctrl_socket_path as a running proxy
will cause that old proxy to transfer open file descriptors
to the new proxy, after which the new proxy will listen on
that path ready to be replaced itself.

sending SIGUSR1 will cause the proxy to stop listening for
new connections while continuing to serve established
connections. SIGUSR1 is not necessary when -u is used
and will not transfer established connections to another
proxy.

./testclient num_clients port num_messages

num_clients must be an even numbers. client(i) will
exchange messages with client(i + 1). 1 client is
started every 500msec. if testclient does not
print any output, no messages were lost.