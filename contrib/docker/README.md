Copied from here: 
   https://github.com/vergecurrency/Docker-Verge-Daemon

To build:
---
    docker build --rm -t verge .


Before running:
---
Place VERGE.conf in /tmp/verge/.VERGE/VERGE.conf on the HOST system. You probably want something a directory other than /tmp,
I am just using it to test that everything works. It will need to have at least these two values (like this):

    rpcuser=bitcoinrpc
    rpcpassword=Bnec4eGig52MTEAkzk4uMjsJechM7rA9EQ4NzkDHLwpG


Test that the docker runs:
---
    mkdir -p tmpverge/.VERGE
    echo "rpcuser=bitcoinrpc" > tmpverge/.VERGE/VERGE.conf
    echo "rpcpassword=Bnec4eGig52MTEAkzk4uMjsJechM7rA9EQ4NzkDHLwpG" >> tmpverge/.VERGE/VERGE.conf


To run:
---
    docker run -d --name verge -v `pwd`/tmpverge:/coin/home -p 20102:20102 -p 21102:21102 verge

This command should return a container id. You can use this id (or the first few characters of it to refer to later. We added a name option above so we can just refer to it as 'verge'.)

To watch the debug log:
---
    tail -f tmpverge/.VERGE/debug.log

To connect to the running container
---
    docker exec -it verge bash

To kill the running container:
---
    docker kill verge

To remove the stopped container:
---
    docker rm verge

